#include "renderer.h"

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

    m_swapchain           = std::make_unique<Swapchain>(m_window, m_surface, m_physicalDevice, m_device, m_queueFamilyIndex);
    m_surfaceExtent       = m_swapchain->getSurfaceExtent();
    m_swapchainImageCount = m_swapchain->getImageCounts();

    m_depthImage =
        Resources::createImage(m_surfaceExtent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

    createRenderPass();
    createFramebuffers();
    createPipelineCache();
    createPipelineLayout();
    createGraphicsPipeline(vertexShaderPath, fragmentShaderPath, pipelineVertexInputStateCreateInfo);

    createRenderCommandPoolsAndBuffers();
    createSyncObjects();

    m_transferCommandPool = createCommandPool();
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

Renderer*       Renderer::m_object_instance;
const Renderer* Renderer::getInstance() { return m_object_instance; }

void Renderer::initRenderer(const char* vertexShaderPath, const char* fragmentShaderPath,
                            const VkPipelineVertexInputStateCreateInfo& pipelineVertexInputStateCreateInfo) {
    m_object_instance = new Renderer();
    m_object_instance->init(vertexShaderPath, fragmentShaderPath, pipelineVertexInputStateCreateInfo);
}

const VkDevice&                         Renderer::getDevice() { return m_object_instance->m_device; };
const VkPhysicalDeviceProperties&       Renderer::getDeviceProperties() { return m_object_instance->m_physicalDeviceProperties; };
const VkPhysicalDeviceMemoryProperties& Renderer::getMemoryProperties() { return m_object_instance->m_physicalDeviceMemoryProperties; };
const VkCommandPool&                    Renderer::getTransferCommandPool() { return m_object_instance->m_transferCommandPool; };
const VkQueue&                          Renderer::getQueue() { return m_object_instance->m_queue; }

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
    const float             queuePriorities       = 1.0f;
    VkDeviceQueueCreateInfo deviceQueueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    deviceQueueCreateInfo.queueCount              = 1;
    deviceQueueCreateInfo.queueFamilyIndex        = m_queueFamilyIndex;
    deviceQueueCreateInfo.pQueuePriorities        = &queuePriorities;

    std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceCreateInfo      = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount    = 1;
    deviceCreateInfo.pQueueCreateInfos       = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2  = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    physicalDeviceFeatures2.features.multiDrawIndirect = VK_TRUE;

    deviceCreateInfo.pNext = &physicalDeviceFeatures2;

    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));
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

    VkRenderPassCreateInfo renderPassCreateInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassCreateInfo.attachmentCount        = static_cast<uint32_t>(attachments.size());
    renderPassCreateInfo.pAttachments           = attachments.data();
    renderPassCreateInfo.subpassCount           = 1;
    renderPassCreateInfo.pSubpasses             = &subpass;

    VK_CHECK(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &m_renderPass));
}

void Renderer::createFramebuffers() {
    VkFramebufferCreateInfo framebufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferCreateInfo.renderPass              = m_renderPass;
    framebufferCreateInfo.attachmentCount         = 2;
    framebufferCreateInfo.width                   = m_surfaceExtent.width;
    framebufferCreateInfo.height                  = m_surfaceExtent.height;
    framebufferCreateInfo.layers                  = 1;

    m_framebuffers = std::vector<VkFramebuffer>(m_swapchainImageCount);
    std::array<VkImageView, 2> attachments({});
    attachments[1] = m_depthImage->view;

    const std::vector<VkImageView>& swapchainImageViews = m_swapchain->getImageViews();
    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        attachments[0]                     = swapchainImageViews[i];
        framebufferCreateInfo.pAttachments = attachments.data();
        VK_CHECK(vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &m_framebuffers[i]));
    }
}

void Renderer::createPipelineCache() {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    pipelineCacheCreateInfo.initialDataSize           = 0;
    VK_CHECK(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));
}

const VkCommandPool Renderer::createCommandPool() {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex        = m_queueFamilyIndex;

    VkCommandPool commandPool = 0;
    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &commandPool));

    return commandPool;
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

