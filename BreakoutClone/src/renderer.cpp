#include "renderer.h"

#include "sharedStructures.h"

#include "common.h"

Renderer::Renderer() {
    initSDL();

    VK_CHECK(volkInitialize());
    createInstance();

#ifdef VALIDATION_ENABLED
    setupDebugUtils();
#endif

    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface)) {
        throw std::runtime_error("Failed to create SDL Vulkan surface!");
    }

    pickPhysicalDevice();
    createDevice();

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

    createCommandPool();

    VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    m_swapchain                      = std::make_unique<Swapchain>(m_window, m_surface, m_physicalDevice, m_device, m_queueFamilyIndex, surfaceFormat);
    m_surfaceExtent                  = m_swapchain->getSurfaceExtent();
    m_swapchainImageCount            = m_swapchain->getImageCount();

    m_depthImage =
        createImage(m_surfaceExtent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT, "Depth image");

    createRenderPass();
    createFramebuffers();

    createSampler();

    createDescriptorLayout();

    createPipelineCache();
    createPipelineLayout();
    createPipeline();

    createDescriptorPool();
    allocateDescriptorSet();

    allocateRenderCommandBuffers();
    createSyncObjects();

    setupRenderLoop();

    m_stagingBuffer = createBuffer(STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "Staging buffer");

    m_uniformBuffer = createBuffer(sizeof(UniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Uniform buffer");

    writeDescriptorSet();

    createVertexAndIndexBuffers();
}

Renderer::~Renderer() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    for (VkSemaphore& semaphore : m_imageAvailableSemaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    for (VkSemaphore& semaphore : m_renderFinishedSemaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }

    for (VkFence& fence : m_inFlightFences) {
        vkDestroyFence(m_device, fence, nullptr);
    }

    vkFreeCommandBuffers(m_device, m_commandPool, m_swapchainImageCount, m_renderCommandBuffers.data());

    vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

    vkDestroySampler(m_device, m_sampler, nullptr);

    for (VkFramebuffer& framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

    m_indexBuffer.reset();
    m_vertexBuffer.reset();
    m_stagingBuffer.reset();
    m_uniformBuffer.reset();
    m_depthImage.reset();
    m_swapchain.reset();

    vkDestroyDevice(m_device, nullptr);

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

#ifdef VALIDATION_ENABLED
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugUtilsMessenger, nullptr);
#endif

    vkDestroyInstance(m_instance, nullptr);

    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Renderer::showWindow() const { SDL_ShowWindow(m_window); }

void Renderer::waitIdle() const { vkDeviceWaitIdle(m_device); }

void Renderer::setWindowTitle(const char* title) { SDL_SetWindowTitle(m_window, title); }

void Renderer::acquireImage() {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain->get(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex));

    if (m_imagesInFlight[m_currentImageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_device, 1, &m_imagesInFlight[m_currentImageIndex], VK_TRUE, UINT64_MAX);
    }
    m_imagesInFlight[m_currentImageIndex] = m_inFlightFences[m_currentFrame];
}

void Renderer::renderAndPresentImage() {
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    m_renderSubmitInfo.pWaitSemaphores   = &m_imageAvailableSemaphores[m_currentFrame];
    m_renderSubmitInfo.pCommandBuffers   = &m_renderCommandBuffers[m_currentImageIndex];
    m_renderSubmitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
    VK_CHECK(vkQueueSubmit(m_queue, 1, &m_renderSubmitInfo, m_inFlightFences[m_currentFrame]));

    m_presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
    m_presentInfo.pImageIndices   = &m_currentImageIndex;
    VK_CHECK(vkQueuePresentKHR(m_queue, &m_presentInfo));

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recordRenderCommandBuffers(const VkBuffer& instanceBuffer, const uint32_t& instanceCount) {
    if (m_renderCommandBuffersRecorded) {
        resetRenderCommandBuffers();
    }

    for (uint32_t i = 0; i < m_swapchainImageCount; ++i) {
        recordRenderCommandBuffer(i, instanceBuffer, instanceCount);
    }

    m_renderCommandBuffersRecorded = true;
}

void Renderer::updateTextureArray(const std::vector<std::unique_ptr<Image>>& textures) {
    VkDescriptorImageInfo descriptorImageInfo;
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::vector<VkDescriptorImageInfo> descriptorImageInfos;
    size_t                             i = 0;
    for (; i < textures.size(); ++i) {
        descriptorImageInfo.imageView = textures[i]->view;
        descriptorImageInfos.push_back(descriptorImageInfo);
    }

    // Fill the bindings so valdation layers would shut up.
    for (; i < MAX_TEXTURE_COUNT; ++i) {
        descriptorImageInfo.imageView = textures[0]->view;
        descriptorImageInfos.push_back(descriptorImageInfo);
    }

    VkWriteDescriptorSet writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeDescriptorSet.dstBinding           = 1;
    writeDescriptorSet.dstArrayElement      = 0;
    writeDescriptorSet.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.descriptorCount      = static_cast<uint32_t>(descriptorImageInfos.size());
    writeDescriptorSet.pImageInfo           = descriptorImageInfos.data();
    writeDescriptorSet.dstSet               = m_descriptorSet;

    vkDeviceWaitIdle(m_device);
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
}

const Buffer* const Renderer::getUniformBuffer() const { return m_uniformBuffer.get(); }

std::unique_ptr<Image> Renderer::createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
                                             const VkImageAspectFlags& aspectMask, const char* name) {
    const VkImage image = createVkImage(imageSize, imageUsageFlags, imageFormat);

#ifdef VALIDATION_ENABLED
    nameObject(image, VK_OBJECT_TYPE_IMAGE, name);
#endif

    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(m_device, image, &imageMemoryRequirements);
    const VkDeviceMemory memory = allocateVulkanObjectMemory(imageMemoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkBindImageMemory(m_device, image, memory, 0);

    VkImageView view = VK_NULL_HANDLE;
    if (aspectMask != 0) {
        view = createImageView(image, imageFormat, aspectMask);
    }

    std::unique_ptr<Image> imagePtr = std::make_unique<Image>(m_device, image, memory, view, imageSize);

    return std::move(imagePtr);
}

std::unique_ptr<Buffer> Renderer::createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                               const VkMemoryPropertyFlags& memoryPropertyFlags, const char* name) {
    const VkBuffer buffer = createVkBuffer(bufferSize, bufferUsageFlags);
#ifdef VALIDATION_ENABLED
    nameObject(buffer, VK_OBJECT_TYPE_BUFFER, name);
#endif

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

    const VkDeviceMemory memory = allocateVulkanObjectMemory(memoryRequirements, memoryPropertyFlags);
    vkBindBufferMemory(m_device, buffer, memory, 0);

    return std::make_unique<Buffer>(m_device, buffer, memory);
}

void Renderer::uploadToDeviceLocalImage(const void* data, const uint32_t& imageSize, const Image& deviceImage, const VkImageLayout& initialLayout,
                                        const VkImageLayout& finalLayout) {
    void* stagingBufferPointer;
    VK_CHECK(vkMapMemory(m_device, m_stagingBuffer->memory, 0, imageSize, 0, &stagingBufferPointer));
    memcpy(stagingBufferPointer, reinterpret_cast<const uint8_t*>(data), imageSize);
    vkUnmapMemory(m_device, m_stagingBuffer->memory);

    VkCommandBufferAllocateInfo transferCommandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    transferCommandBufferAllocateInfo.commandPool                 = m_commandPool;
    transferCommandBufferAllocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    transferCommandBufferAllocateInfo.commandBufferCount          = 1;

    VkCommandBuffer transferCommandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &transferCommandBufferAllocateInfo, &transferCommandBuffer));

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

    vkCmdCopyBufferToImage(transferCommandBuffer, m_stagingBuffer->buffer, deviceImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

    const VkImageMemoryBarrier imageMemoryBarrierAfter = createImageMemoryBarrier(deviceImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout);

    vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrierAfter);

    VK_CHECK(vkEndCommandBuffer(transferCommandBuffer));

    VkSubmitInfo transferSubmitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    transferSubmitInfo.commandBufferCount = 1;
    transferSubmitInfo.pCommandBuffers    = &transferCommandBuffer;

    VK_CHECK(vkQueueSubmit(m_queue, 1, &transferSubmitInfo, VK_NULL_HANDLE));
    vkDeviceWaitIdle(m_device);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &transferCommandBuffer);
}

