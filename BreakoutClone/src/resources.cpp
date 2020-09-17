#include "resources.h"

#include "renderer.h"

#include <stdexcept>

std::unique_ptr<Image> Resources::createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
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

    return std::make_unique<Image>(device, image, memory, view);
}

std::unique_ptr<Buffer> Resources::createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                                const VkMemoryPropertyFlags& memoryPropertyFlags) {

    const VkDevice& device = Renderer::getDevice();
    const VkBuffer  buffer = createBuffer(bufferSize, bufferUsageFlags);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    const VkDeviceMemory memory = allocateVulkanObjectMemory(memoryRequirements, memoryPropertyFlags);
    vkBindBufferMemory(device, buffer, memory, 0);

    return std::make_unique<Buffer>(device, buffer, memory);
}

VkImage Resources::createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat) {
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
    VK_CHECK(vkCreateImage(Renderer::getDevice(), &imageCreateInfo, nullptr, &image));

    return image;
}

VkImageView Resources::createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask) {
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
    VK_CHECK(vkCreateImageView(Renderer::getDevice(), &imageViewCreateInfo, nullptr, &imageView));

    return imageView;
}

VkBuffer Resources::createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags) {
    VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCreateInfo.size               = bufferSize;
    bufferCreateInfo.usage              = bufferUsageFlags;
    bufferCreateInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = 0;
    VK_CHECK(vkCreateBuffer(Renderer::getDevice(), &bufferCreateInfo, nullptr, &buffer));

    return buffer;
}

uint32_t Resources::findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags) {
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

VkDeviceMemory Resources::allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags) {

    uint32_t memoryType = findMemoryType(memoryRequirements.memoryTypeBits, memoryPropertyFlags);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryAllocateInfo.allocationSize       = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex      = memoryType;

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(Renderer::getDevice(), &memoryAllocateInfo, nullptr, &memory));

    return memory;
}

void Resources::uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& bufferMemory) {
    const VkDevice& device = Renderer::getDevice();
    void*           hostBufferPointer;
    VK_CHECK(vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &hostBufferPointer));
    memcpy(hostBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(device, bufferMemory);
}

void Resources::uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const Buffer& stagingBuffer, const VkBuffer& deviceBuffer,
                                          const uint32_t& dstOffset) {

    const VkDevice& device = Renderer::getDevice();
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
    bufferCopy.dstOffset    = dstOffset;
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
