#include "resources.h"

#include <stdexcept>

VkImage createImage(const VkDevice device, const VkExtent2D imageSize, const VkImageUsageFlags imageUsageFlags, const VkFormat imageFormat) {
    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.usage             = imageUsageFlags;
    imageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.format            = imageFormat;
    imageCreateInfo.extent            = {imageSize.width, imageSize.height, 1};
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = 1;

    VkImage image = 0;
    VK_CHECK(vkCreateImage(device, &imageCreateInfo, nullptr, &image));

    return image;
}

VkImageView createImageView(const VkDevice device, const VkImage image, const VkFormat format, const VkImageAspectFlags aspectMask) {
    VkImageViewCreateInfo imageViewCreateInfo           = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewCreateInfo.image                           = image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = format;
    imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.subresourceRange.aspectMask     = aspectMask;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView = 0;
    VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView));

    return imageView;
}

VkImageMemoryBarrier createImageMemoryBarrier(const VkImage image, const VkImageLayout oldLayout, const VkImageLayout newLayout) {
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
    case VK_IMAGE_LAYOUT_GENERAL:
        barrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    default:
        assert(false);
#pragma warning(suppress : 4061) // Not all enumerators handled in the switch of enum
    }

    switch (newLayout) {
    case VK_IMAGE_LAYOUT_GENERAL:
        barrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        barrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    default:
        assert(false);
#pragma warning(suppress : 4061) // Not all enumerators handled in the switch of enum
    }

    return barrier;
}

VkBuffer createBuffer(const VkDevice device, const VkDeviceSize bufferSize, const VkBufferUsageFlags bufferUsageFlags) {
    VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCreateInfo.size               = bufferSize;
    bufferCreateInfo.usage              = bufferUsageFlags;
    bufferCreateInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = 0;
    VK_CHECK(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer));

    return buffer;
}

Buffer createBuffer(const VkDevice device, const VkDeviceSize bufferSize, const VkBufferUsageFlags bufferUsageFlags,
                    const VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties, const VkMemoryPropertyFlags memoryPropertyFlags,
                    const VkMemoryAllocateFlags memoryAllocateFlags) {

    Buffer buffer;
    buffer.buffer = createBuffer(device, bufferSize, bufferUsageFlags);

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(device, buffer.buffer, &memoryRequirements);

    buffer.memory = allocateVulkanObjectMemory(device, memoryRequirements, physicalDeviceMemoryProperties, memoryPropertyFlags, memoryAllocateFlags);
    vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);

    if (memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && memoryAllocateFlags & VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo deviceAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        deviceAddressInfo.buffer                    = buffer.buffer;

        buffer.deviceAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);
    }

    return buffer;
}

uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties, const uint32_t memoryTypeBits,
                        const VkMemoryPropertyFlags memoryPropertyFlags) {
    uint32_t memoryType = UINT32_MAX;
    for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
        bool memoryIsOfRequiredType        = memoryTypeBits & (1 << i);
        bool memoryHasDesiredPropertyFlags = (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags;

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

VkDeviceMemory allocateVulkanObjectMemory(const VkDevice device, const VkMemoryRequirements& memoryRequirements,
                                          const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
                                          const VkMemoryPropertyFlags memoryPropertyFlags, const VkMemoryAllocateFlags memoryAllocateFlags) {

    uint32_t memoryType = findMemoryType(physicalDeviceMemoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryAllocateInfo.allocationSize       = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex      = memoryType;

    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    if (memoryAllocateFlags != 0) {
        memoryAllocateFlagsInfo.flags = memoryAllocateFlags;
        memoryAllocateInfo.pNext      = &memoryAllocateFlagsInfo;
    }

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &memory));

    return memory;
}

void uploadToHostVisibleBuffer(const VkDevice device, const void* data, const uint32_t bufferSize, const VkDeviceMemory bufferMemory) {
    void* hostBufferPointer;
    VK_CHECK(vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &hostBufferPointer));
    memcpy(hostBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(device, bufferMemory);
}

void uploadToDeviceLocalBuffer(const VkDevice device, const void* data, const uint32_t bufferSize, const VkBuffer stagingBuffer,
                               const VkDeviceMemory stagingBufferMemory, const VkBuffer deviceBuffer, const VkCommandPool transferCommandPool,
                               const VkQueue queue, const uint32_t dstOffset) {

    void* stagingBufferPointer;
    VK_CHECK(vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &stagingBufferPointer));
    memcpy(stagingBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

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
    bufferCopy.dstOffset    = dstOffset;
    bufferCopy.size         = bufferSize;
    vkCmdCopyBuffer(transferCommandBuffer, stagingBuffer, deviceBuffer, 1, &bufferCopy);
    VK_CHECK(vkEndCommandBuffer(transferCommandBuffer));

    VkSubmitInfo transferSubmitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    transferSubmitInfo.commandBufferCount = 1;
    transferSubmitInfo.pCommandBuffers    = &transferCommandBuffer;

    VK_CHECK(vkQueueSubmit(queue, 1, &transferSubmitInfo, VK_NULL_HANDLE));
    vkDeviceWaitIdle(device);

    vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}