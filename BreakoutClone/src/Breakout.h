#pragma once

#include "level.h"

class Breakout {
  public:
    void run();

    Breakout();
    ~Breakout();

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

    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    Buffer m_drawCommandsBuffer;

    void generateVertexAndIndexBuffers();
    void loadLevelData();
    void setupIndirectDrawCommands();
    void gameLoop();
};
