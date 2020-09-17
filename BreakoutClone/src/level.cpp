#include "level.h"

#include "renderer.h"

#pragma warning(push, 0)
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6319)  // Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.
#pragma warning(disable : 26495) // Variable * is uninitialized.Always initialize a member variable.
#include "tinyxml2.cpp"
#include "tinyxml2.h"
#pragma warning(enable : 26495)
#pragma warning(enable : 26451)
#pragma warning(enable : 6319)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>

const uint32_t Level::getBrickCount() const { return m_brickCount; }

void Level::load(const uint32_t& vertexBufferBindId, const uint32_t& instanceBufferBindId, const VkBuffer& vertexBuffer, const VkBuffer& indexBuffer,
                 const Buffer& stagingBuffer, std::array<VkDrawIndexedIndirectCommand, 2>& drawCommands, VkBuffer& drawCommandsBuffer) {
    drawCommands[1].instanceCount = BRICK_START_INDEX + m_brickCount;
    Resources::uploadToDeviceLocalBuffer(drawCommands.data(), VECTOR_SIZE_BYTES(drawCommands), stagingBuffer, drawCommandsBuffer, 0);

    Renderer::recordRenderCommandBuffers(vertexBuffer, vertexBufferBindId, indexBuffer, m_instanceBuffer->buffer, instanceBufferBindId, drawCommandsBuffer);
}

void Level::parseXml(const char* fullPath) {
    tinyxml2::XMLDocument levelXml;

    if (levelXml.LoadFile(fullPath)) {
        char error[512];
        sprintf_s(error, "Failed to open file at location %s", fullPath);
        throw std::runtime_error(error);
    }

    const tinyxml2::XMLElement* levelData = levelXml.FirstChild()->ToElement();

    levelData->FindAttribute("RowCount")->QueryUnsignedValue(&m_rowCount);
    levelData->FindAttribute("ColumnCount")->QueryUnsignedValue(&m_columnCount);
    levelData->FindAttribute("RowSpacing")->QueryUnsignedValue(&m_rowSpacing);
    levelData->FindAttribute("ColumnSpacing")->QueryUnsignedValue(&m_columnSpacing);
    m_backgroundTexturePath = levelData->FindAttribute("BackgroundTexture")->Value();

    m_brickCount = m_rowCount * m_columnCount;

    std::map<const std::string, uint32_t> idMap;
    idMap["_"]         = 0;
    uint32_t idCounter = 1;

    const tinyxml2::XMLElement* brickTypesNode = levelData->FirstChildElement("BrickTypes");

    for (const tinyxml2::XMLElement* brickTypeElement = brickTypesNode->FirstChildElement(); brickTypeElement != NULL;
         brickTypeElement                             = brickTypeElement->NextSiblingElement()) {
        BrickType brick;

        const std::string brickId = brickTypeElement->FindAttribute("Id")->Value();
        idMap[brickId]            = idCounter;
        brick.id                  = idCounter;
        ++idCounter;

        brick.texturePath     = brickTypeElement->FindAttribute("Texture")->Value();
        const char* hitPoints = brickTypeElement->FindAttribute("HitPoints")->Value();
        if (!strcmp(hitPoints, "Infinite")) {
            brick.hitPoints = UINT32_MAX;
        } else {
            brickTypeElement->FindAttribute("HitPoints")->QueryUnsignedValue(&brick.hitPoints);
        }

        brick.hitSoundPath = brickTypeElement->FindAttribute("HitSound")->Value();

        if (brickTypeElement->FindAttribute("BreakSound")) {
            brick.breakSoundPath = brickTypeElement->FindAttribute("BreakSound")->Value();
        }

        if (brickTypeElement->FindAttribute("BreakScore")) {
            brickTypeElement->FindAttribute("BreakScore")->QueryIntValue(&brick.breakScore);
        }

        m_brickTypes[brick.id] = brick;
    }

    std::string layoutData = levelData->FirstChildElement("Bricks")->GetText();
    layoutData.erase(std::remove(layoutData.begin(), layoutData.end(), '\t'), layoutData.end());
    uint32_t start = 0;
    uint32_t end   = static_cast<uint32_t>(layoutData.length()) - 1;
    if (layoutData[0] == '\n') {
        ++start;
    }
    if (layoutData[layoutData.length() - 1] == '\n') {
        --end;
    }
    layoutData = layoutData.substr(start, end);

    std::string       line;
    std::stringstream lineFeed(layoutData);
    while (std::getline(lineFeed, line, '\n')) {
        std::vector<BrickState> row;
        std::string             blockName;
        std::stringstream       wordFeed(line);
        while (std::getline(wordFeed, blockName, ' ')) {
            auto id = idMap.find(blockName);
            if (id != idMap.end()) {
                row.push_back({id->second, m_brickTypes[id->second].hitPoints});
            }
        }
        while (row.size() < m_columnCount) {
            row.push_back({0, 0});
        }

        m_levelState.push_back(row);
    }

    while (m_levelState.size() < m_rowCount) {
        std::vector<BrickState> row(m_columnCount, {0, 0});
        m_levelState.push_back(row);
    }
}

