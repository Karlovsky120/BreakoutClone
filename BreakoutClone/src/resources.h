#pragma once

#include "common.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

#include <vector>

struct Buffer {
    VkBuffer        buffer        = VK_NULL_HANDLE;
    VkDeviceMemory  memory        = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = VK_NULL_HANDLE;
};

VkImage              createImage(const VkDevice device, const VkExtent2D imageSize, const VkImageUsageFlags imageUsageFlags, const VkFormat imageFormat);
VkImageView          createImageView(const VkDevice device, const VkImage image, const VkFormat format, const VkImageAspectFlags aspectMask);
VkImageMemoryBarrier createImageMemoryBarrier(const VkImage image, const VkImageLayout oldLayout, const VkImageLayout newLayout);
VkBuffer             createBuffer(const VkDevice device, const VkDeviceSize bufferSize, const VkBufferUsageFlags bufferUsageFlags);
Buffer               createBuffer(const VkDevice device, const VkDeviceSize bufferSize, const VkBufferUsageFlags bufferUsageFlags,
                                  const VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties, const VkMemoryPropertyFlags memoryPropertyFlags,
                                  const VkMemoryAllocateFlags memoryAllocateFlags = 0);
uint32_t             findMemoryType(const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties, const uint32_t memoryTypeBits,
                                    const VkMemoryPropertyFlags memoryPropertyFlags);
VkDeviceMemory       allocateVulkanObjectMemory(const VkDevice device, const VkMemoryRequirements& memoryRequirements,
                                                const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
                                                const VkMemoryPropertyFlags memoryPropertyFlags, const VkMemoryAllocateFlags memoryAllocateFlags = 0);
void                 uploadToDeviceLocalBuffer(const VkDevice device, const void* data, const uint32_t bufferSize, const VkBuffer stagingBuffer,
                                               const VkDeviceMemory stagingBufferMemory, const VkBuffer deviceBuffer, const VkCommandPool transferCommandPool,
                                               const VkQueue queue);
