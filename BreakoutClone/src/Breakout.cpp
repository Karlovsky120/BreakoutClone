#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "physics.h"
#include "renderer.h"
#include "resources.h"
#include "sharedStructures.h"

#include "common.h"

#include <filesystem>
#include <thread>

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

const uint32_t Breakout::getFrametime() {
    std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
    uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_time).count());
    m_time             = currentTime;

    return frameTime;
}

void Breakout::gameLoop() {
    uint32_t targetFrameTime = 1'000'000 / TARGET_FRAMERATE;

    m_time = std::chrono::high_resolution_clock::now();

    while (!m_quit) {
        pollEvents();

        float     ballSpeed     = 0.0f;
        float     padSpeed      = 0.0f;
        glm::vec2 ballDirection = {0.0f, -1.0f};

        if (m_keyPressed[SDLK_a]) {
            padSpeed = -0.001f;
        }

        if (m_keyPressed[SDLK_d]) {
            padSpeed = 0.001f;
        }

        uint32_t frameTime = getFrametime();
        Physics::resolveFrame(frameTime, m_levels[0], ballSpeed, padSpeed, ballDirection);
        m_levels[0].updateGPUData();

        if (m_timeCounter > 500'000) {
            char title[256];
            sprintf_s(title, "Breakout! Frametime: %.2fms", MILISECONDS(m_timeCounter) / static_cast<float>(m_frameCount));
            Renderer::setWindowTitle(title);
            m_timeCounter = 0;
            m_frameCount  = 0;
        }

        Renderer::getInstance()->acquireImage();
        Renderer::getInstance()->renderAndPresentImage();

        int32_t remainingFrameTime = targetFrameTime - frameTime;
        std::this_thread::sleep_for(std::chrono::microseconds(remainingFrameTime));

        m_timeCounter += targetFrameTime == 0 ? frameTime : targetFrameTime;
        ++m_frameCount;
    }

    vkDeviceWaitIdle(Renderer::getDevice());
}