void Renderer::uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& bufferMemory) {
    void* hostBufferPointer;
    VK_CHECK(vkMapMemory(m_device, bufferMemory, 0, bufferSize, 0, &hostBufferPointer));
    memcpy(hostBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(m_device, bufferMemory);
}

void Renderer::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        throw std::runtime_error("Failed to initialize SDL!");
    }

    m_window = SDL_CreateWindow("Breakout", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);

    if (!m_window) {
        throw std::runtime_error("Failed to create SDL window!");
    }
}

#ifdef VALIDATION_ENABLED
void Renderer::setupDebugUtils() {
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
#if INFO
    debugUtilsMessengerCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
#endif
#if VERBOSE
    debugUtilsMessengerCreateInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
#endif
    debugUtilsMessengerCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.pfnUserCallback = debugUtilsCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &debugUtilsMessengerCreateInfo, nullptr, &m_debugUtilsMessenger));
}
#endif

void Renderer::createInstance() {
    VkApplicationInfo applicationInfo  = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.apiVersion         = VK_API_VERSION_1_0;
    applicationInfo.applicationVersion = 0;
    applicationInfo.pApplicationName   = NULL;
    applicationInfo.pEngineName        = NULL;
    applicationInfo.engineVersion      = 0;

    VkInstanceCreateInfo createInfo  = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo      = &applicationInfo;
    createInfo.enabledLayerCount     = 0;
    createInfo.enabledExtensionCount = 0;

#ifdef VALIDATION_ENABLED
    std::vector<const char*> layers = {
#if API_DUMP
        "VK_LAYER_LUNARG_api_dump",
#endif
        "VK_LAYER_KHRONOS_validation"
    };

    createInfo.enabledLayerCount   = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();
#endif

    uint32_t sdlExtensionCount;
    SDL_Vulkan_GetInstanceExtensions(m_window, &sdlExtensionCount, nullptr);
    std::vector<const char*> extensions(sdlExtensionCount);
    SDL_Vulkan_GetInstanceExtensions(m_window, &sdlExtensionCount, extensions.data());

#ifdef VALIDATION_ENABLED
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_instance));
    volkLoadInstance(m_instance);
}

