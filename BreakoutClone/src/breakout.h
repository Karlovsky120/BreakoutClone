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

/// <summary>
/// States that the game can be in.
/// </summary>
enum class GameState { BEGIN_LEVEL, BALL_ATTACHED, PLAYING, LOSE_LIFE, LOSE_GAME, RESTART_SCREEN, WIN_LEVEL, WIN_GAME };

/// <summary>
/// Main class that initializes all the other components required for runing the game and manages the game state.
/// </summary>
class Breakout {
  public:
    /// <summary>
    /// Initializes renderer, texture manager, sound manager and physics engine. Loads data into uniform buffer and loads all levels.
    /// </summary>
    Breakout();
    ~Breakout();

    /// <summary>
    /// Shows window and enters main game loop.
    /// </summary>
    void run();

  private:
    /// <summary>
    /// Global renderer.
    /// </summary>
    std::unique_ptr<Renderer> m_renderer;

    /// <summary>
    /// Global texture manager.
    /// </summary>
    std::unique_ptr<TextureManager> m_textureManager;

    /// <summary>
    /// Global sound manager.
    /// </summary>
    std::unique_ptr<SoundManager> m_soundManager;

    /// <summary>
    /// Global physics reslover.
    /// </summary>
    std::unique_ptr<Physics> m_physics;

    /// <summary>
    /// Timestamp used to calculate sleep time and step time for physics.
    /// </summary>
    std::chrono::high_resolution_clock::time_point m_time;

    /// <summary>
    /// Frame time targeted by the application. The application will not run faster than this.
    /// </summary>
    uint32_t m_targetFrameTime = TARGET_FRAMERATE == 0 ? 0 : 1'000'000 / TARGET_FRAMERATE;

    /// <summary>
    /// Timer for fade in and fade out operations at the start of various game states.
    /// </summary>
    uint32_t m_stateTimeCounter = 0;

    /// <summary>
    /// Time counter used for window title frametime updates.
    /// </summary>
    uint32_t m_timeCounter = 0;

    /// <summary>
    /// Counter of frames for calculating average frametime between window title updates
    /// </summary>
    uint32_t m_frameCount = 0;

    /// <summary>
    /// Boolean used to control when the game exits.
    /// </summary>
    bool m_quit = false;

    /// <summary>
    /// X-axis speed of the pad for the current frame.
    /// </summary>
    float m_padControl = 0;

    /// <summary>
    /// Current direction of the ball, normalized.
    /// </summary>
    glm::vec2 m_ballDirection = {0.0f, 0.0f};

    /// <summary>
    /// Remaining player lives.
    /// </summary>
    uint32_t m_lifeCount = START_LIFE_COUNT;

    /// <summary>
    /// Current player score
    /// </summary>
    uint32_t m_score = 0;

    /// <summary>
    /// Current game state.
    /// </summary>
    GameState m_gameState = GameState::BEGIN_LEVEL;

    /// <summary>
    /// Pointer to currently active level.
    /// </summary>
    Level* m_currentLevel = nullptr;

    /// <summary>
    /// Index of the currently active level.
    /// </summary>
    uint32_t m_currentLevelIndex = 0;

    /// <summary>
    /// Vector of all loaded levels.
    /// </summary>
    std::vector<std::unique_ptr<Level>> m_levels = {};

    /// <summary>
    /// Map holding the states of any pressed or released keys.
    /// </summary>
    std::map<SDL_Keycode, bool> m_keyPressed = {};

    /// <summary>
    /// Vector of collision data for the current frame.
    /// </summary>
    std::vector<CollisionData> m_collisionInfo = {};

    /// <summary>
    /// Loads all levels found in levels folder, alphabetically.
    /// </summary>
    void loadAllLevels();

    /// <summary>
    /// Initializes the game and runs the game loop for each frame until the game is shutdown.
    /// </summary>
    void gameLoop();

    /// <summary>
    /// Runs a single frame worth of game logic.
    /// </summary>
    void doGame(const uint32_t& frameTime);

    /// <summary>
    /// Sets up a level, making it start next iteration of the game loop.
    /// </summary>
    /// <param name="lifeCount">Amount of lives to be initially shown on screen.</param>
    /// <param name="score">Score to be initially shown on screen.</param>
    /// <param name="levelIndex">Ordinal number of the level to be initially shown on screen.</param>
    void initializeLevel(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex);

    /// <summary>
    /// Stores key presses and handles pad input.
    /// </summary>
    void pollEvents();

    /// <summary>
    /// Returns time passed since it was previous called and stores current timestamp for next measurement.
    /// </summary>
    /// <returns>Time passed since it was previously called in microseconds.</returns>
    const uint32_t getFrametime();

    /// <summary>
    /// Calculates an alpha value based on time and parameters given.
    /// </summary>
    /// <param name="holdBefore">Amount of time to keep alpha clamped to initial value (0.0f or 1.0f, see fadeTime param) before fading in or out.</param>
    /// <param name="fadeTime">Time it takes to fade from visible to black. Negative value fades from black to visible.</param>
    /// <param name="currentStateTime">Timestamp from the start for which alpha value is to be calculated.</param>
    /// <returns>Alpha value for fade at the input timestamp.</returns>
    const float fade(const int32_t& holdBefore, const int32_t& fadeTime, const uint32_t& currentStateTime);
};
