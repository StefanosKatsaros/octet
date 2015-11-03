////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2012-2014
//
// Modular Framework for OpenGLES2 rendering on multiple platforms.
//
// Text overlay
//

#pragma warning(disable : 4267)
#define OCTET_BULLET 1


#include "../../octet.h"
#include "example_shapes.h"
//#include "include/fmod.h"
//#include "stdlib.h"
//#include "sound.hpp"

//FMUSIC_MODULE* handle;

/// Create a box with octet
int main(int argc, char **argv) {
  // set up the platform.

  
  octet::app::init_all(argc, argv);

  // our application.
  octet::example_shapes app(argc, argv);

  app.init();

  // open windows
  octet::app::run_all_apps();

 // Sound::initialise();									// fmod tryout 2
 // Sound::load("Sandbox.wav");
//  Sound::play();

  // init FMOD sound system									// fmod tryout 1
//  FSOUND_Init(44100, 32, 0);
  // load song
//  handle = FMUSIC_LoadSong("Sandbox.wav");
  // make the song loop
 // FMUSIC_SetLooping(handle, true);
  // play the song
//  FMUSIC_PlaySong(handle);
}