void Renderer::pickPhysicalDevice() {
    uint32_t physicalDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, 0));

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data()));

    for (size_t i = 0; i < physicalDeviceCount; ++i) {

        m_physicalDevice = physicalDevices[i];

        VkPhysicalDeviceProperties physicalDeviceProperties = {};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

        if (physicalDeviceProperties.apiVersion < VK_API_VERSION_1_0) {
            continue;
        }

        m_queueFamilyIndex = getGenericQueueFamilyIndex(m_physicalDevice);
        if (m_queueFamilyIndex == UINT32_MAX) {
            continue;
        }

        VkBool32 presentSupported;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_queueFamilyIndex, m_surface, &presentSupported));

        if (presentSupported == VK_FALSE) {
            continue;
        }

        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_physicalDeviceMemoryProperties);

        return;
    }

    throw std::runtime_error("No suitable GPU found!");
}

void Renderer::createDevice() {
    const float             queuePriorities  = 1.0f;
    VkDeviceQueueCreateInfo queueCrreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCrreateInfo.queueCount              = 1;
    queueCrreateInfo.queueFamilyIndex        = m_queueFamilyIndex;
    queueCrreateInfo.pQueuePriorities        = &queuePriorities;

    std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount    = 1;
    createInfo.pQueueCreateInfos       = &queueCrreateInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2                       = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    physicalDeviceFeatures2.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

    createInfo.pNext = &physicalDeviceFeatures2;

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));
    volkLoadDevice(m_device);
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    createInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex        = m_queueFamilyIndex;

    VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool));
}

void Renderer::createRenderPass() {
    std::array<VkAttachmentDescription, 2> attachments;
    attachments.fill({});

    attachments[0].format        = m_swapchain->getSurfaceFormat().format;
    attachments[0].samples       = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format         = VK_FORMAT_D32_SFLOAT_S8_UINT;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass    = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo createInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount        = static_cast<uint32_t>(attachments.size());
    createInfo.pAttachments           = attachments.data();
    createInfo.subpassCount           = 1;
    createInfo.pSubpasses             = &subpass;

    VK_CHECK(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass));
}

