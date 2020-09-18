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

void Breakout::run() {
    m_levels[0].load(m_vertexBuffer.buffer, m_indexBuffer.buffer, m_drawCommands, m_drawCommandsBuffer.buffer);
    gameLoop();
}

Breakout::Breakout() {
    Renderer::initRenderer();
    generateVertexAndIndexBuffers();
    setupIndirectDrawCommands();
    loadLevelData();
}

Breakout::~Breakout() {
    m_drawCommandsBuffer.destroy();
    m_indexBuffer.destroy();
    m_vertexBuffer.destroy();

    for (Level& level : m_levels) {
        level.destroy();
    }

    Renderer::getInstance()->destroy();
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
    Resources::uploadToDeviceLocalBuffer(squareVertices.data(), squareVertexBufferSize, m_vertexBuffer.buffer, squareVertexBufferOffset);

    m_circleVertexOffset              = m_squareVertexOffset + static_cast<uint32_t>(squareVertices.size());
    uint32_t circleVertexBufferOffset = m_squareVertexOffset + VECTOR_SIZE_BYTES(squareVertices);
    uint32_t circleVertexBufferSize   = VECTOR_SIZE_BYTES(circleVertices);
    Resources::uploadToDeviceLocalBuffer(circleVertices.data(), circleVertexBufferSize, m_vertexBuffer.buffer, circleVertexBufferOffset);

    uint32_t indexBufferSize = sizeof(uint16_t) * (static_cast<uint32_t>(squareIndices.size() + circleIndices.size()));
    m_indexBuffer =
        Resources::createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_squareIndexOffset              = 0;
    uint32_t squareIndexBufferOffset = 0;
    uint32_t squareIndexBuffferSize  = VECTOR_SIZE_BYTES(squareIndices);
    Resources::uploadToDeviceLocalBuffer(squareIndices.data(), squareIndexBuffferSize, m_indexBuffer.buffer, squareIndexBufferOffset);

    m_circleIndexOffset              = m_squareIndexOffset + static_cast<uint32_t>(squareIndices.size());
    uint32_t circleIndexBufferOffset = m_squareIndexOffset + VECTOR_SIZE_BYTES(squareIndices);
    uint32_t circleIndexBufferSize   = VECTOR_SIZE_BYTES(circleIndices);
    Resources::uploadToDeviceLocalBuffer(circleIndices.data(), circleIndexBufferSize, m_indexBuffer.buffer, circleIndexBufferOffset);
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

void Breakout::gameLoop() {
    bool updatedUI = false;

    std::chrono::high_resolution_clock::time_point oldTime = std::chrono::high_resolution_clock::now();
    uint32_t                                       time    = 0;

    SDL_Event sdlEvent;
    bool      quit = false;

    while (!quit) {
        while (SDL_PollEvent(&sdlEvent)) {
            switch (sdlEvent.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                // m_keyStates[sdlEvent.key.keysym.sym].pressed = true;
                break;
            case SDL_KEYUP:
                // m_keyStates[sdlEvent.key.keysym.sym].pressed = false;
                break;
            }
        }

        Renderer::getInstance()->acquireImage();

        std::chrono::high_resolution_clock::time_point newTime = std::chrono::high_resolution_clock::now();
        uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(newTime - oldTime).count());
        oldTime            = newTime;
        time += frameTime;

        if (time > 500'000 || updatedUI) {
            char title[256];
            sprintf_s(title, "Breakout! Frametime: %.2fms", frameTime / 1'000.0f);
            Renderer::setWindowTitle(title);
            time      = 0;
            updatedUI = false;
        }

        Renderer::getInstance()->renderAndPresentImage();
    }

    vkDeviceWaitIdle(Renderer::getDevice());
}
