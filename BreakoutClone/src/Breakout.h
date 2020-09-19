#pragma once
#include "level.h"

#include "commonExternal.h"

#include <chrono>

// In frames per second. Set to 0 for uncapped.
#define TARGET_FRAMERATE 200

class Breakout {
  public:
    void run();

    Breakout();
    ~Breakout();

  private:
    bool     m_quit        = false;
    uint32_t m_timeCounter = 0;
    uint32_t m_frameCount  = 0;

    std::chrono::high_resolution_clock::time_point m_time;

    std::vector<Level>          m_levels     = {};
    std::map<SDL_Keycode, bool> m_keyPressed = {};

    void loadLevelData();

    void           pollEvents();
    const uint32_t getFrametime();
    void           gameLoop();
};
