#pragma once
#include "level.h"

#include "commonExternal.h"

#include <chrono>

// In frames per second. Set to 0 for uncapped.
#define TARGET_FRAMERATE 200

#define START_LIFE_COUNT 3

#define BEGIN_LEVEL_BEFORE_FADE SECONDS_TO_MICROSECONDS(1)
#define BEGIN_LEVEL_FADE        SECONDS_TO_MICROSECONDS(2)
#define LOSE_GAME_FADE          SECONDS_TO_MICROSECONDS(3)
#define LEVEL_WIN_FADE          SECONDS_TO_MICROSECONDS(3)

enum class GameState { BEGIN_LEVEL, BALL_ATTACHED, PLAYING, LOSE_LIFE, LOSE_GAME, RESTART_SCREEN, WIN_LEVEL, WIN_GAME };

class Breakout {
  public:
    void run();

    static const Level* getActiveLevel();

    Breakout();
    ~Breakout();

  private:
    uint32_t m_targetFrameTime = 1'000'000 / TARGET_FRAMERATE;
    uint32_t m_timeCounter     = 0;
    uint32_t m_frameCount      = 0;

    bool     m_quit             = false;
    uint32_t m_stateTimeCounter = 0;

    uint32_t m_currentLevelIndex = 0;

    uint32_t m_score     = 0;
    uint32_t m_lifeCount = START_LIFE_COUNT;

    float m_padControl = 0.0f;

    GameState m_gameState = GameState::BEGIN_LEVEL;

    static Level* m_currentLevel;

    std::chrono::high_resolution_clock::time_point m_time;

    std::vector<Level>          m_levels     = {};
    std::map<SDL_Keycode, bool> m_keyPressed = {};

    void loadLevelData();

    void           pollEvents();
    void           doGame(const uint32_t& frameTime);
    const uint32_t getFrametime();
    void           gameLoop();
};
