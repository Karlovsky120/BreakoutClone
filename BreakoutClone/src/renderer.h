#pragma once
#include "resources.h"
#include "swapchain.h"

#include "common.h"
#include "commonExternal.h"

#include <memory>

#define API_DUMP 0
#define VERBOSE  0
#define INFO     0

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080

#define PI 3.1415926535897932384f

#define VERTEX_BUFFER_BIND_ID   0
#define INSTANCE_BUFFER_BIND_ID 1

#define MAX_FRAMES_IN_FLIGHT 2

#define STAGING_BUFFER_SIZE 33'554'432 // 32MB

class Renderer {
  public:
    static Renderer* getInstance();

    static void initRenderer();

    static const VkDevice&                         getDevice();
    static const VkPhysicalDeviceProperties&       getDeviceProperties();
    static const VkPhysicalDeviceMemoryProperties& getMemoryProperties();
    static const VkCommandPool&                    getTransferCommandPool();
    static const Buffer&                           getStagingBuffer();
    static const VkQueue&                          getQueue();

    static void setWindowTitle(const char* title);
    static void showWindow();
    void        acquireImage();
    void        renderAndPresentImage();

    static void recordRenderCommandBuffers(const VkBuffer& instanceBuffer, const uint32_t& instanceCount);
    static void updateTextureArray();
    static void nameObject(void* handle, const VkObjectType& type, const char* name);

    void destroy();

    Renderer(Renderer const&) = delete;
    void operator=(Renderer const&) = delete;

  private:
    SDL_Window* m_window = nullptr;

    VkInstance            m_instance            = VK_NULL_HANDLE;
    VkSurfaceKHR          m_surface             = VK_NULL_HANDLE;
    VkPhysicalDevice      m_physicalDevice      = VK_NULL_HANDLE;
    VkDevice              m_device              = VK_NULL_HANDLE;
    VkQueue               m_queue               = VK_NULL_HANDLE;
    VkRenderPass          m_renderPass          = VK_NULL_HANDLE;
    VkSampler             m_sampler             = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_descriptorSet       = VK_NULL_HANDLE;
    VkPipelineCache       m_pipelineCache       = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            m_pipeline            = VK_NULL_HANDLE;
    VkCommandPool         m_renderCommandPool   = VK_NULL_HANDLE;
    VkCommandPool         m_transferCommandPool = VK_NULL_HANDLE;

#ifdef VALIDATION_ENABLED
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
#endif

    Buffer                     m_vertexBuffer;
    Buffer                     m_indexBuffer;
    Buffer                     m_stagingBuffer;
    Buffer                     m_uniformBuffer;
    Image                      m_depthImage;
    std::unique_ptr<Swapchain> m_swapchain;

    VkExtent2D                       m_surfaceExtent                  = {};
    VkPhysicalDeviceProperties       m_physicalDeviceProperties       = {};
    VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties = {};
    VkSubmitInfo                     m_renderSubmitInfo               = {};
    VkPresentInfoKHR                 m_presentInfo                    = {};

    std::vector<VkFramebuffer>   m_framebuffers;
    std::vector<VkCommandBuffer> m_renderCommandBuffers;
    std::vector<VkSemaphore>     m_renderFinishedSemaphores;
    std::vector<VkSemaphore>     m_imageAvailableSemaphores;
    std::vector<VkFence>         m_inFlightFences;
    std::vector<VkFence>         m_imagesInFlight;

    uint32_t m_queueFamilyIndex    = UINT32_MAX;
    uint32_t m_swapchainImageCount = UINT32_MAX;
    uint32_t m_currentImageIndex   = UINT32_MAX;
    uint32_t m_currentFrame        = 0;

    bool m_renderCommandBuffersRecorder = false;

    VkPipelineStageFlags m_renderSubmitWaitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    void init();
    void initSDL();
#ifdef VALIDATION_ENABLED
    void setupDebugUtils();
#endif
    void createInstance();
    void pickPhysicalDevice();
    void createDevice();
    void createRenderPass();
    void createFramebuffers();
    void createDescriptorLayout();
    void createPipelineCache();
    void createPipelineLayout();
    void createGraphicsPipeline();
    void createDescriptorPool();
    void allocateDescriptorSet();
    void createUniformBuffer();
    void writeDescriptorSet();
    void createRenderCommandPools();
    void allocateRenderCommandBuffers();
    void createSyncObjects();
    void setupRenderLoop();
    void createVertexAndIndexBuffers();
    void recordRenderCommandBuffer(const uint32_t& frameIndex, const VkBuffer& instanceBuffer, const uint32_t& instanceCount) const;
    void resetRenderCommandBuffers();

    const uint32_t       getGenericQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const;
    const VkShaderModule loadShader(const char* pathToSource) const;
    const VkCommandPool  createCommandPool();

#ifdef VALIDATION_ENABLED
    static VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/);
#endif

    Renderer();

    static Renderer* m_renderer;
};