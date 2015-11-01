////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2014
//
// Modular Framework for OpenGLES2 rendering on multiple platforms.
//
// invaderer example: simple game with sprites and sounds
//
// Level: 1
//
// Demonstrates:
//   Basic framework app
//   Shaders
//   Basic Matrices
//   Simple game mechanics
//   Texture loaded from GIF file
//   Audio
//

#include <cstdlib>
#include "ctime"
#include <iostream>
#include <fstream>
#include <vector>


using namespace std;

namespace octet {
	class sprite {
		// where is our sprite (overkill for a 2D game!)
		mat4t modelToWorld;

		// half the width of the sprite
		float halfWidth;

		// half the height of the sprite
		float halfHeight;

		// what texture is on our sprite
		int texture;

		// true if this sprite is enabled.
		bool enabled;
	public:
		sprite() {
			texture = 0;
			enabled = true;
		}

		void init(int _texture, float x, float y, float w, float h) {				//Size of template
			modelToWorld.loadIdentity();
			modelToWorld.translate(x, y, 0);
			halfWidth = w * 0.5f;
			halfHeight = h * 0.5f;
			texture = _texture;
			enabled = true;
		}

		void render(texture_shader &shader, mat4t &cameraToWorld) {
			// invisible sprite... used for gameplay.
			if (!texture) return;

			// build a projection matrix: model -> world -> camera -> projection
			// the projection space is the cube -1 <= x/w, y/w, z/w <= 1
			mat4t modelToProjection = mat4t::build_projection_matrix(modelToWorld, cameraToWorld);

			// set up opengl to draw textured triangles using sampler 0 (GL_TEXTURE0)
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);
			shader.render(modelToProjection, 0);

			// this is an array of the positions of the corners of the sprite in 3D
			// a straight "float" here means this array is being generated here at runtime.
			float vertices[] = {
				-halfWidth, -halfHeight, 0,
				halfWidth, -halfHeight, 0,
				halfWidth,  halfHeight, 0,
				-halfWidth,  halfHeight, 0,
			};

			// attribute_pos (=0) is position of each corner
			// each corner has 3 floats (x, y, z)
			// there is no gap between the 3 floats and hence the stride is 3*sizeof(float)
			glVertexAttribPointer(attribute_pos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)vertices);
			glEnableVertexAttribArray(attribute_pos);

			// this is an array of the positions of the corners of the texture in 2D
			static const float uvs[] = {
				0,  0,
				1,  0,
				1,  1,
				0,  1,
			};

