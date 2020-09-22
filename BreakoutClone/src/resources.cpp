#include "resources.h"

#include "breakout.h"
#include "renderer.h"

#pragma warning(push, 0)
#pragma warning(disable : 6011)  // Dereferencing NULL pointer *.
#pragma warning(disable : 6262)  // Function uses '*' bytes of stack: exceeds stacksize '16384'.Consider moving some data to heap.
#pragma warning(disable : 6308)  // 'realloc' might return null pointer: assigning null pointer to '*', which is passed as an argument to 'realloc', will cause
                                 // the original memory block to be leaked.
#pragma warning(disable : 6385)  // Reading invalid data from '*': the readable size is '*' bytes, but '*' bytes may be read.
#pragma warning(disable : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#pragma warning(enable : 26451)
#pragma warning(enable : 6385)
#pragma warning(enable : 6308)
#pragma warning(enable : 6262)
#pragma warning(enable : 6011)
#pragma warning(pop)

#include <filesystem>
#include <string>

const Image Resources::createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
                                   const VkImageAspectFlags& aspectMask) {

    const VkDevice& device = Renderer::getDevice();
    const VkImage   image  = createImage(imageSize, imageUsageFlags, imageFormat);

    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(device, image, &imageMemoryRequirements);
    const VkDeviceMemory memory = allocateVulkanObjectMemory(imageMemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkBindImageMemory(device, image, memory, 0);

    VkImageView view = VK_NULL_HANDLE;
    if (aspectMask != 0) {
        view = createImageView(image, imageFormat, aspectMask);
    }

    return Image(device, image, memory, view, imageSize);
}

const Buffer Resources::createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                     const VkMemoryPropertyFlags& memoryPropertyFlags) {

    const VkDevice& device = Renderer::getDevice();
    const VkBuffer  buffer = createBuffer(bufferSize, bufferUsageFlags);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    const VkDeviceMemory memory = allocateVulkanObjectMemory(memoryRequirements, memoryPropertyFlags);
    vkBindBufferMemory(device, buffer, memory, 0);

    return Buffer(device, buffer, memory);
}

const VkImageMemoryBarrier Resources::createImageMemoryBarrier(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout) {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
    barrier.oldLayout            = oldLayout;
    barrier.newLayout            = newLayout;
    barrier.image                = image;
    barrier.subresourceRange     = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    switch (oldLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        barrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    default:
        assert(!"Unsupported old layout!");
#pragma warning(suppress : 4061) // Not all enumerators handled in the switch of enum
    }

    switch (newLayout) {
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    default:
        assert(!"Unsupported new layout!");
#pragma warning(suppress : 4061) // Not all enumerators handled in the switch of enum
    }

    return barrier;
}

const VkSampler Resources::createSampler() {
    VkSamplerCreateInfo createInfo     = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    createInfo.magFilter               = VK_FILTER_NEAREST;
    createInfo.minFilter               = VK_FILTER_NEAREST;
    createInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.anisotropyEnable        = VK_FALSE;
    createInfo.maxAnisotropy           = 1.0f;
    createInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable           = VK_FALSE;
    createInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.mipLodBias              = 0.0f;
    createInfo.minLod                  = 0.0f;
    createInfo.maxLod                  = 0.0f;

    VkSampler sampler = 0;
    VK_CHECK(vkCreateSampler(Renderer::getDevice(), &createInfo, nullptr, &sampler));

    return sampler;
}

void Resources::uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& bufferMemory) {
    const VkDevice& device = Renderer::getDevice();
    void*           hostBufferPointer;
    VK_CHECK(vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &hostBufferPointer));
    memcpy(hostBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(device, bufferMemory);
}

