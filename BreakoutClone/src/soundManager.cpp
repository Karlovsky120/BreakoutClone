#include "SoundManager.h"

#include <filesystem>
#include <fstream>

void SoundManager::init() { Mix_OpenAudio(44100, AUDIO_S32, 2, 1024); }

void SoundManager::playSound(const std::string& soundId) {
    auto soundCacheMapEntry = m_sounds.find(soundId);
    if (soundCacheMapEntry != m_sounds.end()) {
        Mix_PlayChannel(-1, soundCacheMapEntry->second, 0);
    } else {
        Mix_PlayChannel(-1, loadSound(soundId), 0);
    }
}

void SoundManager::deinit() {
    for (auto& entry : m_sounds) {
        Mix_FreeChunk(entry.second);
    }

    Mix_CloseAudio();
}

Mix_Chunk* SoundManager::loadSound(const std::string& soundId) {
    std::filesystem::path fullPath = std::filesystem::current_path();
    fullPath += "\\resources\\sounds\\";
    fullPath += soundId;

    m_sounds[soundId] = Mix_LoadWAV(fullPath.string().c_str());

    return m_sounds[soundId];
}

std::map<std::string, Mix_Chunk*> SoundManager::m_sounds;