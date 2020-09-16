#define VOLK_IMPLEMENTATION
#include "breakout.h"

#include "commandPools.h"
#include "level.h"

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/fwd.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/mat4x4.hpp"
#pragma warning(pop)

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

#define API_DUMP 0
#define VERBOSE  0
#define INFO     0

#define PI 3.1415926535897932384f

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define FOV           glm::radians(100.0f)
#define NEAR          0.001f

#define ACCELERATION_FACTOR 300.0f

#define STAGING_BUFFER_SIZE 67'108'864 // 64MB

#define MAX_FRAMES_IN_FLIGHT    2
#define FRAMERATE_UPDATE_PERIOD 500'000 // 0.5 seconds

#define MAX_COLUMN_COUNT 50
#define MAX_ROW_COUNT    40

#define MAX_COLUMN_SPACING 5
#define MAX_ROW_SPACING    5

#define BALL_INDEX        0
#define PAD_INDEX         1
#define LEFT_WALL_INDEX   2
#define RIGHT_WALL_INDEX  3
#define BRICK_START_INDEX 4

#define VERTEX_BUFFER_BIND_ID   0
#define INSTANCE_BUFFER_BIND_ID 1

Breakout::~Breakout() {

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
        vkFreeCommandBuffers(m_device, m_commandPools[i], 1, &m_commandBuffers[i]);
        vkDestroyCommandPool(m_device, m_commandPools[i], nullptr);
    }

    vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);

    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

    vkDestroyPipeline(m_device, m_rayTracingPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_rayTracingPipelineLayout, nullptr);

    vkDestroyPipeline(m_device, m_rasterPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_rasterPipelineLayout, nullptr);
    vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);

    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

    vkDestroyBuffer(m_device, m_indexBuffer.buffer, nullptr);
    vkFreeMemory(m_device, m_indexBuffer.memory, nullptr);

    vkDestroyBuffer(m_device, m_vertexBuffer.buffer, nullptr);
    vkFreeMemory(m_device, m_vertexBuffer.memory, nullptr);

    for (VkFramebuffer& framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);

    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

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

