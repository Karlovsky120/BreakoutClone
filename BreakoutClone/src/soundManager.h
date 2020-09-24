#pragma once

#include "common.h"

#include "SDL_mixer.h"

#include <string>

#define SOUND_FOLDER "\\resources\\sounds\\"

#define SOUND_PAD  "pad.wav"
#define SOUND_WALL "wall.wav"

class SoundManager {
  public:
    SoundManager();
    ~SoundManager();

    void playSound(const std::string& soundId);

  private:
    Mix_Chunk* loadSound(const std::string& soundId);

    std::map<std::string, Mix_Chunk*> m_sounds;
};