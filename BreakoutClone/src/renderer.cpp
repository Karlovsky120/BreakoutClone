#include "renderer.h"

#include "sharedStructures.h"

#include <array>
#include <chrono>
#include <stdexcept>

Renderer::Renderer() {}

void Renderer::init(const char* vertexShaderPath, const char* fragmentShaderPath,
                    const VkPipelineVertexInputStateCreateInfo& pipelineVertexInputStateCreateInfo) {
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

    m_transferCommandPool = createCommandPool();
    m_stagingBuffer       = Resources::createBuffer(STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_swapchain           = std::make_unique<Swapchain>(m_window, m_surface, m_physicalDevice, m_device, m_queueFamilyIndex);
    m_surfaceExtent       = m_swapchain->getSurfaceExtent();
    m_swapchainImageCount = m_swapchain->getImageCounts();

    m_depthImage =
        Resources::createImage(m_surfaceExtent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

    createRenderPass();
    createFramebuffers();

    createDescriptorLayout();

    createPipelineCache();
    createPipelineLayout();
    createGraphicsPipeline(vertexShaderPath, fragmentShaderPath, pipelineVertexInputStateCreateInfo);

    createDescriptorPool();
    allocateDescriptorSet();
    createUniformBuffer();
    writeDescriptorSet();

    createRenderCommandPoolsAndAllocateBuffers();
    createSyncObjects();

    setupRenderLoop();
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

    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        vkFreeCommandBuffers(m_device, m_renderCommandPool, m_swapchainImageCount, m_renderCommandBuffers.data());
    }

    vkDestroyCommandPool(m_device, m_renderCommandPool, nullptr);
    vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);

    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

    for (VkFramebuffer& framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

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

Renderer* Renderer::m_object_instance;
Renderer* Renderer::getInstance() { return m_object_instance; }

void Renderer::initRenderer(const char* vertexShaderPath, const char* fragmentShaderPath,
                            const VkPipelineVertexInputStateCreateInfo& pipelineVertexInputStateCreateInfo) {
    m_object_instance = new Renderer();
    m_object_instance->init(vertexShaderPath, fragmentShaderPath, pipelineVertexInputStateCreateInfo);
}

const VkDevice&                         Renderer::getDevice() { return m_object_instance->m_device; };
const VkPhysicalDeviceProperties&       Renderer::getDeviceProperties() { return m_object_instance->m_physicalDeviceProperties; };
const VkPhysicalDeviceMemoryProperties& Renderer::getMemoryProperties() { return m_object_instance->m_physicalDeviceMemoryProperties; };
const VkCommandPool&                    Renderer::getTransferCommandPool() { return m_object_instance->m_transferCommandPool; };
const Buffer&                           Renderer::getStagingBuffer() { return *(m_object_instance->m_stagingBuffer); }
const VkQueue&                          Renderer::getQueue() { return m_object_instance->m_queue; }

void Renderer::setWindowTitle(const char* title) { SDL_SetWindowTitle(m_object_instance->m_window, title); }

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

void Renderer::recordRenderCommandBuffers(const VkBuffer& vertexBuffer, const uint32_t& vertexBufferBindId, const VkBuffer& indexBuffer,
                                          const VkBuffer& instanceBuffer, const uint32_t& instanceBufferBindId, const VkBuffer& drawCommandBuffer) {
    for (uint32_t i = 0; i < getInstance()->m_swapchainImageCount; ++i) {
        getInstance()->recordRenderCommandBuffer(i, vertexBuffer, vertexBufferBindId, indexBuffer, instanceBuffer, instanceBufferBindId, drawCommandBuffer);
    }
}

void Renderer::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        throw std::runtime_error("Failed to initialize SDL!");
    }

    m_window = SDL_CreateWindow("Breakout", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    if (!m_window) {
        throw std::runtime_error("Failed to create SDL window!");
    }

    SDL_CaptureMouse(static_cast<SDL_bool>(true));
    SDL_SetRelativeMouseMode(static_cast<SDL_bool>(true));
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
    applicationInfo.apiVersion         = VK_API_VERSION_1_2;
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

        if (physicalDeviceProperties.apiVersion < VK_API_VERSION_1_2) {
            continue;
        }

        if (physicalDeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
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

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2  = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    physicalDeviceFeatures2.features.multiDrawIndirect = VK_TRUE;

    createInfo.pNext = &physicalDeviceFeatures2;

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));
    volkLoadDevice(m_device);
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

void Renderer::createDescriptorLayout() {
    std::array<VkDescriptorSetLayoutBinding, 1> descriptorSetLayoutBindings;

    // Uniform buffer
    descriptorSetLayoutBindings[0].binding         = 0;
    descriptorSetLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[0].descriptorCount = 1;
    descriptorSetLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

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

void Renderer::createGraphicsPipeline(const char* vertexShaderPath, const char* fragmentShaderPath,
                                      const VkPipelineVertexInputStateCreateInfo& vertexInputStateCreateInfo) {
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages.fill({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});

    VkShaderModule vertexShader   = loadShader(vertexShaderPath);
    VkShaderModule fragmentShader = loadShader(fragmentShaderPath);

    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName  = "main";

    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName  = "main";

    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages    = shaderStages.data();

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
    rasterizationStateCreateInfo.cullMode                               = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
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
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

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
    std::array<VkDescriptorPoolSize, 1> descriptorPoolSizes = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

    VkDescriptorPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount              = static_cast<uint32_t>(descriptorPoolSizes.size());
    createInfo.pPoolSizes                 = descriptorPoolSizes.data();
    createInfo.maxSets                    = 1;
    VK_CHECK(vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_descriptorPool));
}

void Renderer::allocateDescriptorSet() {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool              = m_descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount          = 1;
    descriptorSetAllocateInfo.pSetLayouts                 = &m_descriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSet));
}