void Renderer::createFramebuffers() {
    VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass              = m_renderPass;
    createInfo.attachmentCount         = 2;
    createInfo.width                   = m_surfaceExtent.width;
    createInfo.height                  = m_surfaceExtent.height;
    createInfo.layers                  = 1;

    m_framebuffers = std::vector<VkFramebuffer>(m_swapchainImageCount);
    std::array<VkImageView, 2> attachments({});
    attachments[1] = m_depthImage->view;

    const std::vector<VkImageView>& swapchainImageViews = m_swapchain->getImageViews();
    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        attachments[0]          = swapchainImageViews[i];
        createInfo.pAttachments = attachments.data();

        VK_CHECK(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffers[i]));
    }
}

void Renderer::createSampler() {
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

    VK_CHECK(vkCreateSampler(m_device, &createInfo, nullptr, &m_sampler));
}

void Renderer::createDescriptorLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings;

    // Uniform buffer
    descriptorSetLayoutBindings[0].binding            = 0;
    descriptorSetLayoutBindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutBindings[0].descriptorCount    = 1;
    descriptorSetLayoutBindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBindings[0].pImmutableSamplers = nullptr;

    // Sampled texture array
    std::vector<VkSampler> samplers(MAX_TEXTURE_COUNT, m_sampler);
    descriptorSetLayoutBindings[1].binding            = 1;
    descriptorSetLayoutBindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBindings[1].descriptorCount    = static_cast<uint32_t>(samplers.size());
    descriptorSetLayoutBindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBindings[1].pImmutableSamplers = samplers.data();

    VkDescriptorSetLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount                    = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
    createInfo.pBindings                       = descriptorSetLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_descriptorSetLayout));
}

void Renderer::createPipelineCache() {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    pipelineCacheCreateInfo.initialDataSize           = 0;

    VK_CHECK(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));
}

void Renderer::createPipelineLayout() {
    VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.pushConstantRangeCount     = 0;
    createInfo.pPushConstantRanges        = nullptr;
    createInfo.setLayoutCount             = 1;
    createInfo.pSetLayouts                = &m_descriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout));
}

