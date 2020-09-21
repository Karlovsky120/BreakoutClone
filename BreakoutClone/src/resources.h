#pragma once
#include "common.h"
#include "commonExternal.h"

#define PAD_TEXTURE_PATH        "pad.png"
#define BALL_TEXTURE_PATH       "ball.png"
#define CRACKS_TEXTURE_PATH     "bricks\\cracks.png"
#define FOREGROUND_TEXTURE_PATH "boards\\foreground.png"

#define RESOURCES_FOLDER "\\resources\\"

#define LEVELS_FOLDER "levels\\";

struct Buffer {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    Buffer(){};
    Buffer(const VkDevice& device, const VkBuffer& buffer, const VkDeviceMemory& memory) : buffer(buffer), memory(memory), device(device) {}

    void destroy() const {
        if (device != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
            vkFreeMemory(device, memory, nullptr);
        }
    }

  private:
    VkDevice device = VK_NULL_HANDLE;
};

struct Image {
    VkImage        image  = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView    view   = VK_NULL_HANDLE;
    VkExtent2D     size   = {};

    Image(){};
    Image(const VkDevice& device, const VkImage& image, const VkDeviceMemory& memory, const VkImageView& view, const VkExtent2D& size)
        : device(device), image(image), memory(memory), view(view), size(size) {}

    const void destroy() const {
        if (device != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
            vkDestroyImage(device, image, nullptr);
            vkFreeMemory(device, memory, nullptr);
        }
    }

  private:
    VkDevice device = VK_NULL_HANDLE;
};

class Resources {
  public:
    static const std::string& getResourcesPath();
    static const Image        createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
                                          const VkImageAspectFlags& aspectMask);
    static const Buffer       createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                           const VkMemoryPropertyFlags& memoryPropertyFlags);
    static const VkSampler    createSampler();
    static void               uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& memory);
    static void               uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const VkBuffer& deviceBuffer);
    static void           uploadToDeviceLocalImage(const void* data, const uint32_t& imageSize, const Image& deviceImage, const VkImageLayout& initialLayout,
                                                   const VkImageLayout& finalLayout);
    static const uint32_t getTextureId(const std::string& texturePath, const float& scale = 1.0f);
    static const std::vector<Image>& getTextureArray();
    static void                      cleanup();

  private:
    static std::string m_resourcesPath;

    static std::map<std::string, uint32_t> m_textureMap;
    static std::vector<Image>              m_textures;
    static uint32_t                        m_textureMaxId;

    static const VkImage        createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat);
    static const VkImageView    createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask);
    static const VkBuffer       createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags);
    static const uint32_t       findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags);
    static const VkDeviceMemory allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags);
    static const VkImageMemoryBarrier createImageMemoryBarrier(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout);
    static const uint32_t             loadTexture(const std::string& pathToTexture, const float& scale = 1.0f);
};
