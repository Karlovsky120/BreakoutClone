#include "SoundManager.h"

#include <filesystem>

SoundManager::SoundManager() { Mix_OpenAudio(44100, AUDIO_S32, 2, 1024); }

void SoundManager::playSound(const std::string& soundId) {
    auto soundCacheMapEntry = m_sounds.find(soundId);
    if (soundCacheMapEntry != m_sounds.end()) {
        Mix_PlayChannel(-1, soundCacheMapEntry->second, 0);
    } else {
        Mix_PlayChannel(-1, loadSound(soundId), 0);
    }
}

SoundManager::~SoundManager() {
    for (auto& entry : m_sounds) {
        Mix_FreeChunk(entry.second);
    }

    Mix_CloseAudio();
}

Mix_Chunk* SoundManager::loadSound(const std::string& soundId) {
    std::string path = std::filesystem::current_path().string();
    path += SOUND_FOLDER + soundId;

    m_sounds[soundId] = Mix_LoadWAV(path.c_str());
    return m_sounds[soundId];
}