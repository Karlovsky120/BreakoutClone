#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "physics.h"
#include "renderer.h"
#include "sharedStructures.h"
#include "soundManager.h"
#include "textureManager.h"

#include "common.h"

#include <filesystem>
#include <thread>

Breakout::Breakout() {
    m_renderer       = std::make_unique<Renderer>();
    m_textureManager = std::make_unique<TextureManager>(m_renderer.get());

    UniformData uniformData = {{1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT}, m_textureManager->getTextureId(TEXTURE_CRACKS)};
    m_renderer->uploadToDeviceLocalBuffer(&uniformData, sizeof(uniformData), m_renderer->getUniformBuffer()->buffer);

    m_soundManager = std::make_unique<SoundManager>();
    m_physics      = std::make_unique<Physics>();

    loadAllLevels();
}

Breakout::~Breakout() {}

void Breakout::run() {
    m_renderer->showWindow();
    gameLoop();
}

void Breakout::loadAllLevels() {
    uint32_t    counter = 0;
    std::string path    = std::filesystem::current_path().string() + LEVEL_FOLDER;

    for (const auto& file : std::filesystem::directory_iterator(path)) {
        m_levels.push_back(
            std::make_unique<Level>(file.path().string().c_str(), counter, WINDOW_WIDTH, WINDOW_HEIGHT, m_renderer.get(), m_textureManager.get()));
        ++counter;
    }
}

void Breakout::gameLoop() {
    m_time = std::chrono::high_resolution_clock::now();
    initializeLevel(START_LIFE_COUNT, 0, 0);

    while (!m_quit) {
        pollEvents();

        uint32_t frameTime            = getFrametime();
        uint32_t m_operatingFrametime = m_targetFrameTime == 0 ? frameTime : m_targetFrameTime;

        doGame(m_operatingFrametime);

        m_renderer->acquireImage();
        m_renderer->renderAndPresentImage();

        int32_t  remainingFrameTime = m_operatingFrametime - frameTime;
        uint32_t elapsedFrametime   = frameTime;

        m_stateTimeCounter += m_operatingFrametime;
        m_timeCounter += m_operatingFrametime;
        ++m_frameCount;

        Instance* const bricks = m_currentLevel->getBricksPtr();
        for (const CollisionData& collisionData : m_collisionInfo) {
            uint32_t collisionTime = collisionData.collisionTime;
            if (m_targetFrameTime != 0) {
                if (collisionTime > elapsedFrametime) {
                    std::this_thread::sleep_for(std::chrono::microseconds(collisionTime - elapsedFrametime));
                    elapsedFrametime = collisionTime;
                }
            }

            switch (collisionData.type) {
                case CollisionType::WALL: {
                    m_soundManager->playSound(SOUND_WALL);
                    break;
                }
                case CollisionType::PAD: {
                    m_soundManager->playSound(SOUND_PAD);
                    break;
                }
                case CollisionType::BRICK: {
                    const Instance&  brick     = bricks[collisionData.hitIndex];
                    const BrickType& brickType = m_currentLevel->getBrickData(brick.id);
                    std::string      soundId;
                    if (brick.health == 0) {
                        m_soundManager->playSound(brickType.breakSoundPath);
                    } else {
                        m_soundManager->playSound(brickType.hitSoundPath);
                    }
                    break;
                }
            }
        }

        if (m_operatingFrametime > elapsedFrametime) {
            std::this_thread::sleep_for(std::chrono::microseconds(m_operatingFrametime - elapsedFrametime));
        }
        m_collisionInfo.clear();
    }
}

