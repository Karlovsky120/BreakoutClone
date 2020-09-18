#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "renderer.h"
#include "resources.h"
#include "sharedStructures.h"

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/gtx/rotate_vector.hpp"
#pragma warning(pop)

#include <array>
#include <filesystem>

void Breakout::run() {
    m_levels[0].load();
    gameLoop();
}

Breakout::Breakout() {
    Renderer::initRenderer();
    loadLevelData();
}

Breakout::~Breakout() {
    for (Level& level : m_levels) {
        level.destroy();
    }

    Renderer::getInstance()->destroy();
}

void Breakout::loadLevelData() {
    std::filesystem::path fullPath = std::filesystem::current_path();
    fullPath += "\\resources\\levels\\";
    uint32_t counter = 0;
    for (const auto& file : std::filesystem::directory_iterator(fullPath)) {
        m_levels.push_back(Level(file.path().string().c_str(), counter, WINDOW_WIDTH, WINDOW_HEIGHT));
        ++counter;
    }
}

void Breakout::gameLoop() {
    bool updatedUI = false;

    std::chrono::high_resolution_clock::time_point oldTime = std::chrono::high_resolution_clock::now();
    uint32_t                                       time    = 0;

    SDL_Event sdlEvent;
    bool      quit = false;

    while (!quit) {
        while (SDL_PollEvent(&sdlEvent)) {
            switch (sdlEvent.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                // m_keyStates[sdlEvent.key.keysym.sym].pressed = true;
                break;
            case SDL_KEYUP:
                // m_keyStates[sdlEvent.key.keysym.sym].pressed = false;
                break;
            }
        }

        Renderer::getInstance()->acquireImage();

        std::chrono::high_resolution_clock::time_point newTime = std::chrono::high_resolution_clock::now();
        uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(newTime - oldTime).count());
        oldTime            = newTime;
        time += frameTime;

        if (time > 500'000 || updatedUI) {
            char title[256];
            sprintf_s(title, "Breakout! Frametime: %.2fms", frameTime / 1'000.0f);
            Renderer::setWindowTitle(title);
            time      = 0;
            updatedUI = false;
        }

        Renderer::getInstance()->renderAndPresentImage();
    }

    vkDeviceWaitIdle(Renderer::getDevice());
}
