#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "physics.h"
#include "renderer.h"
#include "resources.h"
#include "sharedStructures.h"

#include "common.h"

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

void Breakout::pollEvents() {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
        case SDL_QUIT:
            m_quit = true;
            break;
        case SDL_KEYDOWN:
            m_keyPressed[sdlEvent.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            m_keyPressed[sdlEvent.key.keysym.sym] = false;
            break;
        }
    }
}

void Breakout::gameLoop() {
    std::chrono::high_resolution_clock::time_point oldTime = std::chrono::high_resolution_clock::now();

    while (!m_quit) {
        pollEvents();

        Renderer::getInstance()->acquireImage();

        std::chrono::high_resolution_clock::time_point newTime = std::chrono::high_resolution_clock::now();
        uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(newTime - oldTime).count());
        oldTime            = newTime;
        m_time += frameTime;

        float     ballSpeed     = 0.0f;
        float     padSpeed      = 0.0f;
        glm::vec2 ballDirection = {0.0f, -1.0f};

        if (m_keyPressed[SDLK_a]) {
            padSpeed = -0.001f;
        }

        if (m_keyPressed[SDLK_d]) {
            padSpeed = 0.001f;
        }

        Physics::resolveFrame(frameTime, m_levels[0], ballSpeed, padSpeed, ballDirection);
        m_levels[0].updateGPUData();

        if (m_time > 500'000) {
            char title[256];
            sprintf_s(title, "Breakout! Frametime: %.2fms", frameTime / 1'000.0f);
            Renderer::setWindowTitle(title);
            m_time = 0;
        }

        Renderer::getInstance()->renderAndPresentImage();
    }

    vkDeviceWaitIdle(Renderer::getDevice());
}