			// attribute_uv is position in the texture of each corner
			// each corner (vertex) has 2 floats (x, y)
			// there is no gap between the 2 floats and hence the stride is 2*sizeof(float)
			glVertexAttribPointer(attribute_uv, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)uvs);
			glEnableVertexAttribArray(attribute_uv);

			// finally, draw the sprite (4 vertices)
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}

		// move the object
		void translate(float x, float y) {
			modelToWorld.translate(x, y, 0);
		}

		// position the object relative to another.
		void set_relative(sprite &rhs, float x, float y) {
			modelToWorld = rhs.modelToWorld;
			modelToWorld.translate(x, y, 0);
		}

		// return true if this sprite collides with another.
		// note the "const"s which say we do not modify either sprite
		bool collides_with(const sprite &rhs) const {
			float dx = rhs.modelToWorld[3][0] - modelToWorld[3][0];
			float dy = rhs.modelToWorld[3][1] - modelToWorld[3][1];

			// both distances have to be under the sum of the halfwidths
			// for a collision
			return
				(fabsf(dx) < halfWidth + rhs.halfWidth) &&
				(fabsf(dy) < halfHeight + rhs.halfHeight)
				;
		}

		bool is_above(const sprite &rhs, float margin) const {
			float dx = rhs.modelToWorld[3][0] - modelToWorld[3][0];

			return
				(fabsf(dx) < halfWidth + margin)
				;
		}

		bool &is_enabled() {
			return enabled;
		}
	};



	class invaderers_app : public octet::app {
		// Matrix to transform points in our camera space to the world.
		// This lets us move our camera
		mat4t cameraToWorld;
		// shader to draw a textured triangle
		texture_shader texture_shader_;

		enum {
			num_sound_sources = 8,
			num_rows = 7,
			num_cols = 13,
			num_missiles = 2,
			num_bombs = 7,
			num_borders = 4,
			num_invaderers = num_rows * num_cols,

			// sprite definitions
			ship_sprite = 0,
			game_over_sprite,
			restart_sprite,
			success_sprite,
			nextWave_sprite,
			holyBanana_sprite,

			first_invaderer_sprite,
			last_invaderer_sprite = first_invaderer_sprite + num_invaderers - 1,

			first_missile_sprite,
			last_missile_sprite = first_missile_sprite + num_missiles - 1,

			first_bomb_sprite,
			last_bomb_sprite = first_bomb_sprite + num_bombs - 1,

			first_border_sprite,
			last_border_sprite = first_border_sprite + num_borders - 1,

			num_sprites,
		};


		// timers for missiles and bombs
		int missiles_disabled;
		int bombs_disabled;

		// accounting for bad guys
		int live_invaderers;
		int num_lives;

		// game state
		bool game_over;
		bool loss;
		bool reWave;   
		int score;

		bool readFile = true;

		// speed of enemy
		float invader_velocity;

		// sounds
		ALuint whoosh;
		ALuint bang;
		ALuint sandbox;
		unsigned cur_source;
		ALuint sources[num_sound_sources];

		// big array of sprites
		sprite sprites[num_sprites];

		// random number generator
		class random randomizer;

		// a texture for our text
		GLuint font_texture;

		// information for our text
		bitmap_font font;

		ALuint get_sound_source() { return sources[cur_source++ % num_sound_sources]; }

		// called when we hit an enemy
		void on_hit_invaderer() {
			ALuint source = get_sound_source();
			alSourcei(source, AL_BUFFER, bang);
			alSourcePlay(source);

			live_invaderers--;
			score++;
			if (live_invaderers == 0) {										// win sprite will appear if all enemies have died and user will be prompted with message to continue playing
				game_over = true;
				sprites[success_sprite].translate(-20, 0);
				sprites[nextWave_sprite].translate(-18.3, -2.2);

			}
		}

		// called when we are hit
		void on_hit_ship() {
			ALuint source = get_sound_source();
			alSourcei(source, AL_BUFFER, bang);
			alSourcePlay(source);	
			
			if (--num_lives == 0) {											// lose sprite will appear if player has died and user will be prompted with message to replay
				game_over = true;
				loss = true;
				sprites[game_over_sprite].translate(-20, 0.2);
				sprites[restart_sprite].translate(-18.3, -2.2);
			}
		}


		// use the keyboard to move the ship
		void move_ship() {
			const float ship_speed = 0.05f;

			if (is_key_down(key_left)) {
				sprites[ship_sprite].translate(-ship_speed, 0);
				if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 2])) {
					sprites[ship_sprite].translate(+ship_speed, 0);
				}
			}  if (is_key_down(key_right)) {
				sprites[ship_sprite].translate(+ship_speed, 0);
				if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 3])) {
					sprites[ship_sprite].translate(-ship_speed, 0);
				}
			}
			if (is_key_down(key_up)) {
				sprites[ship_sprite].translate(0, +ship_speed);
				if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 2])) {
					sprites[ship_sprite].translate(0, -ship_speed);
				}

			}
			if (is_key_down(key_down)) {
				
				sprites[ship_sprite].translate(0, -ship_speed);
				if (sprites[ship_sprite].collides_with(sprites[first_border_sprite + 3])) {
					sprites[ship_sprite].translate(0, +ship_speed);

				}
			}
		}

		void initEnemies() {														// enemy initialization

			string enemy = "assets/invaderers/invaderer.gif";
			GLuint invaderer = resource_dict::get_texture_handle(GL_RGBA, enemy);

			if (reWave == true) {													//will be initialized if the user wins the game, in order for enemies to respawn in the next wave
				reWave = false;														// we set to false so only one wave comes at a time unless we say otherwise

				for (int j = 0; j != 6; ++j) {								//		DETERMINES NUMBER OF ROWS AND COLUMNS
					for (int i = 0; i != 14; ++i) {
						assert(first_invaderer_sprite + i + j * 14 <= last_invaderer_sprite);
						sprites[first_invaderer_sprite + i + j * 14].init(			
							invaderer, ((float)i - num_cols * 0.5f) * 0.4f, 2.75f /*  <-- INCREASE TO HAVE ENEMIES START FROM HIGHER */ - ((float)j * 0.37f), /* <-- increases height between enemies */ 0.25f, 0.25f);
					}
				}
			}
		}

		// fire button (space)
		void fire_missiles() {
			if (missiles_disabled) {
				--missiles_disabled;
			}
			else if (is_key_going_down(' ') && num_lives % 2 != 0)  {					//will shoot only if daytime (light blue coloured background)

				// find a missile
				for (int i = 0; i != num_missiles; ++i) {
					if (!sprites[first_missile_sprite + i].is_enabled()) {
						sprites[first_missile_sprite + i].set_relative(sprites[ship_sprite], 0, 0.5f);
						sprites[first_missile_sprite + i].is_enabled() = true; 
						missiles_disabled = 5;
						ALuint source2 = get_sound_source();
						alSourcei(source2, AL_BUFFER, whoosh);
						alSourcePlay(source2);
						break;
					}
				}
			}
		}

		// pick and invader and fire a bomb 
		void fire_bombs() {
			if (bombs_disabled) {
				--bombs_disabled;
			}
			else {
				// find an invaderer
				sprite &ship = sprites[ship_sprite];
				for (int j = randomizer.get(0, num_invaderers); j < num_invaderers; ++j) {
					sprite &invaderer = sprites[first_invaderer_sprite + j];
					if (invaderer.is_enabled() && invaderer.is_above(ship, 2.0f)) {																																					// find a bomb
						for (int i = 0; i != num_bombs; ++i) {
							if (!sprites[first_bomb_sprite + i].is_enabled()) {
								sprites[first_bomb_sprite + i].set_relative(invaderer, 0, -0.25f);
								sprites[first_bomb_sprite + i].is_enabled() = true;
								bombs_disabled = 0;																					
								ALuint source = get_sound_source();
								alSourcei(source, AL_BUFFER, whoosh);
								alSourcePlay(source);
								return;
							}
						}
						return;
					}
				}
			}
		}

		// animate the missiles
		void move_missiles() {
			const float missile_speed = 0.3f;
			for (int i = 0; i != num_missiles; ++i) {
				sprite &missile = sprites[first_missile_sprite + i];
				if (missile.is_enabled()) {
					missile.translate(0, missile_speed);
					for (int j = 0; j != num_invaderers; ++j) {
						sprite &invaderer = sprites[first_invaderer_sprite + j];
						if (invaderer.is_enabled() && missile.collides_with(invaderer)) {
							invaderer.is_enabled() = false;
							invaderer.translate(20, 0);
							missile.is_enabled() = false;
							missile.translate(20, 0);
							on_hit_invaderer();
							goto next_missile;
						}
					}
					if (missile.collides_with(sprites[first_border_sprite + 1])) {
						missile.is_enabled() = false;
						missile.translate(20, 0);
					}
				}
			next_missile:;
			}
		}

		// animate the bombs
		void move_bombs() {
			const float bomb_speed = 0.2f;
			for (int i = 0; i != num_bombs; ++i) {
				sprite &bomb = sprites[first_bomb_sprite + i];
				if (bomb.is_enabled()) {
					bomb.translate(0, -bomb_speed);
					if (bomb.collides_with(sprites[ship_sprite])) {
						bomb.is_enabled() = false;
						bomb.translate(20, 0);
						bombs_disabled = 50;
						on_hit_ship();
						goto next_bomb;
					}
					if (bomb.collides_with(sprites[first_border_sprite + 0])) {
						bomb.is_enabled() = false;
						bomb.translate(20, 0);
					}
				}
			next_bomb:;
			}
		}

		void gameOverCondition() {												// called when the game has finished and will send next wave to player if he has succeded


			initEnemies();
			game_over = false;
			reWave = true;
			sprites[game_over_sprite].translate(-40, 0.2);						// sprites are moved out of the screen and will be re-used when needed
			sprites[restart_sprite].translate(-48.3, -2.2);

			sprites[success_sprite].translate(-40, 0);
			sprites[nextWave_sprite].translate(-48.3, -2.2);		
		}
				

		void initHolyBananas() {																// initialization of magic bananas that give life once taken

			srand(static_cast<unsigned>(time(0)));												// with this our rand() function will always start differently. without it rand() would always start and end in the same way (pseudo-randomly)

			sprite &theHolyBanana = sprites[holyBanana_sprite];									
			sprite &ship = sprites[ship_sprite];

			if (ship.collides_with(theHolyBanana)) {											// when our character collides with the magic banana
				GLuint holyBanana = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/holyBomb.gif");

				vector<vector<float>> xypoints(10, vector<float>(2));							// A multidimentional vector with 10 rows and 2 columns

				int r = 0;
				int f = 0;
				float i;
				char *inname = "points.txt";
				ifstream infile(inname);
				for (r = 0; r < 10; r++) {														//Reads the x & y locations that we set in our text.document and places them in our multidimentional vector
					for (f = 0; f < 2; f++) {
						infile >> i;
						xypoints[r][f] = i;
						cout << xypoints[r][f];
					}
				}


				int randCount = rand() % 10 + 1;												// gets a random value from 1 - 10


				switch (randCount) {															// use the random value along with the coordinates to spawn the magic banana in different locations drawn from our text file

				case 1:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[0][0], xypoints[0][1], 0.25f, 0.25f);
					break;
				case 2:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[1][0], xypoints[1][1], 0.25f, 0.25f);
					break;
				case 3:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[2][0] * (-1), xypoints[2][1], 0.25f, 0.25f);
					break;
				case 4:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[3][0], xypoints[3][1], 0.25f, 0.25f);
					break;
				case 5:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[4][0] * (-1), xypoints[4][1], 0.25f, 0.25f);
					break;
				case 6:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[5][0], xypoints[5][1] * (-1), 0.25f, 0.25f);
					break;
				case 7:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[6][0], xypoints[6][1] * (-1), 0.25f, 0.25f);
					break;
				case 8:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[7][0] * (-1), xypoints[7][1] * (-1), 0.25f, 0.25f);
					break;
				case 9:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[8][0], xypoints[8][1] * (-1), 0.25f, 0.25f);
					break;
				case 10:
					sprites[holyBanana_sprite].init(holyBanana, xypoints[9][0] * (-1), xypoints[9][1] * (-1), 0.25f, 0.25f);
					break;
				}

				num_lives = num_lives + 1;																			// colliding with the magic banana will give the player an extra life!

			}
		}

		void draw_text(texture_shader &shader, float x, float y, float scale, const char *text) {
			mat4t modelToWorld;
			modelToWorld.loadIdentity();
			modelToWorld.translate(x, y, 0);
			modelToWorld.scale(scale, scale, 1);
			mat4t modelToProjection = mat4t::build_projection_matrix(modelToWorld, cameraToWorld);

			/*mat4t tmp;
			glLoadIdentity();
			glTranslatef(x, y, 0);
			glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&tmp);
			glScalef(scale, scale, 1);
			glGetFloatv(GL_MODELVIEW_MATRIX, (float*)&tmp);*/

			enum { max_quads = 32 };
			bitmap_font::vertex vertices[max_quads * 4];
			uint32_t indices[max_quads * 6];
			aabb bb(vec3(0, 0, 0), vec3(256, 256, 0));

			unsigned num_quads = font.build_mesh(bb, vertices, indices, max_quads, text, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, font_texture);

			shader.render(modelToProjection, 0);

			glVertexAttribPointer(attribute_pos, 3, GL_FLOAT, GL_FALSE, sizeof(bitmap_font::vertex), (void*)&vertices[0].x);
			glEnableVertexAttribArray(attribute_pos);
			glVertexAttribPointer(attribute_uv, 3, GL_FLOAT, GL_FALSE, sizeof(bitmap_font::vertex), (void*)&vertices[0].u);
			glEnableVertexAttribArray(attribute_uv);

			glDrawElements(GL_TRIANGLES, num_quads * 6, GL_UNSIGNED_INT, indices);


		}

	public:

		// this is called when we construct the class
		invaderers_app(int argc, char **argv) : app(argc, argv), font(512, 256, "assets/big.fnt") {
		}

		// this is called once OpenGL is initialized
		void app_init() {
			// set up the shader
			texture_shader_.init();

			// set up the matrices with a camera 5 units from the origin
			cameraToWorld.loadIdentity();
			cameraToWorld.translate(0, 0, 3);

			font_texture = resource_dict::get_texture_handle(GL_RGBA, "assets/big_0.gif");

			GLuint ship = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/ship.gif");
			sprites[ship_sprite].init(ship, 0, -2.75f, 0.25f, 0.25f);

			GLuint GameOver = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/GameOver.gif");
			sprites[game_over_sprite].init(GameOver, 20, 0, 3, 1.5f);

			GLuint Restart = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/restart.gif");
			sprites[restart_sprite].init(Restart, 20, 0, 3, 1.5f);

			GLuint Success = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/success.gif");
			sprites[success_sprite].init(Success, 20, 0, 3, 1.5f);

			GLuint NextWave = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/nextWave.gif");
			sprites[nextWave_sprite].init(NextWave, 20, 0, 3, 1.5f);

			GLuint holyBanana = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/holyBomb.gif");
			sprites[holyBanana_sprite].init(holyBanana, 0.0f, 0.0f, 0.25f, 0.25f);




			// set the border to white for clarity
			GLuint white = resource_dict::get_texture_handle(GL_RGB, "#ef9d97");
			sprites[first_border_sprite + 0].init(white, 0, -3, 6, 0.2f);
			sprites[first_border_sprite + 1].init(white, 0, 3, 6, 0.2f);
			sprites[first_border_sprite + 2].init(white, -3, 0, 0.2f, 6);
			sprites[first_border_sprite + 3].init(white, 3, 0, 0.2f, 6);

			// use the missile texture
			GLuint missile = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/missile.gif");
			for (int i = 0; i != num_missiles; ++i) {
				// create missiles off-screen
				sprites[first_missile_sprite + i].init(missile, 20, 0, 0.0625f, 0.25f);
				sprites[first_missile_sprite + i].is_enabled() = false;
			}

			// use the bomb texture
			GLuint bomb = resource_dict::get_texture_handle(GL_RGBA, "assets/invaderers/bomb.gif");
			for (int i = 0; i != num_bombs; ++i) {
				// create bombs off-screen
				sprites[first_bomb_sprite + i].init(bomb, 20, 0, 0.0625f, 0.25f);
				sprites[first_bomb_sprite + i].is_enabled() = false;
			}

			// sounds
			whoosh = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/whoosh.wav");
			bang = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/bang.wav");
			sandbox = resource_dict::get_sound_handle(AL_FORMAT_MONO16, "assets/invaderers/Sandbox.wav");
			cur_source = 0;
			alGenSources(num_sound_sources, sources);

			//ALuint source = get_sound_source();											If I want to include background music! but decided not to in the end
			//alSourcei(source, AL_BUFFER, sandbox);
			//alSourcePlay(source);

	

			// sundry counters and game state.
			missiles_disabled = 0;
			bombs_disabled = 50;
			invader_velocity = 0.03f;
			live_invaderers = num_invaderers - 7;
			num_lives = 5;
			game_over = false;
			loss = false;
			reWave = true;
			score = 0;
			srand(static_cast<unsigned>(time(0)));												// set this to have a random number at start to have different results in our rand() every time 
		}

		// called every frame to move things
		void simulate() {
			if (game_over) {

				if (is_key_down('Y') && game_over == true) {

					gameOverCondition();																	// Calls the game Over Condition when the game has been lost or won and user wants to continue
				}
				if (is_key_down('R') && game_over == true) {

					app_init();																			// Calls the game Over Condition when the game has been lost or won and user wants to continue
				}
				return;
			}

			move_ship();

			initEnemies();

			fire_missiles();

			fire_bombs();

			move_missiles();

			move_bombs();

			initHolyBananas();

		}

		// this is called to draw the world
		void draw_world(int x, int y, int w, int h) {
			simulate();

			// set a viewport - includes whole window area
			glViewport(x, y, w, h);

			
			// clear the background to blue
			if (num_lives % 2 == 0){							// if lives lives/2 does leaves a remainder then we will have day sky! else night sky
				glClearColor(0.2f, 0.0f, 0.5f, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			}else {
				glClearColor(0, 0.8f, 1, 1);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			}

		

			// don't allow Z buffer depth testing (closer objects are always drawn in front of far ones)
			glDisable(GL_DEPTH_TEST);

			// allow alpha blend (transparency when alpha channel is 0)
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// draw all the sprites
			for (int i = 0; i != num_sprites; ++i) {
				sprites[i].render(texture_shader_, cameraToWorld);
			}

			char score_text[32];
			sprintf(score_text, "score: %d   lives: %d\n", score, num_lives);
			draw_text(texture_shader_, -1.75f, 2, 1.0f / 256, score_text);

			// move the listener with the camera
			vec4 &cpos = cameraToWorld.w();
			alListener3f(AL_POSITION, cpos.x(), cpos.y(), cpos.z());
		}
	};
}
