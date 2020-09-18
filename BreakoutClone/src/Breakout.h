#pragma once

#include "level.h"

class Breakout {
  public:
    void run();

    Breakout();
    ~Breakout();

  private:
    std::vector<Level> m_levels = {};

    void loadLevelData();
    void gameLoop();
};
