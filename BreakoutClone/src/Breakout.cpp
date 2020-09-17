#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "renderer.h"
#include "resources.h"
#include "sharedStructures.h"

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/gtx/rotate_vector.hpp"
#pragma warning(pop)

#include <array>
#include <filesystem>

#define PI 3.1415926535897932384f

#define BALL_INDEX        0
#define LEFT_WALL_INDEX   1
#define RIGHT_WALL_INDEX  2
#define PAD_INDEX         3
#define BRICK_START_INDEX 4

#define VERTEX_BUFFER_BIND_ID   0
#define INSTANCE_BUFFER_BIND_ID 1

void Breakout::setupRenderer() {
    std::array<VkVertexInputBindingDescription, 2> vertexInputBindingDescriptions;
    vertexInputBindingDescriptions[0].binding   = VERTEX_BUFFER_BIND_ID;
    vertexInputBindingDescriptions[0].stride    = sizeof(Vertex);
    vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexInputBindingDescriptions[1].binding   = INSTANCE_BUFFER_BIND_ID;
    vertexInputBindingDescriptions[1].stride    = sizeof(Instance);
    vertexInputBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 5> vertexInputAttributeDescriptions;
    // Vertex position
    vertexInputAttributeDescriptions[0].binding  = VERTEX_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[0].location = 0;
    vertexInputAttributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset   = 0;

    // Instance position
    vertexInputAttributeDescriptions[1].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[1].location = 1;
    vertexInputAttributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset   = 0;

    // Instance scale
    vertexInputAttributeDescriptions[2].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[2].location = 2;
    vertexInputAttributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[2].offset   = offsetof(Instance, scale);

    // Instance texture index
    vertexInputAttributeDescriptions[3].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[3].location = 3;
    vertexInputAttributeDescriptions[3].format   = VK_FORMAT_R32_UINT;
    vertexInputAttributeDescriptions[3].offset   = offsetof(Instance, textureIndex);

    // Instance health
    vertexInputAttributeDescriptions[4].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[4].location = 4;
    vertexInputAttributeDescriptions[4].format   = VK_FORMAT_R32_UINT;
    vertexInputAttributeDescriptions[4].offset   = offsetof(Instance, health);

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputStateCreateInfo.pVertexBindingDescriptions           = vertexInputBindingDescriptions.data();
    vertexInputStateCreateInfo.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertexInputBindingDescriptions.size());
    vertexInputStateCreateInfo.pVertexAttributeDescriptions         = vertexInputAttributeDescriptions.data();
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());

    Renderer::initRenderer("src/shaders/spirv/vertexShader.spv", "src/shaders/spirv/fragmentShader.spv", vertexInputStateCreateInfo);
}

