#pragma once

#include "level.h"

#define STAGING_BUFFER_SIZE 33'554'432 // 32MB

class Breakout {
  public:
    Breakout();

    void run();

  private:
    std::unique_ptr<Buffer> m_stagingBuffer;

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
};
