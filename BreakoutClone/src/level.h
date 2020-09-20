#pragma once
#include "physics.h"
#include "resources.h"
#include "sharedStructures.h"

#include "common.h"
#include "commonExternal.h"

// Smaller the number, stronger the blur
#define SIDE_BLUR_STRENGTH (1.0f / 16.0f)

#define LIVES_COUNT 3

#define MAX_COLUMN_COUNT 35
#define MAX_ROW_COUNT    30

#define MAX_COLUMN_SPACING 5
#define MAX_ROW_SPACING    5

#define BACKGROUND_INDEX  0
#define LEFT_WALL_INDEX   1
#define RIGHT_WALL_INDEX  2
#define PAD_INDEX         3
#define BALL_INDEX        4
#define BRICK_START_INDEX 5

#define PAD_SPEED_FACTOR  0.5f
#define BALL_SPEED_FACTOR 0.5f

#define FOREGROUND_DEPTH 0.1f
#define GAME_DEPTH       0.5f
#define BACKGROUND_DEPTH 0.9f

struct BrickType {
    uint32_t    id             = 0;
    uint32_t    hitPoints      = 0;
    int32_t     breakScore     = 0;
    uint32_t    textureId      = 0;
    std::string texturePath    = "";
    std::string hitSoundPath   = "";
    std::string breakSoundPath = "";
};

class Level {
  public:
    void                   load(const bool& reset = false);
    void                   updateGPUData() const;
    std::vector<Instance>& getInstances();
    const glm::vec2        getWindowDimensions() const;
    const uint32_t&        getTotalBrickCount() const;
    const uint32_t&        getRemainingBrickCount() const;
    const float&           getBasePadSpeed() const;
    const float&           getBaseBallSpeed() const;
    const uint32_t&        destroyBrick();
    glm::vec2&             getBallDirection();
    const uint32_t&        loseLife();
    Instance&              getForeground();
    void                   resetPadAndBall();
    Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight);
    void destroy();

  private:
    std::string m_backgroundTexturePath;

    uint32_t m_rowCount      = 0;
    uint32_t m_columnCount   = 0;
    uint32_t m_rowSpacing    = 0;
    uint32_t m_columnSpacing = 0;

    uint32_t m_levelIndex          = 0;
    uint32_t m_brickCount          = 0;
    uint32_t m_remainingBrickCount = 0;
    uint32_t m_remainingLivesCount = 0;

    uint32_t m_windowWidth  = 0;
    uint32_t m_windowHeight = 0;

    uint32_t m_foregroundIndex = 0;

    uint32_t m_transparentTextureId = 0;

    uint32_t m_padTextureId;
    uint32_t m_ballTextureId;
    uint32_t m_foregroundTextureId;
    uint32_t m_backgroundTextureId;
    uint32_t m_backgroundTextureSmallId;

    glm::vec2 m_padInitialPosition;
    glm::vec2 m_ballInitialPosition;

    float m_basePadSpeed;
    float m_baseBallSpeed;

    glm::vec2 m_ballDirection = {0.0f, 0.0f};

    std::vector<Instance>              m_instances;
    std::vector<std::vector<uint32_t>> m_levelLayout;

    std::map<uint32_t, BrickType>   m_brickTypes;
    std::map<std::string, uint32_t> m_textureCacheMap;

    std::vector<Image> m_textures;

    Buffer   m_instanceBuffer;
    uint32_t m_instanceDataBufferSize;

    void parseXml(const char* fullPath);
    void generateRenderData();

    uint32_t loadTexture(const std::string& pathToTexture, const float& scale = 1.0f);
};
