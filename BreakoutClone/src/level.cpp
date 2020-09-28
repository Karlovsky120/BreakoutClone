#include "level.h"

#include "renderer.h"
#include "textureManager.h"

// This generally silences all warnings in the external file, but it doesn't work for warning bellow.
#pragma warning(push, 0)
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6319)  // Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.
#pragma warning(disable : 26495) // Variable * is uninitialized.Always initialize a member variable.
#pragma warning(disable : 26819) // Dereferencing NULL pointer.'*' contains the same NULL value as '*' did.
#pragma warning(disable : 28182) // Dereferencing NULL pointer '*' contains the same NULL value as '*'.

#include "tinyxml2.cpp"
#include "tinyxml2.h"

#pragma warning(enable : 28182)
#pragma warning(enable : 26819)
#pragma warning(enable : 26495)
#pragma warning(enable : 26451)
#pragma warning(enable : 6319)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include "common.h"

#include <algorithm>
#include <sstream>

Level::Level(const char* levelPath, const uint32_t& windowWidth, const uint32_t& windowHeight, Renderer* const renderer, TextureManager* const textureManager)
    : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_renderer(renderer), m_textureManager(textureManager) {

    parseXml(levelPath);
    generateRenderData();
}

Level::~Level() { m_renderer->waitIdle(); }

void Level::load(const uint32_t& lifeCount, const uint32_t& score, const uint32_t& levelIndex) {
    m_inUse = m_backup;

    m_renderer->uploadToHostVisibleBuffer(m_inUse.instances.data(), m_instanceDataBufferSize, m_instanceBuffer->memory);
    m_renderer->updateTextureArray(m_textureManager->getTextureArray());
    m_renderer->recordRenderCommandBuffers(m_instanceBuffer->buffer, static_cast<uint32_t>(m_inUse.instances.size()));

    setNumber(m_levelCountStartIndex, LEVEL_COUNT_DIGITS, levelIndex);
    setLifeCount(lifeCount);
    setScore(score);
}

void Level::updateGPUData() const { m_renderer->uploadToHostVisibleBuffer(m_inUse.instances.data(), m_instanceDataBufferSize, m_instanceBuffer->memory); }

void Level::setForegroundVisibility(const float& alpha) { m_inUse.instances[m_foregroundIndex].textureAlpha = alpha; }

void Level::setTitleVisibility(const float& alpha) { m_inUse.instances[m_titleIndex].textureAlpha = alpha; }

void Level::setTitle(const std::string& textureId) { m_inUse.instances[m_titleIndex].textureIndex = m_textureManager->getTextureId(textureId); }

void Level::setSubtitleVisibility(const float& alpha) { m_inUse.instances[m_subtitleIndex].textureAlpha = alpha; }

void Level::setSubtitle(const std::string& textureId) { m_inUse.instances[m_subtitleIndex].textureIndex = m_textureManager->getTextureId(textureId); }

void Level::setScore(const uint32_t& score) { setNumber(m_scoreCountStartIndex, SCORE_COUNT_DIGITS, score); }

void Level::setLifeCount(const uint32_t& lifeCount) { setNumber(m_livesCountStartIndex, LIFE_COUNT_DIGITS, lifeCount); }

std::vector<Instance>& Level::getInstances() { return m_inUse.instances; };

const BrickType& Level::getBrickData(const uint32_t& id) const { return m_brickTypes.find(id)->second; }

const uint32_t& Level::getRemainingBrickCount() const { return m_inUse.remainingBrickCount; }

const uint32_t& Level::getTotalBrickCount() const { return m_totalBrickCount; }

Instance* const Level::getBricksPtr() { return &m_inUse.instances[BRICK_START_INDEX]; }

const uint32_t& Level::destroyBrick() {
    --m_inUse.remainingBrickCount;
    return m_inUse.remainingBrickCount;
}

const glm::vec2 Level::getWindowDimensions() const { return {m_windowWidth, m_windowHeight}; }

const uint32_t& Level::getBallIndex() const { return m_ballIndex; }

const glm::vec2 Level::getStartingBallDirection() const {
    float padXOffset = m_inUse.instances[PAD_INDEX].position.x - m_wallWidth - m_playAreaWidth * 0.5f;
    return glm::normalize(glm::vec2(padXOffset * 1.2f, -m_playAreaWidth));
}

const float& Level::getBasePadSpeed() const { return m_basePadSpeed; }

const float& Level::getBaseBallSpeed() const { return m_baseBallSpeed; }

void Level::resetPadAndBall() {
    m_inUse.instances[PAD_INDEX].position   = m_padInitialPosition;
    m_inUse.instances[m_ballIndex].position = m_ballInitialPosition;
}

