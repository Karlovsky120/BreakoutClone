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

#define MAX_COLUMN_COUNT 40
#define MAX_ROW_COUNT    30

#define MAX_COLUMN_SPACING 5
#define MAX_ROW_SPACING    5

#define BALL_INDEX        0
#define LEFT_WALL_INDEX   1
#define RIGHT_WALL_INDEX  2
#define PAD_INDEX         3
#define BRICK_START_INDEX 4

struct BrickType {
    uint32_t    id             = 0;
    uint32_t    hitPoints      = 0;
    int32_t     breakScore     = 0;
    const char* texturePath    = nullptr;
    const char* hitSoundPath   = nullptr;
    const char* breakSoundPath = nullptr;
};

struct BrickState {
    uint32_t id;
    uint32_t health;
};

class Level {
  public:
    Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight);

    const uint32_t getBrickCount() const;
    void           load(const uint32_t& vertexBufferBindId, const uint32_t& instanceBufferBindId, const VkBuffer& vertexBuffer, const VkBuffer& indexBuffer,
                        Buffer& stagingBuffer, std::array<VkDrawIndexedIndirectCommand, 2>& drawCommands, VkBuffer& drawCommandsBuffer);

  private:
    const char* m_backgroundTexturePath = nullptr;

    uint32_t m_rowCount;
    uint32_t m_columnCount;
    uint32_t m_rowSpacing;
    uint32_t m_columnSpacing;

    uint32_t m_levelIndex;
    uint32_t m_brickCount;

    uint32_t m_windowWidth;
    uint32_t m_windowHeight;

    std::map<uint32_t, BrickType>        m_brickTypes;
    std::vector<std::vector<BrickState>> m_levelState;
    std::vector<Instance>                m_instances;

    std::unique_ptr<Buffer> m_instanceBuffer;

    void parseXml(const char* fullPath);
    void generateRenderData();
};
