#pragma once
#include "level.h"

#include "commonExternal.h"

class Breakout {
  public:
    void run();

    Breakout();
    ~Breakout();

  private:
    bool     m_quit = false;
    uint32_t m_time;

    std::vector<Level>          m_levels     = {};
    std::map<SDL_Keycode, bool> m_keyPressed = {};

    void loadLevelData();

    void pollEvents();
    void gameLoop();
};
