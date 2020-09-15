#pragma once

#include "common.h"

#include <vector>

struct BrickType {
    uint32_t    id             = UINT32_MAX;
    const char* texturePath    = nullptr;
    uint32_t    hitPoints      = 0;
    const char* hitSoundPath   = nullptr;
    const char* breakSoundPath = nullptr;
    int32_t     breakScore     = 0;
};

class Level {
  public:
    Level(const char* path);

  private:
    uint32_t    rowCount              = 0;
    uint32_t    columnCount           = 0;
    uint32_t    rowSpacing            = 0;
    uint32_t    columnSpacing         = 0;
    const char* backgroundTexturePath = nullptr;
    uint32_t    lastFilledRow         = 0;

    std::vector<BrickType> brickTypes;

    std::vector<std::vector<uint32_t>> levelLayout;
};