void Renderer::createPipeline() {
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages.fill({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});

    VkShaderModule vertexShader   = loadShader("src/shaders/spirv/vertexShader.spv");
    VkShaderModule fragmentShader = loadShader("src/shaders/spirv/fragmentShader.spv");

    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName  = "main";

    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName  = "main";

    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages    = shaderStages.data();

    std::array<VkVertexInputBindingDescription, 2> vertexInputBindingDescriptions;
    vertexInputBindingDescriptions[0].binding   = VERTEX_BUFFER_BIND_ID;
    vertexInputBindingDescriptions[0].stride    = sizeof(Vertex);
    vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexInputBindingDescriptions[1].binding   = INSTANCE_BUFFER_BIND_ID;
    vertexInputBindingDescriptions[1].stride    = sizeof(Instance);
    vertexInputBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 10> vertexInputAttributeDescriptions;
    uint32_t                                          inputAttributeIndex = 0;

    // Vertex position
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = VERTEX_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 0;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Vertex, position);
    ++inputAttributeIndex;

    // Instance position
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 1;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, position);
    ++inputAttributeIndex;

    // Instance depth
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 2;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, depth);
    ++inputAttributeIndex;

    // Instance scale
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 3;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, scale);
    ++inputAttributeIndex;

    // Instance texture index
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 4;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32_UINT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, textureIndex);
    ++inputAttributeIndex;

    // Instance texture alpha
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 5;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, textureAlpha);
    ++inputAttributeIndex;

    // Instance UV offset
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 6;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, uvOffset);
    ++inputAttributeIndex;

    // Instance UV scale
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 7;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, uvScale);
    ++inputAttributeIndex;

    // Instance health
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 8;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32_UINT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, health);
    ++inputAttributeIndex;

    // Instance max health
    vertexInputAttributeDescriptions[inputAttributeIndex].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[inputAttributeIndex].location = 9;
    vertexInputAttributeDescriptions[inputAttributeIndex].format   = VK_FORMAT_R32_UINT;
    vertexInputAttributeDescriptions[inputAttributeIndex].offset   = offsetof(Instance, maxHealth);
    ++inputAttributeIndex;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputStateCreateInfo.pVertexBindingDescriptions           = vertexInputBindingDescriptions.data();
    vertexInputStateCreateInfo.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertexInputBindingDescriptions.size());
    vertexInputStateCreateInfo.pVertexAttributeDescriptions         = vertexInputAttributeDescriptions.data();
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());

    createInfo.pVertexInputState = &vertexInputStateCreateInfo;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyStateCreateInfo.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    createInfo.pInputAssemblyState                                      = &inputAssemblyStateCreateInfo;

    VkViewport viewport = {};
    viewport.width      = WINDOW_WIDTH;
    viewport.height     = WINDOW_HEIGHT;
    viewport.x          = 0;
    viewport.y          = 0;
    viewport.minDepth   = 1.0f;
    viewport.maxDepth   = 0.0f;

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = {WINDOW_WIDTH, WINDOW_HEIGHT};

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateCreateInfo.pViewports                        = &viewport;
    viewportStateCreateInfo.viewportCount                     = 1;
    viewportStateCreateInfo.pScissors                         = &scissor;
    viewportStateCreateInfo.scissorCount                      = 1;
    createInfo.pViewportState                                 = &viewportStateCreateInfo;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationStateCreateInfo.lineWidth                              = 1.0f;
    rasterizationStateCreateInfo.frontFace                              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.cullMode                               = VK_CULL_MODE_BACK_BIT;
    createInfo.pRasterizationState                                      = &rasterizationStateCreateInfo;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleStateCreateInfo.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    createInfo.pMultisampleState                                    = &multisampleStateCreateInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilStateCreateInfo.depthTestEnable                       = true;
    depthStencilStateCreateInfo.depthWriteEnable                      = true;
    depthStencilStateCreateInfo.depthCompareOp                        = VK_COMPARE_OP_GREATER;
    createInfo.pDepthStencilState                                     = &depthStencilStateCreateInfo;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
    colorBlendAttachmentState.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable         = VK_TRUE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.alphaBlendOp        = VK_BLEND_OP_SUBTRACT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendStateCreateInfo.attachmentCount                     = 1;
    colorBlendStateCreateInfo.pAttachments                        = &colorBlendAttachmentState;
    createInfo.pColorBlendState                                   = &colorBlendStateCreateInfo;

    createInfo.layout     = m_pipelineLayout;
    createInfo.renderPass = m_renderPass;

    VK_CHECK(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &m_pipeline));

    vkDestroyShaderModule(m_device, vertexShader, nullptr);
    vkDestroyShaderModule(m_device, fragmentShader, nullptr);
}

void Renderer::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 1> descriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

    VkDescriptorPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount              = static_cast<uint32_t>(descriptorPoolSizes.size());
    createInfo.pPoolSizes                 = descriptorPoolSizes.data();
    createInfo.maxSets                    = 1;

    VK_CHECK(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool));
}

void Renderer::allocateDescriptorSet() {
    VkDescriptorSetAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool              = m_descriptorPool;
    allocateInfo.descriptorSetCount          = 1;
    allocateInfo.pSetLayouts                 = &m_descriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_device, &allocateInfo, &m_descriptorSet));
}

void Renderer::allocateRenderCommandBuffers() {
    VkCommandBufferAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount          = m_swapchainImageCount;
    allocateInfo.commandPool                 = m_commandPool;

    m_renderCommandBuffers = std::vector<VkCommandBuffer>(m_swapchainImageCount);
    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        VK_CHECK(vkAllocateCommandBuffers(m_device, &allocateInfo, m_renderCommandBuffers.data()));
    }
}

void Renderer::createSyncObjects() {
    m_imageAvailableSemaphores = std::vector<VkSemaphore>(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores = std::vector<VkSemaphore>(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences           = std::vector<VkFence>(MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight           = std::vector<VkFence>(m_swapchainImageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo     fenceCreateInfo     = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags                     = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]));
    }
}

void Renderer::setupRenderLoop() {
    m_renderSubmitInfo                      = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    m_renderSubmitInfo.waitSemaphoreCount   = 1;
    m_renderSubmitInfo.pWaitDstStageMask    = &m_renderSubmitWaitStage;
    m_renderSubmitInfo.commandBufferCount   = 1;
    m_renderSubmitInfo.signalSemaphoreCount = 1;

    m_presentInfo                    = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    m_presentInfo.waitSemaphoreCount = 1;
    m_presentInfo.swapchainCount     = 1;
    m_presentInfo.pSwapchains        = &m_swapchain->get();
}