void Renderer::createUniformBuffer() {
    m_uniformBuffer = Resources::createBuffer(sizeof(UniformData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    UniformData uniformData = {1.0f / WINDOW_WIDTH, 1.0f / WINDOW_HEIGHT};

    Resources::uploadToDeviceLocalBuffer(&uniformData, sizeof(uniformData), *m_stagingBuffer, m_uniformBuffer->buffer);
}

void Renderer::writeDescriptorSet() {
    VkDescriptorBufferInfo descriptorBufferInfo;
    descriptorBufferInfo.buffer = m_uniformBuffer->buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range  = sizeof(UniformData);

    VkWriteDescriptorSet writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

    writeDescriptorSet.dstBinding      = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pBufferInfo     = &descriptorBufferInfo;
    writeDescriptorSet.dstSet          = m_descriptorSet;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
}

void Renderer::createRenderCommandPoolsAndAllocateBuffers() {
    VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    createInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex        = m_queueFamilyIndex;

    VK_CHECK(vkCreateCommandPool(m_device, &createInfo, nullptr, &m_renderCommandPool));

    VkCommandBufferAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount          = m_swapchainImageCount;
    allocateInfo.commandPool                 = m_renderCommandPool;

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

void Renderer::recordRenderCommandBuffer(const uint32_t& frameIndex, const VkBuffer& vertexBuffer, const uint32_t& vertexBufferBindId,
                                         const VkBuffer& indexBuffer, const VkBuffer& instanceBuffer, const uint32_t& instanceBufferBindId,
                                         const VkBuffer& drawCommandBuffer) const {
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
    vkCmdBindVertexBuffers(m_renderCommandBuffers[frameIndex], vertexBufferBindId, 1, &vertexBuffer, &offset);
    vkCmdBindVertexBuffers(m_renderCommandBuffers[frameIndex], instanceBufferBindId, 1, &instanceBuffer, &offset);

    vkCmdBindIndexBuffer(m_renderCommandBuffers[frameIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    if (m_physicalDeviceProperties.limits.maxDrawIndirectCount >= 2) {
        vkCmdDrawIndexedIndirect(m_renderCommandBuffers[frameIndex], drawCommandBuffer, 0, 2, sizeof(VkDrawIndexedIndirectCommand));
    } else {
        vkCmdDrawIndexedIndirect(m_renderCommandBuffers[frameIndex], drawCommandBuffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
        vkCmdDrawIndexedIndirect(m_renderCommandBuffers[frameIndex], drawCommandBuffer, sizeof(VkDrawIndexedIndirectCommand), 1,
                                 sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndRenderPass(m_renderCommandBuffers[frameIndex]);

    VK_CHECK(vkEndCommandBuffer(m_renderCommandBuffers[frameIndex]));
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
    if (fread(buffer, 1, length, source) != length) {
        assert(false);
    }

    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize                 = length;
    createInfo.pCode                    = reinterpret_cast<uint32_t*>(buffer);

    VkShaderModule shaderModule = 0;
    VK_CHECK(vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule));

    delete[] buffer;

    return shaderModule;
}

const VkCommandPool Renderer::createCommandPool() {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex        = m_queueFamilyIndex;

    VkCommandPool commandPool = 0;
    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &commandPool));

    return commandPool;
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