void Resources::uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const VkBuffer& deviceBuffer) {

    const VkDevice& device        = Renderer::getDevice();
    const Buffer&   stagingBuffer = Renderer::getStagingBuffer();
    void*           stagingBufferPointer;

    VK_CHECK(vkMapMemory(device, stagingBuffer.memory, 0, bufferSize, 0, &stagingBufferPointer));
    memcpy(stagingBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(device, stagingBuffer.memory);

    const VkCommandPool&        transferCommandPool               = Renderer::getTransferCommandPool();
    VkCommandBufferAllocateInfo transferCommandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    transferCommandBufferAllocateInfo.commandPool                 = transferCommandPool;
    transferCommandBufferAllocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    transferCommandBufferAllocateInfo.commandBufferCount          = 1;

    VkCommandBuffer transferCommandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &transferCommandBufferAllocateInfo, &transferCommandBuffer));

    VkCommandBufferBeginInfo transferCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(transferCommandBuffer, &transferCommandBufferBeginInfo));

    VkBufferCopy bufferCopy = {};
    bufferCopy.srcOffset    = 0;
    bufferCopy.dstOffset    = 0;
    bufferCopy.size         = bufferSize;
    vkCmdCopyBuffer(transferCommandBuffer, stagingBuffer.buffer, deviceBuffer, 1, &bufferCopy);
    VK_CHECK(vkEndCommandBuffer(transferCommandBuffer));

    VkSubmitInfo transferSubmitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    transferSubmitInfo.commandBufferCount = 1;
    transferSubmitInfo.pCommandBuffers    = &transferCommandBuffer;

    VK_CHECK(vkQueueSubmit(Renderer::getQueue(), 1, &transferSubmitInfo, VK_NULL_HANDLE));
    vkDeviceWaitIdle(device);

    vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

void Resources::uploadToDeviceLocalImage(const void* data, const uint32_t& imageSize, const Image& deviceImage, const VkImageLayout& initialLayout,
                                         const VkImageLayout& finalLayout) {

    const VkDevice& device        = Renderer::getDevice();
    const Buffer&   stagingBuffer = Renderer::getStagingBuffer();
    void*           stagingBufferPointer;

    VK_CHECK(vkMapMemory(device, stagingBuffer.memory, 0, imageSize, 0, &stagingBufferPointer));
    memcpy(stagingBufferPointer, reinterpret_cast<const uint8_t*>(data), imageSize);
    vkUnmapMemory(device, stagingBuffer.memory);

    const VkCommandPool&        transferCommandPool               = Renderer::getTransferCommandPool();
    VkCommandBufferAllocateInfo transferCommandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    transferCommandBufferAllocateInfo.commandPool                 = transferCommandPool;
    transferCommandBufferAllocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    transferCommandBufferAllocateInfo.commandBufferCount          = 1;

    VkCommandBuffer transferCommandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &transferCommandBufferAllocateInfo, &transferCommandBuffer));

    VkCommandBufferBeginInfo transferCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(transferCommandBuffer, &transferCommandBufferBeginInfo));

    VkBufferImageCopy bufferImageCopy               = {};
    bufferImageCopy.bufferOffset                    = 0;
    bufferImageCopy.bufferRowLength                 = 0;
    bufferImageCopy.bufferImageHeight               = 0;
    bufferImageCopy.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopy.imageSubresource.mipLevel       = 0;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;
    bufferImageCopy.imageSubresource.layerCount     = 1;
    bufferImageCopy.imageOffset                     = {0, 0, 0};
    bufferImageCopy.imageExtent                     = {deviceImage.size.width, deviceImage.size.height, 1};

    const VkImageMemoryBarrier imageMemoryBarrierBefore = createImageMemoryBarrier(deviceImage.image, initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrierBefore);

    vkCmdCopyBufferToImage(transferCommandBuffer, stagingBuffer.buffer, deviceImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

    const VkImageMemoryBarrier imageMemoryBarrierAfter = createImageMemoryBarrier(deviceImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout);

    vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrierAfter);

    VK_CHECK(vkEndCommandBuffer(transferCommandBuffer));

    VkSubmitInfo transferSubmitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    transferSubmitInfo.commandBufferCount = 1;
    transferSubmitInfo.pCommandBuffers    = &transferCommandBuffer;

    VK_CHECK(vkQueueSubmit(Renderer::getQueue(), 1, &transferSubmitInfo, VK_NULL_HANDLE));
    vkDeviceWaitIdle(device);

    vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}

