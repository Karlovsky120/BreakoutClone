#pragma once

#include "level.h"

#define PI 3.1415926535897932384f

#define BALL_INDEX        0
#define LEFT_WALL_INDEX   1
#define RIGHT_WALL_INDEX  2
#define PAD_INDEX         3
#define BRICK_START_INDEX 4

#define VERTEX_BUFFER_BIND_ID   0
#define INSTANCE_BUFFER_BIND_ID 1

class Breakout {
  public:
    Breakout();

    void run();

  private:
    uint32_t m_squareVertexOffset;
    uint32_t m_squareIndexOffset;
    uint32_t m_squareIndexCount;

    uint32_t m_circleVertexOffset;
    uint32_t m_circleIndexOffset;
    uint32_t m_circleIndexCount;

    uint32_t m_drawCommandsSize;

    std::vector<Level> m_levels = {};

    std::array<VkDrawIndexedIndirectCommand, 2> m_drawCommands;

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;
    std::unique_ptr<Buffer> m_drawCommandsBuffer;

    void setupRenderer();
    void generateVertexAndIndexBuffers();
    void loadLevelData();
    void setupIndirectDrawCommands();
    void gameLoop();
};