void Breakout::doGame(const uint32_t& frameTime) {
    switch (m_gameState) {
        case GameState::BEGIN_LEVEL: {
            if (m_stateTimeCounter < BEGIN_LEVEL_BEFORE_FADE + BEGIN_LEVEL_FADE) {
                float alpha = fade(BEGIN_LEVEL_BEFORE_FADE, -BEGIN_LEVEL_FADE, m_stateTimeCounter);
                m_currentLevel->setForegroundVisibility(alpha);
                m_currentLevel->setTitleVisibility(alpha);
            } else {
                m_currentLevel->setSubtitle(TEXTURE_UI_RELEASE);
                m_currentLevel->setSubtitleVisibility(1.0f);
                m_gameState = GameState::BALL_ATTACHED;
            }
            break;
        }
        case GameState::BALL_ATTACHED: {
            m_physics->resolveFrame(frameTime, *m_currentLevel, 0.0f, m_padControl, m_ballDirection, m_collisionInfo);
            if (m_keyPressed[SDLK_SPACE]) {
                m_ballDirection = m_currentLevel->getStartingBallDirection();
                m_currentLevel->setSubtitleVisibility(0.0f);
                m_gameState = GameState::PLAYING;
            }
            break;
        }
        case GameState::PLAYING: {
            switch (m_physics->resolveFrame(frameTime, *m_currentLevel, 1.0f, m_padControl, m_ballDirection, m_collisionInfo)) {
                case LevelState::STILL_ALIVE: {
                    Instance* const bricks = m_currentLevel->getBricksPtr();
                    for (CollisionData& collisionData : m_collisionInfo) {
                        if (collisionData.type == CollisionType::BRICK) {
                            Instance& brick = bricks[collisionData.hitIndex];
                            if (brick.maxHealth < UINT32_MAX) {
                                --brick.health;
                                if (brick.health <= 0) {
                                    const BrickType& brickType = m_currentLevel->getBrickData(brick.id);
                                    m_score += brickType.breakScore;
                                    m_currentLevel->setScore(m_score);
                                    m_currentLevel->destroyBrick();
                                }
                            }
                        }
                    }

                    if (m_currentLevel->getRemainingBrickCount() == 0) {
                        if (m_currentLevelIndex == m_levels.size() - 1) {
                            m_currentLevel->setTitle(TEXTURE_UI_VICTORY);
                        } else {
                            m_currentLevel->setTitle(TEXTURE_UI_LEVEL_COMPLETE);
                        }

                        m_gameState        = GameState::WIN_LEVEL;
                        m_stateTimeCounter = 0;
                    }
                    break;
                }
                case LevelState::LOST: {
                    --m_lifeCount;
                    m_currentLevel->setLifeCount(m_lifeCount);
                    if (m_lifeCount == 0) {
                        m_stateTimeCounter = 0;
                        m_currentLevel->setTitle(TEXTURE_UI_GAME_OVER);
                        m_gameState = GameState::LOSE_GAME;
                    } else {
                        m_currentLevel->resetPadAndBall();
                        m_currentLevel->setSubtitle(TEXTURE_UI_RELEASE);
                        m_currentLevel->setSubtitleVisibility(1.0f);
                        m_gameState = GameState::BALL_ATTACHED;
                    }
                    break;
                }
            }
            break;
        }
        case GameState::LOSE_GAME: {
            if (m_stateTimeCounter < LOSE_GAME_FADE) {
                float alpha = fade(0, LOSE_GAME_FADE, m_stateTimeCounter);
                m_currentLevel->setForegroundVisibility(alpha);
                m_currentLevel->setTitleVisibility(alpha);
            } else {
                m_currentLevel->setSubtitle(TEXTURE_UI_TRY);
                m_currentLevel->setSubtitleVisibility(1.0f);
                m_gameState = GameState::RESTART_SCREEN;
            }
            break;
        }
        case GameState::RESTART_SCREEN: {
            if (m_keyPressed[SDLK_SPACE]) {
                initializeLevel(START_LIFE_COUNT, 0, 0);
                m_stateTimeCounter = 0;
                m_currentLevel->setSubtitleVisibility(0.0f);
                m_currentLevel->resetPadAndBall();
            } else if (m_keyPressed[SDLK_ESCAPE]) {
                m_quit = true;
            }
            break;
        }

        case GameState::WIN_LEVEL: {
            if (m_stateTimeCounter < LEVEL_WIN_FADE) {
                float alpha = fade(0, LEVEL_WIN_FADE, m_stateTimeCounter);
                m_currentLevel->setForegroundVisibility(alpha);
                m_currentLevel->setTitleVisibility(alpha);
            } else {
                if (m_currentLevelIndex < m_levels.size() - 1) {
                    initializeLevel(m_lifeCount, m_score, m_currentLevelIndex + 1);
                } else {
                    m_currentLevel->setSubtitle(TEXTURE_UI_TRY);
                    m_currentLevel->setSubtitleVisibility(1.0f);
                    m_gameState = GameState::WIN_GAME;
                }
            }
            break;
        }
        case GameState::WIN_GAME: {
            if (m_keyPressed[SDLK_SPACE]) {
                initializeLevel(START_LIFE_COUNT, 0, 0);
                m_stateTimeCounter = 0;

                m_gameState = GameState::BEGIN_LEVEL;
            } else if (m_keyPressed[SDLK_ESCAPE]) {
                m_quit = true;
            }
            break;
        }
    }

    m_currentLevel->updateGPUData();

    if (m_timeCounter > 500'000) {
        char title[256];
        sprintf_s(title, "Breakout! Frametime: %.2fms", MICROSECONDS_TO_MILISECONDS(m_timeCounter) / static_cast<float>(m_frameCount));
        m_renderer->setWindowTitle(title);
        m_timeCounter = 0;
        m_frameCount  = 0;
    }
}

void Breakout::initializeLevel(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex) {
    m_score        = score;
    m_lifeCount    = lifeCount;
    m_currentLevel = m_levels[levelIndex].get();
    m_currentLevel->load(m_lifeCount, m_score, levelIndex + 1);
    m_currentLevel->setTitleVisibility(1.0f);
    m_currentLevel->setTitle(TEXTURE_UI_LOADING_LEVEL);
    m_stateTimeCounter  = 0;
    m_currentLevelIndex = levelIndex;
    m_gameState         = GameState::BEGIN_LEVEL;
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

const uint32_t Breakout::getFrametime() {
    std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
    uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_time).count());
    m_time             = currentTime;

    return frameTime;
}

const float Breakout::fade(const int32_t& holdBefore, const int32_t& fadeTime, const uint32_t& currentStateTime) {
    float value = static_cast<int32_t>(currentStateTime - holdBefore) / static_cast<float>(fadeTime) + (fadeTime > 0 ? 0.0f : 1.0f);
    return std::clamp(value, 0.0f, 1.0f);
}