const uint32_t Resources::loadTexture(const std::string& pathToTexture, const float& scale) {
    std::string textureMapId = pathToTexture + std::to_string(scale);

    auto textureCacheMapEntry = m_textureMap.find(textureMapId);
    if (textureCacheMapEntry != m_textureMap.end()) {
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

    m_textures.push_back(Resources::createImage({static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight)},
                                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                                                VK_IMAGE_ASPECT_COLOR_BIT));

    Resources::uploadToDeviceLocalImage(pixels, imageSize, m_textures.back(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (manuallyAllocated) {
        free(resizedPixels);
    } else {
        stbi_image_free(resizedPixels);
    }

    const Level* activeLevel = Breakout::getActiveLevel();
    if (activeLevel && activeLevel->isCommandBufferRecorded()) {
        Renderer::updateTextureArray();
        activeLevel->updateCommandBuffers();
    }

    return m_textureMap[textureMapId] = m_textureMaxId++;
}

const uint32_t Resources::getTextureId(const std::string& texturePath, const float& scale) {
    if (texturePath == "") {
        return UINT32_MAX;
    }

    std::string textureMapId = texturePath + std::to_string(scale);
    if (m_textureMap.find(textureMapId) != m_textureMap.end()) {
        return m_textureMap[texturePath + std::to_string(scale)];
    }

    return loadTexture(texturePath, scale);
}

const std::vector<Image>& Resources::getTextureArray() { return m_textures; }

void Resources::cleanup() {
    for (const auto& texture : m_textures) {
        texture.destroy();
    }

    m_textureMap.clear();
}

std::string Resources::m_resourcesPath = "";

std::map<std::string, uint32_t> Resources::m_textureMap;
std::vector<Image>              Resources::m_textures;
uint32_t                        Resources::m_textureMaxId = 0;

const VkImage Resources::createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat) {
    VkImageCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    createInfo.imageType         = VK_IMAGE_TYPE_2D;
    createInfo.usage             = imageUsageFlags;
    createInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.format            = imageFormat;
    createInfo.extent            = {imageSize.width, imageSize.height, 1};
    createInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    createInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.mipLevels         = 1;
    createInfo.arrayLayers       = 1;

    VkImage image = 0;
    VK_CHECK(vkCreateImage(Renderer::getDevice(), &createInfo, nullptr, &image));

    return image;
}

const VkImageView Resources::createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask) {
    VkImageViewCreateInfo createInfo           = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image                           = image;
    createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format                          = format;
    createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask     = aspectMask;
    createInfo.subresourceRange.baseMipLevel   = 0;
    createInfo.subresourceRange.levelCount     = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView = 0;
    VK_CHECK(vkCreateImageView(Renderer::getDevice(), &createInfo, nullptr, &imageView));

    return imageView;
}

const VkBuffer Resources::createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags) {
    VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    createInfo.size               = bufferSize;
    createInfo.usage              = bufferUsageFlags;
    createInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = 0;
    VK_CHECK(vkCreateBuffer(Renderer::getDevice(), &createInfo, nullptr, &buffer));

    return buffer;
}

const uint32_t Resources::findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags) {
    uint32_t                                memoryType       = UINT32_MAX;
    const VkPhysicalDeviceMemoryProperties& memoryProperties = Renderer::getMemoryProperties();

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        bool memoryIsOfRequiredType        = memoryTypeBits & (1 << i);
        bool memoryHasDesiredPropertyFlags = (memoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags;

        if (memoryIsOfRequiredType && memoryHasDesiredPropertyFlags) {
            memoryType = i;
            break;
        }
    }

    if (memoryType == UINT32_MAX) {
        throw std::runtime_error("Couldn't find memory type for depth image!");
    }

    return memoryType;
}

const VkDeviceMemory Resources::allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags) {

    uint32_t memoryType = findMemoryType(memoryRequirements.memoryTypeBits, memoryPropertyFlags);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryAllocateInfo.allocationSize       = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex      = memoryType;

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(Renderer::getDevice(), &memoryAllocateInfo, nullptr, &memory));

    return memory;
}

const std::string& Resources::getResourcesPath() {
    if (m_resourcesPath == "") {
        std::filesystem::path executablePath = std::filesystem::current_path();
        executablePath += std::string(RESOURCES_FOLDER);
        m_resourcesPath = executablePath.string();
    }

    return m_resourcesPath;
}