void Breakout::generateVertexAndIndexBuffers() {
    std::vector<Vertex>   squareVertices = {{{-0.5, -0.5}}, {{-0.5, 0.5}}, {{0.5, -0.5}}, {{0.5, 0.5}}};
    std::vector<uint16_t> squareIndices  = {0, 1, 2, 1, 3, 2};
    m_squareIndexCount                   = static_cast<uint32_t>(squareIndices.size());

    std::vector<Vertex>   circleVertices;
    std::vector<uint16_t> circleIndices;

    Vertex   currentVertex     = {{0.5, 0.0}};
    uint32_t circleVertexCount = 36;
    float    step              = -2.0f * PI / circleVertexCount;
    uint16_t currentIndex      = 1;

    circleVertices.push_back(currentVertex);
    for (uint32_t i = 0; i < circleVertexCount; ++i) {
        currentVertex.position = glm::rotate(currentVertex.position, step);
        circleVertices.push_back(currentVertex);

        circleIndices.push_back(0);
        circleIndices.push_back(currentIndex);
        ++currentIndex;
        circleIndices.push_back(currentIndex);
    }

    circleIndices.pop_back();
    circleIndices.push_back(1);
    m_circleIndexCount = static_cast<uint32_t>(circleIndices.size());

    uint32_t vertexBufferSize = sizeof(Vertex) * (static_cast<uint32_t>(squareVertices.size() + circleVertices.size()));
    m_vertexBuffer =
        Resources::createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_squareVertexOffset              = 0;
    uint32_t squareVertexBufferOffset = 0;
    uint32_t squareVertexBufferSize   = VECTOR_SIZE_BYTES(squareVertices);
    Resources::uploadToDeviceLocalBuffer(squareVertices.data(), squareVertexBufferSize, *m_stagingBuffer, m_vertexBuffer->buffer, squareVertexBufferOffset);

    m_circleVertexOffset              = m_squareVertexOffset + static_cast<uint32_t>(squareVertices.size());
    uint32_t circleVertexBufferOffset = m_squareVertexOffset + VECTOR_SIZE_BYTES(squareVertices);
    uint32_t circleVertexBufferSize   = VECTOR_SIZE_BYTES(circleVertices);
    Resources::uploadToDeviceLocalBuffer(circleVertices.data(), circleVertexBufferSize, *m_stagingBuffer, m_vertexBuffer->buffer, circleVertexBufferOffset);

    uint32_t indexBufferSize = sizeof(uint16_t) * (static_cast<uint32_t>(squareIndices.size() + circleIndices.size()));
    m_indexBuffer =
        Resources::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_squareIndexOffset              = 0;
    uint32_t squareIndexBufferOffset = 0;
    uint32_t squareIndexBuffferSize  = VECTOR_SIZE_BYTES(squareIndices);
    Resources::uploadToDeviceLocalBuffer(squareIndices.data(), squareIndexBuffferSize, *m_stagingBuffer, m_indexBuffer->buffer, squareIndexBufferOffset);

    m_circleIndexOffset              = m_squareIndexOffset + static_cast<uint32_t>(squareIndices.size());
    uint32_t circleIndexBufferOffset = m_squareIndexOffset + VECTOR_SIZE_BYTES(squareIndices);
    uint32_t circleIndexBufferSize   = VECTOR_SIZE_BYTES(circleIndices);
    Resources::uploadToDeviceLocalBuffer(circleIndices.data(), circleIndexBufferSize, *m_stagingBuffer, m_indexBuffer->buffer, circleIndexBufferOffset);
}

void Breakout::loadLevelData() {
    std::filesystem::path fullPath = std::filesystem::current_path();
    fullPath += "\\resources\\levels\\";
    uint32_t counter = 0;
    for (const auto& file : std::filesystem::directory_iterator(fullPath)) {
        m_levels.push_back(Level(file.path().string().c_str(), counter, WINDOW_WIDTH, WINDOW_HEIGHT));
        ++counter;
    }
}

void Breakout::setupIndirectDrawCommands() {
    // Ball
    m_drawCommands[0].indexCount    = static_cast<uint32_t>(m_squareIndexCount); // static_cast<uint32_t>(m_circleIndexCount);
    m_drawCommands[0].instanceCount = 1;
    m_drawCommands[0].firstIndex    = m_squareIndexOffset;  // m_circleIndexOffset;
    m_drawCommands[0].vertexOffset  = m_squareVertexOffset; // m_circleVertexOffset;
    m_drawCommands[0].firstInstance = 0;

    // Pad, walls and bricks
    m_drawCommands[1].indexCount    = static_cast<uint32_t>(m_squareIndexCount);
    m_drawCommands[1].instanceCount = 0; // this needs to be updated on level load!
    m_drawCommands[1].firstIndex    = m_squareIndexOffset;
    m_drawCommands[1].vertexOffset  = m_squareVertexOffset;
    m_drawCommands[1].firstInstance = 1;

    m_drawCommandsBuffer = Resources::createBuffer(VECTOR_SIZE_BYTES(m_drawCommands), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

Breakout::Breakout() {
    setupRenderer();

    m_stagingBuffer = Resources::createBuffer(STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    generateVertexAndIndexBuffers();
    setupIndirectDrawCommands();
    loadLevelData();
}

void Breakout::run() {
    m_levels[0].load(VERTEX_BUFFER_BIND_ID, INSTANCE_BUFFER_BIND_ID, m_vertexBuffer->buffer, m_indexBuffer->buffer, *m_stagingBuffer, m_drawCommands,
                     m_drawCommandsBuffer->buffer);
    Renderer::runRenderLoop();
}
