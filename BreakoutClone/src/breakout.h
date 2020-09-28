#pragma once
#include "level.h"

#include "commonExternal.h"

#include <chrono>
#include <memory>

// In frames per second
#define TARGET_FRAMERATE 144

#define START_LIFE_COUNT 5

#define BEGIN_LEVEL_BEFORE_FADE SECONDS_TO_MICROSECONDS(1)
#define BEGIN_LEVEL_FADE        SECONDS_TO_MICROSECONDS(2)
#define LOSE_GAME_FADE          SECONDS_TO_MICROSECONDS(3)
#define LEVEL_WIN_FADE          SECONDS_TO_MICROSECONDS(3)

struct CollisionData;

class Physics;
class Renderer;
class SoundManager;
class TextureManager;

enum class GameState { BEGIN_LEVEL, BALL_ATTACHED, PLAYING, LOSE_LIFE, LOSE_GAME, RESTART_SCREEN, WIN_LEVEL, WIN_GAME };

class Breakout {
  public:
    Breakout();
    ~Breakout();

    void run();

  private:
    std::unique_ptr<Renderer>       m_renderer;
    std::unique_ptr<TextureManager> m_textureManager;
    std::unique_ptr<SoundManager>   m_soundManager;
    std::unique_ptr<Physics>        m_physics;

    std::chrono::high_resolution_clock::time_point m_time;

    uint32_t m_targetFrameTime  = TARGET_FRAMERATE == 0 ? 0 : 1'000'000 / TARGET_FRAMERATE;
    uint32_t m_stateTimeCounter = 0;
    uint32_t m_timeCounter      = 0;
    uint32_t m_frameCount       = 0;

    bool m_quit = false;

    float     m_padControl;
    glm::vec2 m_ballDirection = {0.0f, 0.0f};

    uint32_t  m_lifeCount = START_LIFE_COUNT;
    uint32_t  m_score     = 0;
    GameState m_gameState = GameState::BEGIN_LEVEL;

    Level*                              m_currentLevel;
    uint32_t                            m_currentLevelIndex = 0;
    std::vector<std::unique_ptr<Level>> m_levels            = {};

    std::map<SDL_Keycode, bool> m_keyPressed = {};
    std::vector<CollisionData>  m_collisionInfo;

    void loadAllLevels();
    void gameLoop();
    void doGame(const uint32_t& frameTime);
    void initializeLevel(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex);
    void pollEvents();

    const uint32_t getFrametime();
    const float    fade(const int32_t& holdBefore, const int32_t& fadeTime, const uint32_t& currentStateTime);
};
