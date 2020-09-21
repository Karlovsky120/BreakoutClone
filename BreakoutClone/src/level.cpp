#include "level.h"

#include "renderer.h"
#include "resources.h"

// This generally silences all warnings in the external file, but it doesn't work for warning bellow.
#pragma warning(push, 0)
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6319)  // Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
#pragma warning(disable : 26495) // Variable * is uninitialized.Always initialize a member variable.
#pragma warning(disable : 28182) // Dereferencing NULL pointer '*' contains the same NULL value as '*'.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.

#include "tinyxml2.cpp"
#include "tinyxml2.h"

#pragma warning(enable : 26451)
#pragma warning(enable : 28182)
#pragma warning(enable : 26495)
#pragma warning(enable : 6319)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include "common.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

void Level::load() {
    Renderer::updateTextureArray();
    Renderer::recordRenderCommandBuffers(m_instanceBuffer.buffer, static_cast<uint32_t>(m_instances.size()));
}

void Level::updateGPUData() const { Resources::uploadToHostVisibleBuffer(m_instances.data(), m_instanceDataBufferSize, m_instanceBuffer.memory); }

std::vector<Instance>& Level::getInstances() { return m_instances; }
const glm::vec2        Level::getWindowDimensions() const { return {m_windowWidth, m_windowHeight}; }
const float&           Level::getBasePadSpeed() const { return m_basePadSpeed; }
const float&           Level::getBaseBallSpeed() const { return m_baseBallSpeed; }
const uint32_t&        Level::getTotalBrickCount() const { return m_brickCount; }
const uint32_t&        Level::getRemainingBrickCount() const { return m_remainingBrickCount; }
glm::vec2&             Level::getBallDirection() { return m_ballDirection; }
Instance&              Level::getForeground() { return m_instances[m_foregroundIndex]; }
void                   Level::resetPadAndBall() {
    m_instances[PAD_INDEX].position  = m_padInitialPosition;
    m_instances[BALL_INDEX].position = m_ballInitialPosition;
}

const uint32_t& Level::loseLife() {
    --m_remainingLivesCount;
    return m_remainingLivesCount;
}

const uint32_t& Level::destroyBrick() {
    --m_remainingBrickCount;
    return m_remainingBrickCount;
}

Level::Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight)
    : m_levelIndex(levelIndex), m_windowWidth(windowWidth), m_windowHeight(windowHeight) {
    parseXml(fullPath);
    generateRenderData();
}

void Level::destroy() { m_instanceBuffer.destroy(); }

void Level::parseXml(const char* fullPath) {
    tinyxml2::XMLDocument levelXml;

    if (levelXml.LoadFile(fullPath)) {
        char error[512];
        sprintf_s(error, "Failed to open file at location %s!", fullPath);
        throw std::runtime_error(error);
    }

    const tinyxml2::XMLElement* levelData = levelXml.FirstChild()->ToElement();

    levelData->FindAttribute("RowCount")->QueryUnsignedValue(&m_rowCount);
    levelData->FindAttribute("ColumnCount")->QueryUnsignedValue(&m_columnCount);
    levelData->FindAttribute("RowSpacing")->QueryUnsignedValue(&m_rowSpacing);
    levelData->FindAttribute("ColumnSpacing")->QueryUnsignedValue(&m_columnSpacing);
    m_backgroundTexturePath = levelData->FindAttribute("BackgroundTexture")->Value();

    m_brickCount = m_rowCount * m_columnCount;

    std::map<const std::string, uint32_t> idNameMap;
    idNameMap["_"]     = 0;
    uint32_t idCounter = 1;

    const tinyxml2::XMLElement* brickTypesNode = levelData->FirstChildElement("BrickTypes");

    for (const tinyxml2::XMLElement* brickTypeElement = brickTypesNode->FirstChildElement(); brickTypeElement != NULL;
         brickTypeElement                             = brickTypeElement->NextSiblingElement()) {
        BrickType brick;

        const std::string brickId = brickTypeElement->FindAttribute("Id")->Value();
        idNameMap[brickId]        = idCounter;
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
        std::vector<uint32_t> row;
        std::string           blockName;
        std::stringstream     wordFeed(line);
        while (std::getline(wordFeed, blockName, ' ')) {
            auto id = idNameMap.find(blockName);
            if (id != idNameMap.end()) {
                row.push_back(id->second);
            }
        }
        while (row.size() < m_columnCount) {
            row.push_back(0);
        }

        m_levelLayout.push_back(row);
    }

    while (m_levelLayout.size() < m_rowCount) {
        std::vector<uint32_t> row(m_columnCount, 0);
        m_levelLayout.push_back(row);
    }
}