void Renderer::writeDescriptorSet() {
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer                 = m_uniformBuffer->buffer;
    descriptorBufferInfo.offset                 = 0;
    descriptorBufferInfo.range                  = sizeof(UniformData);

    // Uniform buffer
    VkWriteDescriptorSet writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeDescriptorSet.dstBinding           = 0;
    writeDescriptorSet.dstArrayElement      = 0;
    writeDescriptorSet.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.descriptorCount      = 1;
    writeDescriptorSet.pBufferInfo          = &descriptorBufferInfo;
    writeDescriptorSet.dstSet               = m_descriptorSet;

    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
}

void Renderer::resetRenderCommandBuffers() {
    vkDeviceWaitIdle(m_device);
    vkFreeCommandBuffers(m_device, m_commandPool, m_swapchainImageCount, m_renderCommandBuffers.data());

    m_renderCommandBuffers.clear();
    allocateRenderCommandBuffers();
}

void Renderer::recordRenderCommandBuffer(const uint32_t& frameIndex, const VkBuffer& instanceBuffer, const uint32_t& instanceCount) const {
    VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    VK_CHECK(vkBeginCommandBuffer(m_renderCommandBuffers[frameIndex], &commandBufferBeginInfo));

    VkRenderPassBeginInfo beginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    beginInfo.renderPass            = m_renderPass;
    beginInfo.renderArea.offset     = {0, 0};
    beginInfo.renderArea.extent     = m_surfaceExtent;

    VkClearValue                colorImageClearColor = {0.0f, 0.0f, 0.2f, 1.0f};
    VkClearValue                depthImageClearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<VkClearValue, 2> imageClearColors     = {colorImageClearColor, depthImageClearColor};
    beginInfo.clearValueCount                        = static_cast<uint32_t>(imageClearColors.size());
    beginInfo.pClearValues                           = imageClearColors.data();
    beginInfo.framebuffer                            = m_framebuffers[frameIndex];
    vkCmdBeginRenderPass(m_renderCommandBuffers[frameIndex], &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m_renderCommandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    vkCmdBindDescriptorSets(m_renderCommandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_renderCommandBuffers[frameIndex], VERTEX_BUFFER_BIND_ID, 1, &m_vertexBuffer->buffer, &offset);
    vkCmdBindVertexBuffers(m_renderCommandBuffers[frameIndex], INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer, &offset);

    vkCmdBindIndexBuffer(m_renderCommandBuffers[frameIndex], m_indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(m_renderCommandBuffers[frameIndex], 6, instanceCount, 0, 0, 0);

    vkCmdEndRenderPass(m_renderCommandBuffers[frameIndex]);

    VK_CHECK(vkEndCommandBuffer(m_renderCommandBuffers[frameIndex]));
}

const uint32_t Renderer::getGenericQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const {
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, 0);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t     queueFamilyIndex = UINT32_MAX;
    VkQueueFlags queueFlags       = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    for (size_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & queueFlags) {
            queueFamilyIndex = static_cast<uint32_t>(i);
            break;
        }
    }

    return queueFamilyIndex;
}

const VkShaderModule Renderer::loadShader(const char* pathToSource) const {
    FILE* source;
    fopen_s(&source, pathToSource, "rb");
    assert(source);

    fseek(source, 0, SEEK_END);
    size_t length = static_cast<size_t>(ftell(source));
    assert(length > 0);
    fseek(source, 0, SEEK_SET);

    char* buffer = new char[length];
    fread(buffer, 1, length, source);

    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize                 = length;
    createInfo.pCode                    = reinterpret_cast<uint32_t*>(buffer);

    VkShaderModule shaderModule = 0;
    VK_CHECK(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule));

    delete[] buffer;

    return shaderModule;
}

#ifdef VALIDATION_ENABLED
void Renderer::nameObject(void* handle, const VkObjectType& type, const char* name) {
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    objectNameInfo.objectHandle                  = reinterpret_cast<uint64_t>(handle);
    objectNameInfo.objectType                    = type;
    objectNameInfo.pObjectName                   = name;

    VK_CHECK(vkSetDebugUtilsObjectNameEXT(m_device, &objectNameInfo));
}
#endif