void Level::setNumber(const uint32_t& instanceIndex, const uint32_t& digitCount, uint32_t number) {
    for (size_t i = 0; i < digitCount; ++i) {
        m_inUse.instances[instanceIndex - i - 1 + digitCount].textureIndex = m_textureManager->getTextureId(TEXTURE_UI_NUMBER(number % 10));
        number /= 10;
    }
}

void Level::parseXml(const char* fullLevelPath) {
    tinyxml2::XMLDocument levelXml;

    if (levelXml.LoadFile(fullLevelPath)) {
        char error[512];
        sprintf_s(error, "Failed to open file at location %s!", fullLevelPath);
        throw std::runtime_error(error);
    }

    const tinyxml2::XMLElement* levelData = levelXml.FirstChild()->ToElement();

    levelData->FindAttribute("RowCount")->QueryUnsignedValue(&m_rowCount);
    levelData->FindAttribute("ColumnCount")->QueryUnsignedValue(&m_columnCount);
    levelData->FindAttribute("RowSpacing")->QueryUnsignedValue(&m_rowSpacing);
    levelData->FindAttribute("ColumnSpacing")->QueryUnsignedValue(&m_columnSpacing);
    m_backgroundTexturePath = levelData->FindAttribute("BackgroundTexture")->Value();

    m_totalBrickCount = m_rowCount * m_columnCount;

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

        if (brickTypeElement->FindAttribute("HitSound")) {
            brick.hitSoundPath = brickTypeElement->FindAttribute("HitSound")->Value();
        }

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
    while (layoutData[end] == '\n' || layoutData[end] == ' ') {
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
    m_playAreaWidth     = m_columnCount * brickWidth + (m_columnCount + 1) * m_columnSpacing;
    m_wallWidth         = (m_windowWidth - m_playAreaWidth) * 0.5f;
    float ballRadius    = 0.375f * brickWidth;
    float padOffset     = m_windowHeight - MAX_ROW_SPACING * 2.0f;

    glm::vec2 padDimensions = {m_playAreaWidth * 0.2f, brickHeight};

    m_basePadSpeed  = PAD_SPEED_FACTOR * m_playAreaWidth;
    m_baseBallSpeed = BALL_SPEED_FACTOR * m_playAreaWidth;

    Instance defaultInstance;
    defaultInstance.id           = UINT32_MAX;
    defaultInstance.position     = {0.0f, 0.0f};
    defaultInstance.depth        = DEPTH_GAME;
    defaultInstance.scale        = {1.0f, 1.0f};
    defaultInstance.textureIndex = 0;
    defaultInstance.textureAlpha = 1.0f;
    defaultInstance.uvOffset     = {0.0f, 0.0f};
    defaultInstance.uvScale      = {1.0f, 1.0f};
    defaultInstance.maxHealth    = UINT32_MAX;
    defaultInstance.health       = UINT32_MAX;

    uint32_t totalUiCount = 5 + LEVEL_COUNT_DIGITS + LIFE_COUNT_DIGITS + SCORE_COUNT_DIGITS;
#pragma warning(suppress : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value
    m_backup.instances = std::vector<Instance>(BRICK_START_INDEX + m_totalBrickCount + 2 + totalUiCount, defaultInstance);

    // Background
    m_backup.instances[BACKGROUND_INDEX].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.5f};
    m_backup.instances[BACKGROUND_INDEX].depth        = DEPTH_BACKGROUND;
    m_backup.instances[BACKGROUND_INDEX].scale        = {m_windowWidth, m_windowHeight};
    m_backup.instances[BACKGROUND_INDEX].textureIndex = m_textureManager->getTextureId(m_backgroundTexturePath);

    // Left wall
    m_backup.instances[LEFT_WALL_INDEX].position     = {m_wallWidth * 0.5f, m_windowHeight * 0.5f};
    m_backup.instances[LEFT_WALL_INDEX].depth        = DEPTH_GAME;
    m_backup.instances[LEFT_WALL_INDEX].scale        = {m_wallWidth, m_windowHeight};
    m_backup.instances[LEFT_WALL_INDEX].textureIndex = m_textureManager->getTextureId(m_backgroundTexturePath, SIDE_BLUR_STRENGTH);
    m_backup.instances[LEFT_WALL_INDEX].uvOffset     = {0.0f, 0.0f};
    m_backup.instances[LEFT_WALL_INDEX].uvScale      = {m_wallWidth / static_cast<float>(m_windowWidth), 1.0f};

    // Right wall
    m_backup.instances[RIGHT_WALL_INDEX].position     = {static_cast<float>(m_windowWidth) - m_wallWidth * 0.5f, m_windowHeight * 0.5f};
    m_backup.instances[RIGHT_WALL_INDEX].depth        = DEPTH_GAME;
    m_backup.instances[RIGHT_WALL_INDEX].scale        = {m_wallWidth, m_windowHeight};
    m_backup.instances[RIGHT_WALL_INDEX].textureIndex = m_textureManager->getTextureId(m_backgroundTexturePath, SIDE_BLUR_STRENGTH);
    m_backup.instances[RIGHT_WALL_INDEX].uvOffset     = {m_windowWidth - m_wallWidth / static_cast<float>(m_windowWidth), 0.0f};
    m_backup.instances[RIGHT_WALL_INDEX].uvScale      = {m_wallWidth / static_cast<float>(m_windowWidth), 1.0f};

    // The pad
    m_padInitialPosition                       = {m_wallWidth + m_playAreaWidth * 0.5f, padOffset};
    m_backup.instances[PAD_INDEX].position     = m_padInitialPosition;
    m_backup.instances[PAD_INDEX].depth        = DEPTH_GAME;
    m_backup.instances[PAD_INDEX].scale        = padDimensions;
    m_backup.instances[PAD_INDEX].textureIndex = m_textureManager->getTextureId(TEXTURE_PAD);

    // Bricks
    uint32_t instanceDataIndex = BRICK_START_INDEX;
    float    offsetY           = m_rowSpacing + 0.5f * brickHeight;
    float    stepY             = m_rowSpacing + brickHeight;
    for (size_t i = 0; i < m_levelLayout.size(); ++i, offsetY += stepY) {
        std::vector<uint32_t>& brickRow = m_levelLayout[i];
        float                  offsetX  = m_wallWidth + m_columnSpacing + 0.5f * brickWidth;
        float                  stepX    = m_columnSpacing + brickWidth;
        for (size_t j = 0; j < brickRow.size(); ++j, offsetX += stepX) {
            const uint32_t brickMaxHealth                      = m_brickTypes[m_levelLayout[i][j]].hitPoints;
            m_backup.instances[instanceDataIndex].id           = m_brickTypes[m_levelLayout[i][j]].id;
            m_backup.instances[instanceDataIndex].position     = {offsetX, offsetY};
            m_backup.instances[instanceDataIndex].depth        = DEPTH_GAME;
            m_backup.instances[instanceDataIndex].scale        = {brickWidth, brickHeight};
            m_backup.instances[instanceDataIndex].textureIndex = m_textureManager->getTextureId(m_brickTypes[m_levelLayout[i][j]].texturePath);
            m_backup.instances[instanceDataIndex].health       = brickMaxHealth;
            m_backup.instances[instanceDataIndex].maxHealth    = brickMaxHealth;

            if (brickMaxHealth > 0 && brickMaxHealth < UINT32_MAX) {
                ++m_backup.remainingBrickCount;
            }

            ++instanceDataIndex;
        }
    }

    // The ball
    m_ballIndex                                  = instanceDataIndex;
    m_ballInitialPosition                        = {m_windowWidth * 0.5f, padOffset - (0.5f * padDimensions.y + ballRadius + 1.0f)};
    m_backup.instances[m_ballIndex].position     = m_ballInitialPosition;
    m_backup.instances[m_ballIndex].depth        = DEPTH_GAME;
    m_backup.instances[m_ballIndex].scale        = {2.0f * ballRadius, 2.0f * ballRadius};
    m_backup.instances[m_ballIndex].textureIndex = m_textureManager->getTextureId(TEXTURE_BALL);
    ++instanceDataIndex;

    // Foreground
    m_foregroundIndex                                  = instanceDataIndex;
    m_backup.instances[m_foregroundIndex].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.5f};
    m_backup.instances[m_foregroundIndex].depth        = DEPTH_FOREGROUND;
    m_backup.instances[m_foregroundIndex].scale        = {m_windowWidth, m_windowHeight};
    m_backup.instances[m_foregroundIndex].textureIndex = m_textureManager->getTextureId(TEXTURE_FOREGROUND);
    m_backup.instances[m_foregroundIndex].textureAlpha = 1.0f;
    ++instanceDataIndex;

    // Title
    float uiTitleHeight                           = m_windowHeight * 0.2f;
    m_titleIndex                                  = instanceDataIndex;
    m_backup.instances[m_titleIndex].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.25f};
    m_backup.instances[m_titleIndex].depth        = DEPTH_UI;
    m_backup.instances[m_titleIndex].scale        = {uiTitleHeight * UI_TITLE_RATIO, uiTitleHeight};
    m_backup.instances[m_titleIndex].textureAlpha = 0.0f;
    ++instanceDataIndex;

    // Subtitle
    float uiSubtitleHeight                           = uiTitleHeight * 0.25f;
    m_subtitleIndex                                  = instanceDataIndex;
    m_backup.instances[m_subtitleIndex].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.75f};
    m_backup.instances[m_subtitleIndex].depth        = DEPTH_UI;
    m_backup.instances[m_subtitleIndex].scale        = {uiSubtitleHeight * UI_SUBTITLE_RATIO, uiSubtitleHeight};
    m_backup.instances[m_subtitleIndex].textureAlpha = 0.0f;
    ++instanceDataIndex;

    // Labels and digits
    float uiLabelHeight = m_windowHeight * 0.05f;
    float uiLabelWidth  = uiLabelHeight * UI_LABEL_RATIO;
    float digitWidth    = m_windowWidth * 0.015f;

    float currentLabelHeight                           = m_windowHeight - uiLabelHeight * 0.5f;
    m_scoreLabelIndex                                  = instanceDataIndex;
    m_backup.instances[m_scoreLabelIndex].position     = {0.5f * uiLabelWidth, currentLabelHeight};
    m_backup.instances[m_scoreLabelIndex].depth        = DEPTH_UI;
    m_backup.instances[m_scoreLabelIndex].scale        = {uiLabelWidth, uiLabelHeight};
    m_backup.instances[m_scoreLabelIndex].textureIndex = m_textureManager->getTextureId(TEXTURE_UI_SCORE);
    ++instanceDataIndex;

    float scoreDigitPositionX = uiLabelWidth + digitWidth * 0.5f;
    m_scoreCountStartIndex    = instanceDataIndex;
    for (size_t i = 0; i < SCORE_COUNT_DIGITS; ++i) {
        m_backup.instances[instanceDataIndex].position = {scoreDigitPositionX, currentLabelHeight};
        m_backup.instances[instanceDataIndex].depth    = DEPTH_UI;
        m_backup.instances[instanceDataIndex].scale    = {digitWidth, digitWidth};
        scoreDigitPositionX += digitWidth;
        ++instanceDataIndex;
    }

    currentLabelHeight -= uiLabelHeight;
    m_levelLabelIndex                                  = instanceDataIndex;
    m_backup.instances[m_levelLabelIndex].position     = {0.5f * uiLabelWidth, currentLabelHeight};
    m_backup.instances[m_levelLabelIndex].depth        = DEPTH_UI;
    m_backup.instances[m_levelLabelIndex].scale        = {uiLabelWidth, uiLabelHeight};
    m_backup.instances[m_levelLabelIndex].textureIndex = m_textureManager->getTextureId(TEXTURE_UI_LEVEL);
    ++instanceDataIndex;

    float levelDigitPositionX = uiLabelWidth + digitWidth * 0.5f;
    m_levelCountStartIndex    = instanceDataIndex;
    for (size_t i = 0; i < LEVEL_COUNT_DIGITS; ++i) {
        m_backup.instances[instanceDataIndex].position = {levelDigitPositionX, currentLabelHeight};
        m_backup.instances[instanceDataIndex].depth    = DEPTH_UI;
        m_backup.instances[instanceDataIndex].scale    = {digitWidth, digitWidth};
        levelDigitPositionX += digitWidth;
        ++instanceDataIndex;
    }

    currentLabelHeight -= uiLabelHeight;
    m_livesLabelIndex                                  = instanceDataIndex;
    m_backup.instances[m_livesLabelIndex].position     = {0.5f * uiLabelWidth, currentLabelHeight};
    m_backup.instances[m_livesLabelIndex].depth        = DEPTH_UI;
    m_backup.instances[m_livesLabelIndex].scale        = {uiLabelWidth, uiLabelHeight};
    m_backup.instances[m_livesLabelIndex].textureIndex = m_textureManager->getTextureId(TEXTURE_UI_LIVES);
    ++instanceDataIndex;

    float lifeDigitPositionX = uiLabelWidth + digitWidth * 0.5f;
    m_livesCountStartIndex   = instanceDataIndex;
    for (size_t i = 0; i < LIFE_COUNT_DIGITS; ++i) {
        m_backup.instances[instanceDataIndex].position = {lifeDigitPositionX, currentLabelHeight};
        m_backup.instances[instanceDataIndex].depth    = DEPTH_UI;
        m_backup.instances[instanceDataIndex].scale    = {digitWidth, digitWidth};
        lifeDigitPositionX += digitWidth;
        ++instanceDataIndex;
    }

    m_instanceDataBufferSize = VECTOR_SIZE_IN_BYTES(m_backup.instances);
    m_instanceBuffer         = m_renderer->createBuffer(m_instanceDataBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "Instance buffer");
}
