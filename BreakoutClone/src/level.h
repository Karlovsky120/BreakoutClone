#pragma once

#include "common.h"

#include <map>
#include <vector>

struct BrickType {
    uint32_t    id             = UINT32_MAX;
    const char* texturePath    = nullptr;
    uint32_t    hitPoints      = 0;
    const char* hitSoundPath   = nullptr;
    const char* breakSoundPath = nullptr;
    int32_t     breakScore     = 0;
};

struct BrickState {
    uint32_t id;
    uint32_t health;
};

class Level {
  public:
    std::vector<std::vector<BrickState>> levelState;

    uint32_t rowCount      = 0;
    uint32_t columnCount   = 0;
    uint32_t rowSpacing    = 0;
    uint32_t columnSpacing = 0;

    Level(const char* path);

  private:
    const char* backgroundTexturePath = nullptr;
    uint32_t    lastFilledRow         = 0;

    std::map<uint32_t, BrickType>      brickTypes;
    std::vector<std::vector<uint32_t>> levelLayout;
};