void Level::generateRenderData() {
    float brickWidth    = (m_windowWidth - (MAX_COLUMN_COUNT + 1) * MAX_COLUMN_SPACING) / static_cast<float>(MAX_COLUMN_COUNT);
    float bottomPadding = MAX_COLUMN_SPACING * 15;
    float brickHeight   = (m_windowHeight - ((MAX_ROW_COUNT + 1) * MAX_ROW_SPACING) - bottomPadding) / static_cast<float>(MAX_ROW_COUNT);
    float playAreaWidth = m_columnCount * brickWidth + (m_columnCount + 1) * m_columnSpacing;
    float wallWidth     = (m_windowWidth - playAreaWidth) * 0.5f;
    float ballRadius    = 0.375f * brickWidth;
    float padOffset     = m_windowHeight - MAX_ROW_SPACING * 2.0f;

    glm::vec2 padDimensions = {playAreaWidth * 0.2f, brickHeight};

    m_basePadSpeed        = PAD_SPEED_FACTOR / playAreaWidth;
    m_baseBallSpeed       = BALL_SPEED_FACTOR / playAreaWidth;
    m_remainingLivesCount = LIVES_COUNT;

    Instance defaultInstance;
    defaultInstance.id           = UINT32_MAX;
    defaultInstance.position     = {0.0f, 0.0f};
    defaultInstance.depth        = GAME_DEPTH;
    defaultInstance.scale        = {1.0f, 1.0f};
    defaultInstance.textureIndex = 0;
    defaultInstance.textureAlpha = 1.0f;
    defaultInstance.uvOffset     = {0.0f, 0.0f};
    defaultInstance.uvScale      = {1.0f, 1.0f};
    defaultInstance.maxHealth    = UINT32_MAX;
    defaultInstance.health       = UINT32_MAX;

#pragma warning(suppress : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value
    m_instances = std::vector<Instance>(BRICK_START_INDEX + m_brickCount + 1, defaultInstance);

    // Background
    m_instances[BACKGROUND_INDEX].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[BACKGROUND_INDEX].depth        = BACKGROUND_DEPTH;
    m_instances[BACKGROUND_INDEX].scale        = {m_windowWidth, m_windowHeight};
    m_instances[BACKGROUND_INDEX].textureIndex = Resources::getTextureId(m_backgroundTexturePath);

    // Left wall
    m_instances[LEFT_WALL_INDEX].position     = {wallWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[LEFT_WALL_INDEX].depth        = GAME_DEPTH;
    m_instances[LEFT_WALL_INDEX].scale        = {wallWidth, m_windowHeight};
    m_instances[LEFT_WALL_INDEX].textureIndex = Resources::getTextureId(m_backgroundTexturePath, SIDE_BLUR_STRENGTH);
    m_instances[LEFT_WALL_INDEX].uvOffset     = {0.0f, 0.0f};
    m_instances[LEFT_WALL_INDEX].uvScale      = {wallWidth / static_cast<float>(m_windowWidth), 1.0f};

    // Right wall
    m_instances[RIGHT_WALL_INDEX].position     = {static_cast<float>(m_windowWidth) - wallWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[RIGHT_WALL_INDEX].depth        = GAME_DEPTH;
    m_instances[RIGHT_WALL_INDEX].scale        = {wallWidth, m_windowHeight};
    m_instances[RIGHT_WALL_INDEX].textureIndex = Resources::getTextureId(m_backgroundTexturePath, SIDE_BLUR_STRENGTH);
    m_instances[RIGHT_WALL_INDEX].uvOffset     = {m_windowWidth - wallWidth / static_cast<float>(m_windowWidth), 0.0f};
    m_instances[RIGHT_WALL_INDEX].uvScale      = {wallWidth / static_cast<float>(m_windowWidth), 1.0f};

    // The pad
    m_padInitialPosition                = {wallWidth + playAreaWidth * 0.5f, padOffset};
    m_instances[PAD_INDEX].position     = m_padInitialPosition;
    m_instances[PAD_INDEX].depth        = GAME_DEPTH;
    m_instances[PAD_INDEX].scale        = padDimensions;
    m_instances[PAD_INDEX].textureIndex = Resources::getTextureId(PAD_TEXTURE_PATH);

    // The ball
    m_ballInitialPosition                = {m_windowWidth * 0.5f, padOffset - (0.5f * padDimensions.y + ballRadius + 1.0f)};
    m_instances[BALL_INDEX].position     = m_ballInitialPosition;
    m_instances[BALL_INDEX].depth        = GAME_DEPTH;
    m_instances[BALL_INDEX].scale        = {2.0f * ballRadius, 2.0f * ballRadius};
    m_instances[BALL_INDEX].textureIndex = Resources::getTextureId(BALL_TEXTURE_PATH);

    // Bricks
    uint32_t instanceDataIndex = BRICK_START_INDEX;
    float    offsetY           = m_rowSpacing + 0.5f * brickHeight;
    float    stepY             = m_rowSpacing + brickHeight;
    for (size_t i = 0; i < m_levelLayout.size(); ++i, offsetY += stepY) {
        std::vector<uint32_t>& brickRow = m_levelLayout[i];
        float                  offsetX  = wallWidth + m_columnSpacing + 0.5f * brickWidth;
        float                  stepX    = m_columnSpacing + brickWidth;
        for (size_t j = 0; j < brickRow.size(); ++j, offsetX += stepX) {
            const uint32_t brickMaxHealth               = m_brickTypes[m_levelLayout[i][j]].hitPoints;
            m_instances[instanceDataIndex].position     = {offsetX, offsetY};
            m_instances[instanceDataIndex].depth        = GAME_DEPTH;
            m_instances[instanceDataIndex].scale        = {brickWidth, brickHeight};
            m_instances[instanceDataIndex].textureIndex = Resources::getTextureId(m_brickTypes[m_levelLayout[i][j]].texturePath);
            m_instances[instanceDataIndex].health       = brickMaxHealth;
            m_instances[instanceDataIndex].maxHealth    = brickMaxHealth;

            if (brickMaxHealth > 0 && brickMaxHealth < UINT32_MAX) {
                ++m_remainingBrickCount;
            }

            ++instanceDataIndex;
        }
    }

    // Foreground
    m_foregroundIndex                           = instanceDataIndex;
    m_instances[m_foregroundIndex].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[m_foregroundIndex].depth        = FOREGROUND_DEPTH;
    m_instances[m_foregroundIndex].scale        = {m_windowWidth, m_windowHeight};
    m_instances[m_foregroundIndex].textureIndex = Resources::getTextureId(FOREGROUND_TEXTURE_PATH);
    m_instances[m_foregroundIndex].textureAlpha = 0.0f;

    m_instanceDataBufferSize = VECTOR_SIZE_IN_BYTES(m_instances);
    m_instanceBuffer         = Resources::createBuffer(m_instanceDataBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    Renderer::nameObject(&m_instanceBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "Instance buffer");

    Resources::uploadToHostVisibleBuffer(m_instances.data(), m_instanceDataBufferSize, m_instanceBuffer.memory);
}
