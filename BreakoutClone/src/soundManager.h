#pragma once

#include "common.h"

#include "SDL_mixer.h"

#include <map>
#include <string>

#define FOLDER_SOUND "sounds\\"

#define SOUND_WILHELM "wilhelm.wav"
#define SOUND_PAD     "pad.wav"
#define SOUND_WALL    "wall.wav"

class SoundManager {
  public:
    static void init();
    static void playSound(const std::string& soundId);
    static void deinit();

  private:
    static Mix_Chunk* loadSound(const std::string& soundId);

    static std::map<std::string, Mix_Chunk*> m_sounds;
};