void Breakout::run() {

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        throw std::runtime_error("Failed to initialize SDL!");
    }

    m_window = SDL_CreateWindow("Breakout", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    if (!m_window) {
        throw std::runtime_error("Failed to create SDL window!");
    }

    SDL_CaptureMouse(static_cast<SDL_bool>(true));
    SDL_SetRelativeMouseMode(static_cast<SDL_bool>(true));

    VK_CHECK(volkInitialize());

    assert(volkGetInstanceVersion() >= VK_API_VERSION_1_2);

    m_instance = createInstance();
    volkLoadInstance(m_instance);

#ifdef VALIDATION_ENABLED
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
#endif

    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface)) {
        throw std::runtime_error("Failed to create SDL Vulkan surface!");
    }

    m_physicalDevice = pickPhysicalDevice();

    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);

    printf("Selected GPU: %s\n", m_physicalDeviceProperties.deviceName);

    m_queueFamilyIndex = getGraphicsQueueFamilyIndex(m_physicalDevice);

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

    VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    physicalDeviceVulkan12Features.scalarBlockLayout                = VK_TRUE;
    physicalDeviceVulkan12Features.bufferDeviceAddress              = VK_TRUE;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2  = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    physicalDeviceFeatures2.features.multiDrawIndirect = VK_TRUE;
    physicalDeviceFeatures2.pNext                      = &physicalDeviceVulkan12Features;

    deviceCreateInfo.pNext = &physicalDeviceFeatures2;

    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

    volkLoadDevice(m_device);

    VkQueue queue = 0;
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &queue);

    VkSurfaceFormatKHR surfaceFormat = {};
    surfaceFormat.format             = VK_FORMAT_B8G8R8A8_UNORM;
    surfaceFormat.colorSpace         = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    m_swapchain           = std::make_unique<Swapchain>(m_window, m_surface, m_physicalDevice, m_device, m_queueFamilyIndex, surfaceFormat);
    m_surfaceExtent       = m_swapchain->getSurfaceExtent();
    m_swapchainImageCount = m_swapchain->getImageCounts();

    m_physicalDeviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_physicalDeviceMemoryProperties);

    m_depthImage = createImage(m_device, m_surfaceExtent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT_S8_UINT);
    VkMemoryRequirements depthImageMemoryRequirements;
    vkGetImageMemoryRequirements(m_device, m_depthImage, &depthImageMemoryRequirements);
    m_depthImageMemory =
        allocateVulkanObjectMemory(m_device, depthImageMemoryRequirements, m_physicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkBindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);

    m_depthImageView = createImageView(m_device, m_depthImage, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);

    m_renderPass   = createRenderPass();
    m_framebuffers = createFramebuffers();

    Buffer stagingBuffer =
        createBuffer(m_device, STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, m_physicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    m_transferCommandPool = createCommandPool(m_device, m_queueFamilyIndex);

    std::vector<glm::vec2> squareVertices = {{-0.5, -0.5}, {-0.5, 0.5}, {0.5, -0.5}, {0.5, 0.5}};
    std::vector<uint16_t>  squareIndices  = {0, 1, 2, 1, 3, 2};

    std::vector<glm::vec2> circleVertices;
    std::vector<uint16_t>  circleIndices;

    glm::vec2 currentVertex     = {0.5, 0.0};
    uint32_t  circleVertexCount = 36;
    float     step              = -2.0f * PI / circleVertexCount;
    uint16_t  currentIndex      = 1;

    circleVertices.push_back(currentVertex);
    for (uint32_t i = 0; i < circleVertexCount; ++i) {
        currentVertex = glm::rotate(currentVertex, step);
        circleVertices.push_back(currentVertex);

        circleIndices.push_back(0);
        circleIndices.push_back(currentIndex);
        ++currentIndex;
        circleIndices.push_back(currentIndex);
    }

    circleIndices.pop_back();
    circleIndices.push_back(1);

    uint32_t vertexBufferSize = sizeof(glm::vec2) * (static_cast<uint32_t>(squareVertices.size() + circleVertices.size()));
    Buffer   vertexBuffer     = createBuffer(m_device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       m_physicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uint32_t squareVertexOffset       = 0;
    uint32_t squareVertexBufferOffset = 0;
    uploadToDeviceLocalBuffer(m_device, squareVertices.data(), static_cast<uint32_t>(squareVertices.size()) * sizeof(glm::vec2), stagingBuffer.buffer,
                              stagingBuffer.memory, vertexBuffer.buffer, m_transferCommandPool, queue, squareVertexBufferOffset);

    uint32_t circleVertexOffset       = squareVertexOffset + static_cast<uint32_t>(squareVertices.size());
    uint32_t circleVertexBufferOffset = squareVertexOffset + static_cast<uint32_t>(squareVertices.size()) * sizeof(glm::vec2);
    uploadToDeviceLocalBuffer(m_device, circleVertices.data(), static_cast<uint32_t>(circleVertices.size()) * sizeof(glm::vec2), stagingBuffer.buffer,
                              stagingBuffer.memory, vertexBuffer.buffer, m_transferCommandPool, queue, circleVertexBufferOffset);

    uint32_t indexBufferSize = sizeof(uint16_t) * (static_cast<uint32_t>(squareIndices.size() + circleIndices.size()));
    Buffer   indexBuffer     = createBuffer(m_device, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      m_physicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uint32_t squareIndexOffset       = 0;
    uint32_t squareIndexBufferOffset = 0;
    uploadToDeviceLocalBuffer(m_device, squareIndices.data(), static_cast<uint32_t>(squareIndices.size()) * sizeof(uint16_t), stagingBuffer.buffer,
                              stagingBuffer.memory, indexBuffer.buffer, m_transferCommandPool, queue, squareIndexBufferOffset);

    uint32_t circleIndexOffset       = squareIndexOffset + static_cast<uint32_t>(squareIndices.size());
    uint32_t circleIndexBufferOffset = squareIndexOffset + static_cast<uint32_t>(squareIndices.size()) * sizeof(uint16_t);
    uploadToDeviceLocalBuffer(m_device, circleIndices.data(), static_cast<uint32_t>(circleIndices.size()) * sizeof(uint16_t), stagingBuffer.buffer,
                              stagingBuffer.memory, indexBuffer.buffer, m_transferCommandPool, queue, circleIndexBufferOffset);

    std::vector<Level> levels;

    std::filesystem::path fullPath = std::filesystem::current_path();
    fullPath += "\\resources\\levels\\";
    for (const auto& file : std::filesystem::directory_iterator(fullPath)) {
        levels.push_back(Level(file.path().string().c_str()));
    }

    Level& level = levels[0];

    std::array<VkDrawIndexedIndirectCommand, 2> drawIndirectCommands;
    // Ball
    drawIndirectCommands[0].indexCount    = static_cast<uint32_t>(circleIndices.size());
    drawIndirectCommands[0].instanceCount = 1;
    drawIndirectCommands[0].firstIndex    = circleIndexOffset;
    drawIndirectCommands[0].vertexOffset  = circleVertexOffset;
    drawIndirectCommands[0].firstInstance = 0;

    // Pad, walls and bricks
    drawIndirectCommands[1].indexCount    = static_cast<uint32_t>(squareIndices.size());
    drawIndirectCommands[1].instanceCount = 1 + 2 + level.columnCount * level.rowCount;
    drawIndirectCommands[1].firstIndex    = squareIndexOffset;
    drawIndirectCommands[1].vertexOffset  = squareVertexOffset;
    drawIndirectCommands[1].firstInstance = 1;

    uint32_t drawCommandsSize = sizeof(VkDrawIndexedIndirectCommand) * static_cast<uint32_t>(drawIndirectCommands.size());

    Buffer drawCommandBuffer = createBuffer(m_device, drawCommandsSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            m_physicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uploadToDeviceLocalBuffer(m_device, drawIndirectCommands.data(), drawCommandsSize, stagingBuffer.buffer, stagingBuffer.memory, drawCommandBuffer.buffer,
                              m_transferCommandPool, queue);

    float brickWidth = (WINDOW_WIDTH - MAX_COLUMN_COUNT * (MAX_COLUMN_SPACING + 1)) / static_cast<float>(MAX_COLUMN_COUNT);

    float padBottomPadding = MAX_ROW_SPACING * 8; // 5 rows of brick height space
    float brickHeight      = (WINDOW_HEIGHT - (MAX_ROW_COUNT * (MAX_ROW_SPACING + 1)) - padBottomPadding) / static_cast<float>(MAX_ROW_COUNT);

    float playAreaWidth = level.columnCount * brickWidth + (level.columnCount + 1) * level.columnSpacing;
    float wallWidth     = (WINDOW_WIDTH - playAreaWidth) * 0.5f;

    float padWidth = playAreaWidth * 0.2f;

#pragma warning(suppress : 26451) // Arithmetic overflow : Using operator'+' on a 4 byte value and then casting the result to a 8 byte value
    std::vector<InstanceData> instanceDatas(4 + level.columnCount * level.rowCount);

    // The ball
    instanceDatas[BALL_INDEX].position.x   = wallWidth + playAreaWidth * 0.5f;
    instanceDatas[BALL_INDEX].position.y   = WINDOW_HEIGHT - MAX_ROW_SPACING * 3 - brickWidth * 0.5f;
    instanceDatas[BALL_INDEX].position.z   = 0.0f;
    instanceDatas[BALL_INDEX].scale        = {brickWidth, brickWidth};
    instanceDatas[BALL_INDEX].textureIndex = 1;

    // The pad
    instanceDatas[PAD_INDEX].position.x   = wallWidth + playAreaWidth * 0.5f;
    instanceDatas[PAD_INDEX].position.y   = WINDOW_HEIGHT - MAX_ROW_SPACING * 2;
    instanceDatas[PAD_INDEX].position.z   = 0.0f;
    instanceDatas[PAD_INDEX].scale        = {padWidth, brickHeight};
    instanceDatas[PAD_INDEX].textureIndex = 0;

    // Left wall
    instanceDatas[LEFT_WALL_INDEX].position.x   = 0;
    instanceDatas[LEFT_WALL_INDEX].position.y   = WINDOW_HEIGHT * 0.5f;
    instanceDatas[LEFT_WALL_INDEX].position.z   = 0.0f;
    instanceDatas[LEFT_WALL_INDEX].scale        = {wallWidth * 2.0f, WINDOW_HEIGHT};
    instanceDatas[LEFT_WALL_INDEX].textureIndex = 1;

    // Right wall
    instanceDatas[RIGHT_WALL_INDEX].position.x   = WINDOW_WIDTH;
    instanceDatas[RIGHT_WALL_INDEX].position.y   = WINDOW_HEIGHT * 0.5f;
    instanceDatas[RIGHT_WALL_INDEX].position.z   = 0.0f;
    instanceDatas[RIGHT_WALL_INDEX].scale        = {wallWidth * 2.0f, WINDOW_HEIGHT};
    instanceDatas[RIGHT_WALL_INDEX].textureIndex = 1;

    // Bricks
    uint32_t instanceDataIndex = BRICK_START_INDEX;
    float    offsetY           = level.rowSpacing + 0.5f * brickHeight;
    float    stepY             = level.rowSpacing + brickHeight;
    for (size_t i = 0; i < level.levelState.size(); ++i, offsetY += stepY) {
        std::vector<BrickState>& brickRow = level.levelState[i];
        float                    offsetX  = wallWidth + level.columnSpacing + 0.5f * brickWidth;
        float                    stepX    = level.columnSpacing + brickWidth;
        for (size_t j = 0; j < brickRow.size(); ++j, offsetX += stepX) {
            instanceDatas[instanceDataIndex].position     = {offsetX, offsetY, 0.0f};
            instanceDatas[instanceDataIndex].scale        = {brickWidth, brickHeight};
            instanceDatas[instanceDataIndex].textureIndex = 2;
            ++instanceDataIndex;
        }
    }

    uint32_t instanceDataBufferSize = sizeof(InstanceData) * static_cast<uint32_t>(instanceDatas.size());
    Buffer   instanceDataBuffer     = createBuffer(m_device, instanceDataBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                             m_physicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    uploadToHostVisibleBuffer(m_device, instanceDatas.data(), instanceDataBufferSize, instanceDataBuffer.memory);

    vkDestroyBuffer(m_device, stagingBuffer.buffer, nullptr);
    vkFreeMemory(m_device, stagingBuffer.memory, nullptr);

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    pipelineCacheCreateInfo.initialDataSize           = 0;
    VK_CHECK(vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache));

    /*std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBindings;
    descriptorSetLayoutBindings.fill({});

    // Vertex and index buffer
    descriptorSetLayoutBindings[0].binding         = 0;
    descriptorSetLayoutBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[0].descriptorCount = 1;
    descriptorSetLayoutBindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    // Vertex and index buffer, again
    descriptorSetLayoutBindings[1].binding         = 1;
    descriptorSetLayoutBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[1].descriptorCount = 1;
    descriptorSetLayoutBindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    // Instance buffer
    descriptorSetLayoutBindings[2].binding         = 2;
    descriptorSetLayoutBindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[2].descriptorCount = 1;
    descriptorSetLayoutBindings[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;*/

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorSetLayoutCreateInfo.bindingCount                    = 0;       // static_cast<uint32_t>(descriptorSetLayoutBindings.size());
    descriptorSetLayoutCreateInfo.pBindings                       = nullptr; // descriptorSetLayoutBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));

    VkPushConstantRange rasterPushConstantRange = {};
    rasterPushConstantRange.offset              = 0;
    rasterPushConstantRange.size                = sizeof(RasterPushData);
    rasterPushConstantRange.stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo rasterPipelineLayoutCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    rasterPipelineLayoutCreateInfo.pushConstantRangeCount     = 1;
    rasterPipelineLayoutCreateInfo.pPushConstantRanges        = &rasterPushConstantRange;
    rasterPipelineLayoutCreateInfo.setLayoutCount             = 1;
    rasterPipelineLayoutCreateInfo.pSetLayouts                = &m_descriptorSetLayout;
    VK_CHECK(vkCreatePipelineLayout(m_device, &rasterPipelineLayoutCreateInfo, nullptr, &m_rasterPipelineLayout));

    VkShaderModule vertexShader   = loadShader("src/shaders/spirv/vertexShader.spv");
    VkShaderModule fragmentShader = loadShader("src/shaders/spirv/fragmentShader.spv");

    m_rasterPipeline = createRasterPipeline(vertexShader, fragmentShader);

    vkDestroyShaderModule(m_device, fragmentShader, nullptr);
    vkDestroyShaderModule(m_device, vertexShader, nullptr);

    /*// clang-format off
    std::array<VkDescriptorPoolSize, 1> descriptorPoolSizes = {{
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}
    }};
    // clang-format on

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptorPoolCreateInfo.poolSizeCount              = static_cast<uint32_t>(descriptorPoolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes                 = descriptorPoolSizes.data();
    descriptorPoolCreateInfo.maxSets                    = m_swapchainImageCount;
    VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

    // Allocating one descriptor set for each swapchain image, all with the same layout
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(m_swapchainImageCount, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorPool              = m_descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount          = m_swapchainImageCount;
    descriptorSetAllocateInfo.pSetLayouts                 = descriptorSetLayouts.data();

    m_descriptorSets = std::vector<VkDescriptorSet>(m_swapchainImageCount);
    vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, m_descriptorSets.data());

    std::array<VkDescriptorBufferInfo, 3> descriptorBufferInfos;
    descriptorBufferInfos[0].buffer = vertexBuffer.buffer;
    descriptorBufferInfos[0].offset = 0;
    descriptorBufferInfos[0].range  = vertexBufferSize;

    descriptorBufferInfos[1].buffer = indexBuffer.buffer;
    descriptorBufferInfos[1].offset = 0;
    descriptorBufferInfos[1].range  = indexBufferSize;

    descriptorBufferInfos[2].buffer = instanceDataBuffer.buffer;
    descriptorBufferInfos[2].offset = 0;
    descriptorBufferInfos[2].range  = instanceDataBufferSize;

    std::array<VkWriteDescriptorSet, 1> writeDescriptorSets;
    writeDescriptorSets.fill({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});

    writeDescriptorSets[0].dstBinding      = 0; // 0 for vertex, 1 for index buffer and 2 for instance buffer
    writeDescriptorSets[0].dstArrayElement = 0;
    writeDescriptorSets[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[0].descriptorCount = static_cast<uint32_t>(descriptorBufferInfos.size());
    writeDescriptorSets[0].pBufferInfo     = descriptorBufferInfos.data();

    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        writeDescriptorSets[0].dstSet = m_descriptorSets[i];

        vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }*/

    VkCommandPoolCreateInfo commandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags                   = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex        = m_queueFamilyIndex;

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandBufferAllocateInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount          = 1;

    m_commandPools   = std::vector<VkCommandPool>(m_swapchainImageCount);
    m_commandBuffers = std::vector<VkCommandBuffer>(m_swapchainImageCount);
    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPools[i]));
        commandBufferAllocateInfo.commandPool = m_commandPools[i];
        VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_commandBuffers[i]));
    }

    m_imageAvailableSemaphores = std::vector<VkSemaphore>(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores = std::vector<VkSemaphore>(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences           = std::vector<VkFence>(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence> imagesInFlight(m_swapchainImageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo     fenceCreateInfo     = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags                     = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_inFlightFences[i]));
    }

    m_camera.orientation = glm::vec2(0.0f, 0.0f);
    m_camera.position    = glm::vec3(0.0f, 0.0f, 2.5f);

    m_rasterPushData.oneOverTanOfHalfFov = 1.0f / tan(0.5f * FOV);
    m_rasterPushData.oneOverAspectRatio  = static_cast<float>(m_surfaceExtent.height) / static_cast<float>(m_surfaceExtent.width);
    m_rasterPushData.near                = NEAR;

    uint32_t currentFrame = 0;
    bool     updatedUI    = false;

    std::chrono::high_resolution_clock::time_point oldTime = std::chrono::high_resolution_clock::now();
    uint32_t                                       time    = 0;

    glm::vec2 ballVelocity(0.0f);
    bool      ballAttached = true;

    SDL_Event sdlEvent;
    bool      quit = false;
    while (!quit) {
        while (SDL_PollEvent(&sdlEvent)) {
            switch (sdlEvent.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_MOUSEMOTION:
                m_mousePosition.x = sdlEvent.motion.xrel;
                m_mousePosition.y = sdlEvent.motion.yrel;
                break;
            case SDL_KEYDOWN:
                m_keyStates[sdlEvent.key.keysym.sym].pressed = true;
                break;
            case SDL_KEYUP:
                m_keyStates[sdlEvent.key.keysym.sym].pressed = false;
                break;
            }
        }

        vkWaitForFences(m_device, 1, &m_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain->get(), UINT64_MAX, m_imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex));

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(m_device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = m_inFlightFences[currentFrame];

        vkResetCommandPool(m_device, m_commandPools[imageIndex], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

        std::chrono::high_resolution_clock::time_point newTime = std::chrono::high_resolution_clock::now();
        uint32_t frameTime = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(newTime - oldTime).count());
        oldTime            = newTime;
        time += frameTime;

        if (time > FRAMERATE_UPDATE_PERIOD || updatedUI) {
            char title[256];
            sprintf_s(title, "Breakout! Frametime: %.2fms", frameTime / 1'000.0f);
            SDL_SetWindowTitle(m_window, title);
            time      = 0;
            updatedUI = false;
        }

        if (m_keyStates[SDLK_a].pressed) {
            instanceDatas[PAD_INDEX].position.x -= 0.0002f * frameTime;

            if (instanceDatas[PAD_INDEX].position.x < wallWidth + padWidth * 0.5f) {
                instanceDatas[PAD_INDEX].position.x = wallWidth + padWidth * 0.5f;
            }

            if (ballAttached) {
                instanceDatas[BALL_INDEX].position.x = instanceDatas[PAD_INDEX].position.x;
            }
        }

        if (m_keyStates[SDLK_d].pressed) {
            instanceDatas[PAD_INDEX].position.x += 0.0002f * frameTime;

            if (instanceDatas[PAD_INDEX].position.x > WINDOW_WIDTH - (wallWidth + padWidth * 0.5f)) {
                instanceDatas[PAD_INDEX].position.x = WINDOW_WIDTH - (wallWidth + padWidth * 0.5f);
            }

            if (ballAttached) {
                instanceDatas[BALL_INDEX].position.x = instanceDatas[PAD_INDEX].position.x;
            }
        }

        if (m_keyStates[SDLK_SPACE].pressed) {
            m_keyStates[SDLK_SPACE].pressed = false;
            ballAttached                    = false;
            ballVelocity                    = {0.0f, -1.0f};
        }

        if (!ballAttached) {
            float velocityFactor = frameTime * 0.0002f;

            instanceDatas[BALL_INDEX].position += glm::vec3(ballVelocity * velocityFactor, 0.0f);
        }

        uploadToHostVisibleBuffer(m_device, instanceDatas.data(), instanceDataBufferSize, instanceDataBuffer.memory);

        updateCameraAndPushData(frameTime);

        recordRasterCommandBuffer(imageIndex, vertexBuffer.buffer, indexBuffer.buffer, instanceDataBuffer.buffer, drawCommandBuffer.buffer);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        VkSubmitInfo submitInfo         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &m_imageAvailableSemaphores[currentFrame];
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &m_commandBuffers[imageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &m_renderFinishedSemaphores[currentFrame];

        vkResetFences(m_device, 1, &m_inFlightFences[currentFrame]);

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, m_inFlightFences[currentFrame]));

        const VkSwapchainKHR& swapchain = m_swapchain->get();

        VkPresentInfoKHR presentInfo   = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphores[currentFrame];
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &swapchain;
        presentInfo.pImageIndices      = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
}

const VkInstance Breakout::createInstance() const {
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

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

    return instance;
}

const uint32_t Breakout::getGraphicsQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const {
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

const VkPhysicalDevice Breakout::pickPhysicalDevice() const {
    uint32_t physicalDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, 0));

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data()));

    uint32_t graphicsQueueIndex = UINT32_MAX;
    for (size_t i = 0; i < physicalDeviceCount; ++i) {
        VkPhysicalDevice physicalDevice = physicalDevices[i];

        VkPhysicalDeviceProperties physicalDeviceProperties = {};
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        printf("GPU%d: %s\n", static_cast<uint32_t>(i), physicalDeviceProperties.deviceName);

        if (physicalDeviceProperties.apiVersion < VK_API_VERSION_1_2) {
            continue;
        }

        if (physicalDeviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            continue;
        }

        graphicsQueueIndex = getGraphicsQueueFamilyIndex(physicalDevice);

        if (graphicsQueueIndex == UINT32_MAX) {
            continue;
        }

        VkBool32 presentSupported;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueIndex, m_surface, &presentSupported));

        if (presentSupported == VK_FALSE) {
            continue;
        }

        return physicalDevice;
    }

    throw std::runtime_error("No suitable GPU found!");
}

