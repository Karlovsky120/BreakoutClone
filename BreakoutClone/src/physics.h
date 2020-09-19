#pragma once

#include "common.h"

#include "level.h"

#include <vector>

class Physics {
  public:
    void resolveFrame(const uint32_t& frameTimeUS, std::vector<Instance>& instances);
};
