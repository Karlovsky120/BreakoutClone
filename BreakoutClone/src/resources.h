#pragma once

#include "common.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

#include <memory>

struct Buffer {
    VkBuffer       buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    Buffer(){};
    Buffer(const VkDevice& device, const VkBuffer& buffer, const VkDeviceMemory& memory) : buffer(buffer), memory(memory), device(device) {}

    Buffer(Buffer&&) = default;
    Buffer& operator=(Buffer&&) = default;
    Buffer(Buffer const&)       = delete;
    void operator=(Buffer const&) = delete;

    ~Buffer() {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

  private:
    VkDevice device = VK_NULL_HANDLE;
};

struct Image {
    VkImage        image  = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView    view   = VK_NULL_HANDLE;

    Image(){};
    Image(const VkDevice& device, const VkImage& image, const VkDeviceMemory& memory, const VkImageView& view)
        : device(device), image(image), memory(memory), view(view) {}

    Image(Image&&) = default;
    Image& operator=(Image&&) = default;
    Image(Image const&)       = delete;
    void operator=(Image const&) = delete;

    ~Image() {
        vkDestroyImageView(device, view, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

  private:
    VkDevice device = VK_NULL_HANDLE;
};

class Resources {
  public:
    static std::unique_ptr<Image>  createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
                                               const VkImageAspectFlags& aspectMask);
    static std::unique_ptr<Buffer> createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                                const VkMemoryPropertyFlags& memoryPropertyFlags);
    static VkSampler               createSampler();
    static void                    uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& memory);
    static void uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const Buffer& stagingBuffer, const VkBuffer& deviceBuffer,
                                          const uint32_t& dstOffset = 0);

  private:
    static VkImage        createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat);
    static VkImageView    createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask);
    static VkBuffer       createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags);
    static uint32_t       findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags);
    static VkDeviceMemory allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags);
};