const VkRenderPass Breakout::createRenderPass() const {
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

    VkRenderPass renderPass = 0;
    VK_CHECK(vkCreateRenderPass(m_device, &renderPassCreateInfo, nullptr, &renderPass));

    return renderPass;
}

const std::vector<VkFramebuffer> Breakout::createFramebuffers() const {
    VkFramebufferCreateInfo framebufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferCreateInfo.renderPass              = m_renderPass;
    framebufferCreateInfo.attachmentCount         = 2;
    framebufferCreateInfo.width                   = m_surfaceExtent.width;
    framebufferCreateInfo.height                  = m_surfaceExtent.height;
    framebufferCreateInfo.layers                  = 1;

    std::vector<VkFramebuffer> framebuffers(m_swapchainImageCount);
    std::array<VkImageView, 2> attachments({});
    attachments[1] = m_depthImageView;

    const std::vector<VkImageView>& swapchainImageViews = m_swapchain->getImageViews();
    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        attachments[0]                     = swapchainImageViews[i];
        framebufferCreateInfo.pAttachments = attachments.data();
        VK_CHECK(vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &framebuffers[i]));
    }

    return framebuffers;
}

const VkShaderModule Breakout::loadShader(const char* pathToSource) const {
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

const VkPipeline Breakout::createRasterPipeline(const VkShaderModule& vertexShader, const VkShaderModule& fragmentShader) const {
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages.fill({VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO});

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
    vertexInputBindingDescriptions[0].stride    = sizeof(glm::vec2);
    vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexInputBindingDescriptions[1].binding   = INSTANCE_BUFFER_BIND_ID;
    vertexInputBindingDescriptions[1].stride    = sizeof(InstanceData);
    vertexInputBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 4> vertexInputAttributeDescriptions;
    // Vertex position
    vertexInputAttributeDescriptions[0].binding  = VERTEX_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[0].location = 0;
    vertexInputAttributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset   = 0;

    // Instance position
    vertexInputAttributeDescriptions[1].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[1].location = 1;
    vertexInputAttributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset   = 0;

    // Instance scale
    vertexInputAttributeDescriptions[2].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[2].location = 2;
    vertexInputAttributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[2].offset   = offsetof(InstanceData, scale);

    // Instance texture index
    vertexInputAttributeDescriptions[3].binding  = INSTANCE_BUFFER_BIND_ID;
    vertexInputAttributeDescriptions[3].location = 3;
    vertexInputAttributeDescriptions[3].format   = VK_FORMAT_R32_UINT;
    vertexInputAttributeDescriptions[3].offset   = offsetof(InstanceData, textureIndex);

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputStateCreateInfo.pVertexBindingDescriptions           = vertexInputBindingDescriptions.data();
    vertexInputStateCreateInfo.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertexInputBindingDescriptions.size());
    vertexInputStateCreateInfo.pVertexAttributeDescriptions         = vertexInputAttributeDescriptions.data();
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
    createInfo.pVertexInputState                                    = &vertexInputStateCreateInfo;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyStateCreateInfo.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    createInfo.pInputAssemblyState                                      = &inputAssemblyStateCreateInfo;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateCreateInfo.viewportCount                     = 1;
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

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateCreateInfo.dynamicStateCount                = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates                   = dynamicStates.data();
    createInfo.pDynamicState                                = &dynamicStateCreateInfo;

    createInfo.layout     = m_rasterPipelineLayout;
    createInfo.renderPass = m_renderPass;

    VkPipeline pipeline = 0;
    VK_CHECK(vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &createInfo, nullptr, &pipeline));

    return pipeline;
}

