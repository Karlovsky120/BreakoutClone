#include "level.h"

#include "renderer.h"

// This generally silences all warnings in the external file, but it doesn't work for warning bellow.
#pragma warning(push, 0)
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6262)  // Function uses '*' bytes of stack: exceeds stacksize '16384'.Consider moving some data to heap.
#pragma warning(disable : 6308)  // 'realloc' might return null pointer: assigning null pointer to '*', which is passed as an argument to 'realloc', will cause
                                 // the original memory block to be leaked.
#pragma warning(disable : 6319)  // Use of the comma-operator in a tested expression causes the left argument to be ignored when it has no side-effects.
#pragma warning(disable : 6385)  // Reading invalid data from '*': the readable size is '*' bytes, but '*' bytes may be read.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.
#pragma warning(disable : 26495) // Variable * is uninitialized.Always initialize a member variable.
#pragma warning(disable : 28182) // Dereferencing NULL pointer '*' contains the same NULL value as '*'.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#include "tinyxml2.cpp"
#include "tinyxml2.h"

#pragma warning(enable : 28182)
#pragma warning(enable : 26495)
#pragma warning(enable : 26451)
#pragma warning(enable : 6385)
#pragma warning(enable : 6319)
#pragma warning(enable : 6308)
#pragma warning(enable : 6262)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include "common.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>

void Level::load() {

    // Load cracked texture to position 0
    loadTexture("bricks\\cracks.png");

    m_padTextureId             = loadTexture("pad.png");
    m_ballTextureId            = loadTexture("ball.png");
    m_backgroundTextureId      = loadTexture(m_backgroundTexturePath);
    m_backgroundTextureSmallId = loadTexture(m_backgroundTexturePath, SIDE_BLUR_STRENGTH);

    for (auto& entry : m_brickTypes) {
        entry.second.textureId = loadTexture(entry.second.texturePath);
    }

    Renderer::updateTextureArray(m_textures);

    generateRenderData();

    Renderer::recordRenderCommandBuffers(m_instanceBuffer.buffer, static_cast<uint32_t>(m_instances.size()));
}

void Level::updateGPUData() const { Resources::uploadToHostVisibleBuffer(m_instances.data(), m_instanceDataBufferSize, m_instanceBuffer.memory); }

Level::Level(const char* fullPath, const uint32_t& levelIndex, const uint32_t& windowWidth, const uint32_t& windowHeight)
    : m_levelIndex(levelIndex), m_windowWidth(windowWidth), m_windowHeight(windowHeight) {
    parseXml(fullPath);
}

