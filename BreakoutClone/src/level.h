#pragma once

#include "common.h"
#include "resources.h"
#include "sharedStructures.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

#include <array>
#include <map>
#include <vector>

#define MAX_COLUMN_COUNT 35
#define MAX_ROW_COUNT    30

#define MAX_COLUMN_SPACING 5
#define MAX_ROW_SPACING    5

#define LEFT_WALL_INDEX   0
#define RIGHT_WALL_INDEX  1
#define BACKGROUND_INDEX  2
#define PAD_INDEX         3
#define BALL_INDEX        4
#define BRICK_START_INDEX 5

#define GAME_DEPTH       0.5f
#define BACKGROUND_DEPTH 0.9f

struct BrickType {
    uint32_t    id             = 0;
    uint32_t    hitPoints      = 0;
    int32_t     breakScore     = 0;
    uint32_t    textureId      = 0;
    std::string texturePath    = "transparent.png";
    std::string hitSoundPath   = "";
    std::string breakSoundPath = "";
};

struct BrickState {
    uint32_t id;
    uint32_t health;
};

class Level {
  public:
    const uint32_t getBrickCount() const;
    void           load(const VkBuffer& vertexBuffer, const VkBuffer& indexBuffer, std::array<VkDrawIndexedIndirectCommand, 2>& drawCommands,
                        VkBuffer& drawCommandsBuffer);
    Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight);
    void destroy();

  private:
    std::string m_backgroundTexturePath;

    uint32_t m_rowCount      = 0;
    uint32_t m_columnCount   = 0;
    uint32_t m_rowSpacing    = 0;
    uint32_t m_columnSpacing = 0;

    uint32_t m_levelIndex = 0;
    uint32_t m_brickCount = 0;

    uint32_t m_windowWidth  = 0;
    uint32_t m_windowHeight = 0;

    uint32_t m_transparentTextureId;

    uint32_t m_padTextureId;
    uint32_t m_ballTextureId;
    uint32_t m_backgroundTextureId;
    uint32_t m_backgroundTextureSmallId;

    std::vector<Instance>                m_instances;
    std::vector<std::vector<BrickState>> m_levelState;

    std::map<uint32_t, BrickType>   m_brickTypes;
    std::map<std::string, uint32_t> m_textureCacheMap;

    std::vector<Image> m_textures;

    Buffer m_instanceBuffer;

    void parseXml(const char* fullPath);
    void generateRenderData();

    uint32_t loadTexture(const std::string& pathToTexture, const float& scale = 1.0f);
};