void Breakout::recordRasterCommandBuffer(const uint32_t& frameIndex, const VkBuffer& vertexBuffer, const VkBuffer& indexBuffer, const VkBuffer& instanceBuffer,
                                         const VkBuffer& drawCommandBuffer) const {
    VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    VkViewport viewport = {};
    viewport.width      = static_cast<float>(m_surfaceExtent.width);
    viewport.height     = static_cast<float>(m_surfaceExtent.height);
    viewport.x          = 0;
    viewport.y          = 0;
    viewport.minDepth   = 1.0f;
    viewport.maxDepth   = 0.0f;

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = m_surfaceExtent;

    VK_CHECK(vkBeginCommandBuffer(m_commandBuffers[frameIndex], &commandBufferBeginInfo));

    vkCmdSetViewport(m_commandBuffers[frameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_commandBuffers[frameIndex], 0, 1, &scissor);

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
    vkCmdBeginRenderPass(m_commandBuffers[frameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdPushConstants(m_commandBuffers[frameIndex], m_rasterPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(RasterPushData), &m_rasterPushData);

    vkCmdBindPipeline(m_commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_rasterPipeline);
    // vkCmdBindDescriptorSets(m_commandBuffers[frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_rasterPipelineLayout, 0, 1, &m_descriptorSets[frameIndex], 0,
    // nullptr);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_commandBuffers[frameIndex], VERTEX_BUFFER_BIND_ID, 1, &vertexBuffer, &offset);
    vkCmdBindVertexBuffers(m_commandBuffers[frameIndex], INSTANCE_BUFFER_BIND_ID, 1, &instanceBuffer, &offset);

    vkCmdBindIndexBuffer(m_commandBuffers[frameIndex], indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    if (m_physicalDeviceProperties.limits.maxDrawIndirectCount >= 2) {
        vkCmdDrawIndexedIndirect(m_commandBuffers[frameIndex], drawCommandBuffer, 0, 2, sizeof(VkDrawIndexedIndirectCommand));
    } else {
        vkCmdDrawIndexedIndirect(m_commandBuffers[frameIndex], drawCommandBuffer, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
        vkCmdDrawIndexedIndirect(m_commandBuffers[frameIndex], drawCommandBuffer, sizeof(VkDrawIndexedIndirectCommand), 1,
                                 sizeof(VkDrawIndexedIndirectCommand));
    }

    vkCmdEndRenderPass(m_commandBuffers[frameIndex]);

    VK_CHECK(vkEndCommandBuffer(m_commandBuffers[frameIndex]));
}

void Breakout::updateCameraAndPushData(const uint32_t& frameTime) {
    float rotateSpeedModifier = 0.0005f;

    m_camera.orientation.x += rotateSpeedModifier * m_mousePosition.x;
    if (m_camera.orientation.x > 2.0f * PI) {
        m_camera.orientation.x -= 2.0f * PI;
    } else if (m_camera.orientation.x < 0.0f) {
        m_camera.orientation.x += 2.0f * PI;
    }

    float epsilon = 0.00001f;

    m_camera.orientation.y += rotateSpeedModifier * m_mousePosition.y;
    if (m_camera.orientation.y > PI * 0.5f) {
        m_camera.orientation.y = PI * 0.5f - epsilon;
    } else if (m_camera.orientation.y < -PI * 0.5f) {
        m_camera.orientation.y = -PI * 0.5f + epsilon;
    }

    m_mousePosition.x = 0;
    m_mousePosition.y = 0;

    glm::vec3 globalUp      = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 globalRight   = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 globalForward = glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 forward = glm::rotate(globalForward, m_camera.orientation.x, globalUp);
    glm::vec3 right   = glm::rotate(globalRight, m_camera.orientation.x, globalUp);

    glm::vec3 deltaPosition = glm::vec3();

    if (m_keyStates[SDLK_w].pressed) {
        deltaPosition += forward;
    }

    if (m_keyStates[SDLK_s].pressed) {
        deltaPosition -= forward;
    }

    if (m_keyStates[SDLK_a].pressed) {
        deltaPosition -= right;
    }

    if (m_keyStates[SDLK_d].pressed) {
        deltaPosition += right;
    }

    if (m_keyStates[SDLK_SPACE].pressed) {
        deltaPosition += globalUp;
    }

    if (m_keyStates[SDLK_LCTRL].pressed) {
        deltaPosition -= globalUp;
    }

    float dt = frameTime * 0.000001f; // Time in seconds

    glm::vec3 acceleration = glm::vec3();
    if (deltaPosition != glm::vec3()) {
        acceleration = glm::normalize(deltaPosition) * ACCELERATION_FACTOR;
    }
    glm::vec3 offset = (0.5f * acceleration * dt + m_camera.velocity) * dt;

    m_camera.velocity += acceleration * dt;
    m_camera.velocity *= 0.99f;

    m_camera.position += offset;

    m_rasterPushData.cameraTransformation = glm::transpose(glm::translate(glm::identity<glm::mat4>(), -m_camera.position));
    m_rasterPushData.cameraTransformation = glm::rotate(m_rasterPushData.cameraTransformation, static_cast<float>(m_camera.orientation.x), globalUp);
    m_rasterPushData.cameraTransformation = glm::rotate(m_rasterPushData.cameraTransformation, static_cast<float>(m_camera.orientation.y), globalRight);
}

#ifdef VALIDATION_ENABLED
VkBool32 VKAPI_CALL Breakout::debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
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