void Level::generateRenderData() {

    float     brickWidth    = (m_windowWidth - (MAX_COLUMN_COUNT + 1) * MAX_COLUMN_SPACING) / static_cast<float>(MAX_COLUMN_COUNT);
    float     bottomPadding = MAX_COLUMN_SPACING * 15;
    float     brickHeight   = (m_windowHeight - ((MAX_ROW_COUNT + 1) * MAX_ROW_SPACING) - bottomPadding) / static_cast<float>(MAX_ROW_COUNT);
    float     playAreaWidth = m_columnCount * brickWidth + (m_columnCount + 1) * m_columnSpacing;
    float     wallWidth     = (m_windowWidth - playAreaWidth) * 0.5f;
    float     ballRadius    = 0.5f * brickWidth;
    glm::vec2 padDimensions = {playAreaWidth * 0.2f, brickHeight};

    float padOffset = m_windowHeight - MAX_ROW_SPACING * 2.0f;

#pragma warning(suppress : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value
    m_instances = std::vector<Instance>(BRICK_START_INDEX + m_brickCount);

    // The ball
    m_instances[BALL_INDEX].position.x   = wallWidth + playAreaWidth * 0.5f;
    m_instances[BALL_INDEX].position.y   = padOffset - (0.5f * padDimensions.y + ballRadius + 1.0f);
    m_instances[BALL_INDEX].position.z   = 0.0f;
    m_instances[BALL_INDEX].scale        = {2.0f * ballRadius, 2.0f * ballRadius};
    m_instances[BALL_INDEX].textureIndex = 1;
    m_instances[BALL_INDEX].health       = UINT32_MAX;

    // Left wall
    m_instances[LEFT_WALL_INDEX].position.x   = 0;
    m_instances[LEFT_WALL_INDEX].position.y   = m_windowHeight * 0.5f;
    m_instances[LEFT_WALL_INDEX].position.z   = 0.0f;
    m_instances[LEFT_WALL_INDEX].scale        = {wallWidth * 2.0f, m_windowHeight};
    m_instances[LEFT_WALL_INDEX].textureIndex = 2;
    m_instances[LEFT_WALL_INDEX].health       = UINT32_MAX;

    // Right wall
    m_instances[RIGHT_WALL_INDEX].position.x   = static_cast<float>(m_windowWidth);
    m_instances[RIGHT_WALL_INDEX].position.y   = m_windowHeight * 0.5f;
    m_instances[RIGHT_WALL_INDEX].position.z   = 0.0f;
    m_instances[RIGHT_WALL_INDEX].scale        = {wallWidth * 2.0f, m_windowHeight};
    m_instances[RIGHT_WALL_INDEX].textureIndex = 2;
    m_instances[RIGHT_WALL_INDEX].health       = UINT32_MAX;

    // The pad
    m_instances[PAD_INDEX].position.x   = wallWidth + playAreaWidth * 0.5f;
    m_instances[PAD_INDEX].position.y   = padOffset;
    m_instances[PAD_INDEX].position.z   = 0.0f;
    m_instances[PAD_INDEX].scale        = padDimensions;
    m_instances[PAD_INDEX].textureIndex = 0;
    m_instances[PAD_INDEX].health       = UINT32_MAX;

    // Bricks
    uint32_t instanceDataIndex = BRICK_START_INDEX;
    float    offsetY           = m_rowSpacing + 0.5f * brickHeight;
    float    stepY             = m_rowSpacing + brickHeight;
    for (size_t i = 0; i < m_levelState.size(); ++i, offsetY += stepY) {
        std::vector<BrickState>& brickRow = m_levelState[i];
        float                    offsetX  = wallWidth + m_columnSpacing + 0.5f * brickWidth;
        float                    stepX    = m_columnSpacing + brickWidth;
        for (size_t j = 0; j < brickRow.size(); ++j, offsetX += stepX) {
            m_instances[instanceDataIndex].position     = {offsetX, offsetY, 0.0f};
            m_instances[instanceDataIndex].scale        = {brickWidth, brickHeight};
            m_instances[instanceDataIndex].textureIndex = 2;
            m_instances[instanceDataIndex].health       = 1;
            ++instanceDataIndex;
        }
    }

    uint32_t instanceDataBufferSize = VECTOR_SIZE_BYTES(m_instances);
    m_instanceBuffer                = Resources::createBuffer(instanceDataBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    Resources::uploadToHostVisibleBuffer(m_instances.data(), instanceDataBufferSize, m_instanceBuffer->memory);
}

Level::Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight)
    : m_levelIndex(levelIndex), m_windowWidth(windowWidth), m_windowHeight(windowHeight) {
    parseXml(fullPath);
    generateRenderData();
}
