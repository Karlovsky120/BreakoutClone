#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "physics.h"
#include "renderer.h"
#include "resources.h"
#include "sharedStructures.h"

#include "common.h"

#include <filesystem>
#include <thread>

void Breakout::run() { gameLoop(); }

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

    m_padControl = 0.0f;
    if (m_keyPressed[SDLK_a]) {
        m_padControl += -1.0f;
    }

    if (m_keyPressed[SDLK_d]) {
        m_padControl += 1.0f;
    }
}

float fade(const int32_t& holdBefore, const int32_t& fadeTime, const uint32_t& currentStateTime) {
    float value = static_cast<int32_t>(currentStateTime - holdBefore) / static_cast<float>(fadeTime) + (fadeTime > 0 ? 0.0f : 1.0f);
    return std::clamp(value, 0.0f, 1.0f);
}

void Breakout::doGame(const uint32_t& /*frameTime*/) {
    switch (m_gameState) {
    case GameState::BEGIN_LEVEL: {
        if (m_stateTimeCounter < BEGIN_LEVEL_BEFORE_FADE + BEGIN_LEVEL_FADE) {
            float alpha                                  = fade(BEGIN_LEVEL_BEFORE_FADE, -BEGIN_LEVEL_FADE, m_stateTimeCounter);
            m_currentLevel->getForeground().textureAlpha = alpha;
        } else {
            m_gameState = GameState::BALL_ATTACHED;
        }
        break;
    }
    case GameState::BALL_ATTACHED: {
        Physics::resolveFrame(m_targetFrameTime, *m_currentLevel, 0.0f, m_padControl);
        if (m_keyPressed[SDLK_SPACE]) {
            m_currentLevel->getBallDirection() = {0.0f, -1.0f};
            m_gameState                        = GameState::PLAYING;
        }
        break;
    }
    case GameState::PLAYING: {
        switch (Physics::resolveFrame(m_targetFrameTime, *m_currentLevel, 1.0f, m_padControl)) {
        case LevelState::HUGE_SUCCESS: {
            m_stateTimeCounter = 0;
            m_gameState        = GameState::WIN_LEVEL;
            break;
        }
        case LevelState::LOST: {
            if (m_currentLevel->loseLife() == 0) {
                m_stateTimeCounter = 0;
                m_gameState        = GameState::LOSE_GAME;
            } else {
                m_currentLevel->resetPadAndBall();
                m_gameState = GameState::BALL_ATTACHED;
            }
            break;
        }
        case LevelState::STILL_ALIVE: {
            break;
        }
        }
        break;
    }
    case GameState::LOSE_GAME: {
        if (m_stateTimeCounter < LOSE_GAME_FADE) {
            float alpha                                  = fade(0, LOSE_GAME_FADE, m_stateTimeCounter);
            m_currentLevel->getForeground().textureAlpha = alpha;
        } else {
            m_gameState = GameState::RESTART_SCREEN;
        }
        break;
    }
    case GameState::RESTART_SCREEN: {
        if (m_keyPressed[SDLK_SPACE]) {
            m_stateTimeCounter = 0;
            m_currentLevel->resetPadAndBall();
            m_gameState = GameState::BEGIN_LEVEL;
        } else if (m_keyPressed[SDLK_ESCAPE]) {
            m_quit = true;
        }
        break;
    }

    case GameState::WIN_LEVEL: {
        if (m_stateTimeCounter < LEVEL_WIN_FADE) {
            float alpha                                  = fade(0, LEVEL_WIN_FADE, m_stateTimeCounter);
            m_currentLevel->getForeground().textureAlpha = alpha;
        } else {
            m_gameState        = GameState::BEGIN_LEVEL;
            m_stateTimeCounter = 0;
            ++m_currentLevelIndex;
            if (m_currentLevelIndex < m_levels.size()) {
                m_currentLevel = &m_levels[m_currentLevelIndex];
                m_currentLevel->load(true);
            } else {
                m_stateTimeCounter = 0;
                m_gameState        = GameState::WIN_GAME;
            }
        }
        break;
    }
    case GameState::WIN_GAME: {
        break;
    }
    }

    m_currentLevel->updateGPUData();

    if (m_timeCounter > 500'000) {
        char title[256];
        sprintf_s(title, "Breakout! Frametime: %.2fms", MICROSECONDS_TO_MILISECONDS(m_timeCounter) / static_cast<float>(m_frameCount));
        Renderer::setWindowTitle(title);
        m_timeCounter = 0;
        m_frameCount  = 0;
    }
}

const uint32_t Breakout::getFrametime() {
    std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
    uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_time).count());
    m_time             = currentTime;

    return frameTime;
}

void Breakout::gameLoop() {
    m_time              = std::chrono::high_resolution_clock::now();
    m_gameState         = GameState::BEGIN_LEVEL;
    m_stateTimeCounter  = 0;
    m_currentLevelIndex = 0;
    m_currentLevel      = &m_levels[0];
    m_currentLevel->load();

    while (!m_quit) {
        pollEvents();

        uint32_t frameTime = getFrametime();

        doGame(frameTime);

        Renderer::getInstance()->acquireImage();
        Renderer::getInstance()->renderAndPresentImage();

        int32_t remainingFrameTime = m_targetFrameTime - frameTime;
        std::this_thread::sleep_for(std::chrono::microseconds(remainingFrameTime));

        m_stateTimeCounter += m_targetFrameTime;

        m_timeCounter += m_targetFrameTime == 0 ? frameTime : m_targetFrameTime;
        ++m_frameCount;
    }

    vkDeviceWaitIdle(Renderer::getDevice());
}
