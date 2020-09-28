#pragma once

#include "common.h"

#include "SDL_mixer.h"

#define SOUND_FOLDER "\\resources\\sounds\\"

#define SOUND_PAD     "pad.wav"
#define SOUND_WALL    "wall.wav"
#define SOUND_WILHELM "wilhelm.wav"

/// <summary>
/// Class used to manage sounds
/// </summary>
class SoundManager {
  public:
    SoundManager();
    ~SoundManager();

    /// <summary>
    /// Plays the requested sound. Loads the sound if not already loaded.
    /// </summary>
    /// <param name="soundId">Path to the sound relative to the sound folder.</param>
    void playSound(const std::string& soundId);

  private:
    /// <summary>
    /// Loads a sound into memory.
    /// </summary>
    /// <param name="soundId">Path to the sound relative to the sound folder.</param>
    /// <returns>Pointer to sound data.</returns>
    Mix_Chunk* loadSound(const std::string& soundId);

    /// <summary>
    /// Map of all loaded sounds.
    /// </summary>
    std::map<std::string, Mix_Chunk*> m_sounds;
};