void Level::destroy() {
    m_instanceBuffer.destroy();

    for (Image& texture : m_textures) {
        texture.destroy();
    }
}

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
    float     brickWidth    = (m_windowWidth - (MAX_COLUMN_COUNT + 1) * MAX_COLUMN_SPACING) / static_cast<float>(MAX_COLUMN_COUNT);
    float     bottomPadding = MAX_COLUMN_SPACING * 15;
    float     brickHeight   = (m_windowHeight - ((MAX_ROW_COUNT + 1) * MAX_ROW_SPACING) - bottomPadding) / static_cast<float>(MAX_ROW_COUNT);
    float     playAreaWidth = m_columnCount * brickWidth + (m_columnCount + 1) * m_columnSpacing;
    float     wallWidth     = (m_windowWidth - playAreaWidth) * 0.5f;
    float     ballRadius    = 0.375f * brickWidth;
    glm::vec2 padDimensions = {playAreaWidth * 0.2f, brickHeight};

    float padOffset = m_windowHeight - MAX_ROW_SPACING * 2.0f;

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
    m_instances[BACKGROUND_INDEX].textureIndex = m_backgroundTextureId;

    // Left wall
    m_instances[LEFT_WALL_INDEX].position     = {wallWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[LEFT_WALL_INDEX].depth        = GAME_DEPTH;
    m_instances[LEFT_WALL_INDEX].scale        = {wallWidth, m_windowHeight};
    m_instances[LEFT_WALL_INDEX].textureIndex = m_backgroundTextureSmallId;
    m_instances[LEFT_WALL_INDEX].uvOffset     = {0.0f, 0.0f};
    m_instances[LEFT_WALL_INDEX].uvScale      = {wallWidth / static_cast<float>(m_windowWidth), 1.0f};

    // Right wall
    m_instances[RIGHT_WALL_INDEX].position     = {static_cast<float>(m_windowWidth) - wallWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[RIGHT_WALL_INDEX].depth        = GAME_DEPTH;
    m_instances[RIGHT_WALL_INDEX].scale        = {wallWidth, m_windowHeight};
    m_instances[RIGHT_WALL_INDEX].textureIndex = m_backgroundTextureSmallId;
    m_instances[RIGHT_WALL_INDEX].uvOffset     = {m_windowWidth - wallWidth / static_cast<float>(m_windowWidth), 0.0f};
    m_instances[RIGHT_WALL_INDEX].uvScale      = {wallWidth / static_cast<float>(m_windowWidth), 1.0f};

    // The pad
    m_instances[PAD_INDEX].position     = {wallWidth + playAreaWidth * 0.5f, padOffset};
    m_instances[PAD_INDEX].depth        = GAME_DEPTH;
    m_instances[PAD_INDEX].scale        = padDimensions;
    m_instances[PAD_INDEX].textureIndex = m_padTextureId;

    // The ball
    m_instances[BALL_INDEX].position.x   = m_windowWidth * 0.5f;
    m_instances[BALL_INDEX].position.y   = padOffset - (0.5f * padDimensions.y + ballRadius + 1.0f);
    m_instances[BALL_INDEX].depth        = GAME_DEPTH;
    m_instances[BALL_INDEX].scale        = {2.0f * ballRadius, 2.0f * ballRadius};
    m_instances[BALL_INDEX].textureIndex = m_ballTextureId;

    // Bricks
    uint32_t instanceDataIndex = BRICK_START_INDEX;
    float    offsetY           = m_rowSpacing + 0.5f * brickHeight;
    float    stepY             = m_rowSpacing + brickHeight;
    for (size_t i = 0; i < m_levelLayout.size(); ++i, offsetY += stepY) {
        std::vector<uint32_t>& brickRow = m_levelLayout[i];
        float                  offsetX  = wallWidth + m_columnSpacing + 0.5f * brickWidth;
        float                  stepX    = m_columnSpacing + brickWidth;
        for (size_t j = 0; j < brickRow.size(); ++j, offsetX += stepX) {
            m_instances[instanceDataIndex].position     = {offsetX, offsetY};
            m_instances[instanceDataIndex].depth        = GAME_DEPTH;
            m_instances[instanceDataIndex].scale        = {brickWidth, brickHeight};
            m_instances[instanceDataIndex].textureIndex = m_brickTypes[m_levelLayout[i][j]].textureId;
            m_instances[instanceDataIndex].health       = m_brickTypes[m_levelLayout[i][j]].hitPoints;
            m_instances[instanceDataIndex].maxHealth    = m_brickTypes[m_levelLayout[i][j]].hitPoints;
            ++instanceDataIndex;
        }
    }

    // Foreground
    m_foregroundIndex                           = instanceDataIndex;
    m_instances[m_foregroundIndex].position     = {m_windowWidth * 0.5f, m_windowHeight * 0.5f};
    m_instances[m_foregroundIndex].depth        = FOREGROUND_DEPTH;
    m_instances[m_foregroundIndex].scale        = {m_windowWidth, m_windowHeight};
    m_instances[m_foregroundIndex].textureIndex = 0;
    m_instances[m_foregroundIndex].textureAlpha = 0.0f;

    m_instanceDataBufferSize = VECTOR_SIZE_IN_BYTES(m_instances);
    m_instanceBuffer         = Resources::createBuffer(m_instanceDataBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    Renderer::nameObject(&m_instanceBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "Instance buffer");

    Resources::uploadToHostVisibleBuffer(m_instances.data(), m_instanceDataBufferSize, m_instanceBuffer.memory);
}

uint32_t Level::loadTexture(const std::string& pathToTexture, const float& scale) {
    std::string textureCacheId = pathToTexture + std::to_string(scale);

    auto textureCacheMapEntry = m_textureCacheMap.find(textureCacheId);
    if (textureCacheMapEntry != m_textureCacheMap.end()) {
        return textureCacheMapEntry->second;
    }

    std::filesystem::path fullPath = std::filesystem::current_path();
    fullPath += "\\resources\\textures\\";
    fullPath += pathToTexture;

    int      textureWidth;
    int      textureHeight;
    int      textureChannelCount;
    stbi_uc* pixels = stbi_load(fullPath.string().c_str(), &textureWidth, &textureHeight, &textureChannelCount, STBI_rgb_alpha);

    if (!pixels) {
        char error[512];
        sprintf_s(error, "Failed to load image %s!", pathToTexture.c_str());
        throw std::runtime_error(error);
    }

    uint32_t imageSize         = 0;
    bool     manuallyAllocated = false;
    stbi_uc* resizedPixels     = nullptr;
    if (scale > 1.0001f || scale < 0.9999f) {
        uint32_t newWidth     = static_cast<uint32_t>(textureWidth * scale);
        uint32_t newHeight    = static_cast<uint32_t>(textureHeight * scale);
        uint32_t newImageSize = newWidth * newHeight * textureChannelCount;

        // stb is crashing unless I allocate the memory myself
        resizedPixels = (stbi_uc*)malloc(newImageSize);
        stbir_resize_uint8(pixels, textureWidth, textureHeight, 0, resizedPixels, newWidth, newHeight, 0, textureChannelCount);
        stbi_image_free(pixels);

        textureWidth  = newWidth;
        textureHeight = newHeight;
        imageSize     = newImageSize;

        pixels = resizedPixels;

        manuallyAllocated = true;
    } else {
        imageSize = textureWidth * textureHeight * textureChannelCount;
    }

    Image texture = Resources::createImage({static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight)},
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    Resources::uploadToDeviceLocalImage(pixels, imageSize, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (manuallyAllocated) {
        free(resizedPixels);
    } else {
        stbi_image_free(resizedPixels);
    }

    m_textureCacheMap[textureCacheId] = static_cast<uint32_t>(m_textures.size());
    m_textures.push_back(texture);

    return static_cast<uint32_t>(m_textures.size()) - 1;
}