void Renderer::createVertexAndIndexBuffers() {
    std::vector<Vertex>   vertices = {{{-0.5, -0.5}}, {{-0.5, 0.5}}, {{0.5, -0.5}}, {{0.5, 0.5}}};
    std::vector<uint16_t> indices  = {0, 1, 2, 1, 3, 2};

    m_vertexBuffer = createBuffer(VECTOR_SIZE_IN_BYTES(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Vertex buffer");
    uploadToDeviceLocalBuffer(vertices.data(), VECTOR_SIZE_IN_BYTES(vertices), m_vertexBuffer->buffer);

    m_indexBuffer = createBuffer(VECTOR_SIZE_IN_BYTES(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Index buffer");
    uploadToDeviceLocalBuffer(indices.data(), VECTOR_SIZE_IN_BYTES(indices), m_indexBuffer->buffer);
}

#ifdef VALIDATION_ENABLED
VkBool32 VKAPI_CALL Renderer::debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/) {
    const char* severity = (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                               ? "ERROR"
                               : (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
                                     ? "WARNING"
                                     : (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" : "VERBOSE";

    const char* type = (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
                           ? "GENERAL"
                           : (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) ? "VALIDATION" : "PERFORMANCE";

    printf("%s-%s: %s\n\n", severity, type, pCallbackData->pMessage);

    return VK_FALSE;
}
#endif

void Renderer::uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const VkBuffer& deviceBuffer) {
    void* stagingBufferPointer;
    VK_CHECK(vkMapMemory(m_device, m_stagingBuffer->memory, 0, bufferSize, 0, &stagingBufferPointer));
    memcpy(stagingBufferPointer, reinterpret_cast<const uint8_t*>(data), bufferSize);
    vkUnmapMemory(m_device, m_stagingBuffer->memory);

    VkCommandBufferAllocateInfo transferCommandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    transferCommandBufferAllocateInfo.commandPool                 = m_commandPool;
    transferCommandBufferAllocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    transferCommandBufferAllocateInfo.commandBufferCount          = 1;

    VkCommandBuffer transferCommandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &transferCommandBufferAllocateInfo, &transferCommandBuffer));

    VkCommandBufferBeginInfo transferCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(transferCommandBuffer, &transferCommandBufferBeginInfo));

    VkBufferCopy bufferCopy = {};
    bufferCopy.srcOffset    = 0;
    bufferCopy.dstOffset    = 0;
    bufferCopy.size         = bufferSize;
    vkCmdCopyBuffer(transferCommandBuffer, m_stagingBuffer->buffer, deviceBuffer, 1, &bufferCopy);
    VK_CHECK(vkEndCommandBuffer(transferCommandBuffer));

    VkSubmitInfo transferSubmitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    transferSubmitInfo.commandBufferCount = 1;
    transferSubmitInfo.pCommandBuffers    = &transferCommandBuffer;

    VK_CHECK(vkQueueSubmit(m_queue, 1, &transferSubmitInfo, VK_NULL_HANDLE));
    vkDeviceWaitIdle(m_device);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &transferCommandBuffer);
}

const VkImage Renderer::createVkImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat) {
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
    VK_CHECK(vkCreateImage(m_device, &createInfo, nullptr, &image));

    return image;
}

const VkImageView Renderer::createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask) {
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
    VK_CHECK(vkCreateImageView(m_device, &createInfo, nullptr, &imageView));

    return imageView;
}

const VkImageMemoryBarrier Renderer::createImageMemoryBarrier(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout) {
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

const VkBuffer Renderer::createVkBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags) {
    VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    createInfo.size               = bufferSize;
    createInfo.usage              = bufferUsageFlags;
    createInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = 0;
    VK_CHECK(vkCreateBuffer(m_device, &createInfo, nullptr, &buffer));

    return buffer;
}

const uint32_t Renderer::findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags) {
    uint32_t memoryType = UINT32_MAX;

    for (uint32_t i = 0; i < m_physicalDeviceMemoryProperties.memoryTypeCount; ++i) {
        bool memoryIsOfRequiredType        = memoryTypeBits & (1 << i);
        bool memoryHasDesiredPropertyFlags = (m_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags;

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

const VkDeviceMemory Renderer::allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags) {

    uint32_t memoryType = findMemoryType(memoryRequirements.memoryTypeBits, memoryPropertyFlags);

    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryAllocateInfo.allocationSize       = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex      = memoryType;

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &memory));

    return memory;
}