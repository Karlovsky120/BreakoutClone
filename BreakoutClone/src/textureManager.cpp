#include "textureManager.h"

#include "renderer.h"

#pragma warning(push, 0)
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6262)  // Function uses '*' bytes of stack: exceeds stacksize '16384'.Consider moving some data to heap.
#pragma warning(disable : 6308)  // 'realloc' might return null pointer: assigning null pointer to '*', which is passed as an argument to 'realloc', will cause
                                 // the original memory block to be leaked.
#pragma warning(disable : 6385)  // Reading invalid data from '*': the readable size is '*' bytes, but '*' bytes may be read.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.
#pragma warning(disable : 26819) // Dereferencing NULL pointer.'*' contains the same NULL value as '*' did.
#pragma warning(disable : 28182) // Dereferencing NULL pointer.'*' contains the same NULL value as '^' did.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#pragma warning(enable : 28182)
#pragma warning(enable : 26819)
#pragma warning(enable : 26451)
#pragma warning(enable : 6385)
#pragma warning(enable : 6308)
#pragma warning(enable : 6262)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include <filesystem>

TextureManager::TextureManager(Renderer* const renderer) : m_renderer(renderer) {
    for (size_t i = 0; i < 10; ++i) {
        loadTexture(TEXTURE_UI_NUMBER(i));
    }

    loadTexture(TEXTURE_UI_VICTORY);
    loadTexture(TEXTURE_UI_GAME_OVER);
    loadTexture(TEXTURE_UI_LOADING_LEVEL);
    loadTexture(TEXTURE_UI_LEVEL_COMPLETE);

    loadTexture(TEXTURE_UI_LEVEL);
    loadTexture(TEXTURE_UI_LIVES);
    loadTexture(TEXTURE_UI_SCORE);

    loadTexture(TEXTURE_UI_TRY);
    loadTexture(TEXTURE_UI_RELEASE);
}

TextureManager::~TextureManager() { m_textureMap.clear(); }

const uint32_t TextureManager::getTextureId(const std::string& textureId, const float& scale) {
    if (textureId == "") {
        return UINT32_MAX;
    }

    std::string textureMapId = textureId + std::to_string(scale);
    if (m_textureMap.find(textureMapId) != m_textureMap.end()) {
        return m_textureMap[textureMapId];
    }

    return loadTexture(textureId, scale);
}

const std::vector<std::unique_ptr<Image>>& TextureManager::getTextureArray() const { return m_textures; }

const uint32_t TextureManager::loadTexture(const std::string& textureId, const float& scale) {
    std::string textureMapId = textureId + std::to_string(scale);

    auto textureCacheMapEntry = m_textureMap.find(textureMapId);
    if (textureCacheMapEntry != m_textureMap.end()) {
        return textureCacheMapEntry->second;
    }

    std::string path = std::filesystem::current_path().string() + TEXTURE_FOLDER + textureId;

    int      textureWidth;
    int      textureHeight;
    int      textureChannelCount;
    stbi_uc* pixels = stbi_load(path.c_str(), &textureWidth, &textureHeight, &textureChannelCount, STBI_rgb_alpha);

    if (!pixels) {
        char error[512];
        sprintf_s(error, "Failed to load image %s!", textureId.c_str());
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

    m_textures.push_back(m_renderer->createImage({static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight)},
                                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_IMAGE_ASPECT_COLOR_BIT, textureId.c_str()));

    m_renderer->uploadToDeviceLocalImage(pixels, imageSize, *m_textures.back(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (manuallyAllocated) {
        free(resizedPixels);
    } else {
        stbi_image_free(resizedPixels);
    }

    m_textureMap[textureMapId] = static_cast<uint32_t>(m_textures.size() - 1);
    return m_textureMap[textureMapId];
}