void Renderer::createPipelineLayout() {
    VkPipelineLayoutCreateInfo rasterPipelineLayoutCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    rasterPipelineLayoutCreateInfo.pushConstantRangeCount     = 0;
    rasterPipelineLayoutCreateInfo.pPushConstantRanges        = nullptr;
    rasterPipelineLayoutCreateInfo.setLayoutCount             = 0;
    rasterPipelineLayoutCreateInfo.pSetLayouts                = nullptr;
    VK_CHECK(vkCreatePipelineLayout(m_device, &rasterPipelineLayoutCreateInfo, nullptr, &m_pipelineLayout));
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

void Renderer::createRenderCommandPoolsAndBuffers() {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex        = m_queueFamilyIndex;

    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_renderCommandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandBufferAllocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount          = m_swapchainImageCount;
    commandBufferAllocateInfo.commandPool                 = m_renderCommandPool;

    m_renderCommandBuffers = std::vector<VkCommandBuffer>(m_swapchainImageCount);
    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, m_renderCommandBuffers.data()));
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

    VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBeginInfo.renderPass            = m_renderPass;
    renderPassBeginInfo.renderArea.offset     = {0, 0};
    renderPassBeginInfo.renderArea.extent     = m_surfaceExtent;

    VkClearValue                colorImageClearColor = {0.0f, 0.0f, 0.2f, 1.0f};
    VkClearValue                depthImageClearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    std::array<VkClearValue, 2> imageClearColors     = {colorImageClearColor, depthImageClearColor};
    renderPassBeginInfo.clearValueCount              = static_cast<uint32_t>(imageClearColors.size());
    renderPassBeginInfo.pClearValues                 = imageClearColors.data();
    renderPassBeginInfo.framebuffer                  = m_framebuffers[frameIndex];
    vkCmdBeginRenderPass(m_renderCommandBuffers[frameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(m_renderCommandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

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

void Renderer::renderLoop() {
    uint32_t currentFrame = 0;
    bool     updatedUI    = false;

    std::chrono::high_resolution_clock::time_point oldTime = std::chrono::high_resolution_clock::now();
    uint32_t                                       time    = 0;

    SDL_Event sdlEvent;
    bool      quit = false;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkSubmitInfo submitInfo         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.signalSemaphoreCount = 1;

    const VkSwapchainKHR& swapchain = m_swapchain->get();

    VkPresentInfoKHR presentInfo   = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain;

    while (!quit) {
        while (SDL_PollEvent(&sdlEvent)) {
            switch (sdlEvent.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                // m_keyStates[sdlEvent.key.keysym.sym].pressed = true;
                break;
            case SDL_KEYUP:
                // m_keyStates[sdlEvent.key.keysym.sym].pressed = false;
                break;
            }
        }

        vkWaitForFences(m_device, 1, &m_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain->get(), UINT64_MAX, m_imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex));

        if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_imagesInFlight[imageIndex] = m_inFlightFences[currentFrame];

        std::chrono::high_resolution_clock::time_point newTime = std::chrono::high_resolution_clock::now();
        uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(newTime - oldTime).count());
        oldTime            = newTime;
        time += frameTime;

        if (time > 500'000 || updatedUI) {
            char title[256];
            sprintf_s(title, "Breakout! Frametime: %.2fms", frameTime / 1'000.0f);
            SDL_SetWindowTitle(m_window, title);
            time      = 0;
            updatedUI = false;
        }

        vkResetFences(m_device, 1, &m_inFlightFences[currentFrame]);

        submitInfo.pWaitSemaphores   = &m_imageAvailableSemaphores[currentFrame];
        submitInfo.pCommandBuffers   = &m_renderCommandBuffers[imageIndex];
        submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[currentFrame];
        VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, m_inFlightFences[currentFrame]));

        presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[currentFrame];
        presentInfo.pImageIndices   = &imageIndex;
        VK_CHECK(vkQueuePresentKHR(m_queue, &presentInfo));

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(m_device);
}

void Renderer::runRenderLoop() { m_object_instance->renderLoop(); }

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