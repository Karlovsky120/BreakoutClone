#pragma once

#include "common.h"

#include "resources.h"
#include "swapchain.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"

#include "SDL.h"
#include "SDL_vulkan.h"
#pragma warning(pop)

#include <memory>
#include <vector>

#define API_DUMP 0
#define VERBOSE  0
#define INFO     0

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

#define MAX_FRAMES_IN_FLIGHT 2

class Renderer {
  public:
    static const Renderer* getInstance();

    static void initRenderer(const char* vertexShaderPath, const char* fragmentShaderPath,
                             const VkPipelineVertexInputStateCreateInfo& pipelineVertexInputStateCreateInfo);

    static const VkDevice&                         getDevice();
    static const VkPhysicalDeviceProperties&       getDeviceProperties();
    static const VkPhysicalDeviceMemoryProperties& getMemoryProperties();
    static const VkCommandPool&                    getTransferCommandPool();
    static const VkQueue&                          getQueue();

    static void recordRenderCommandBuffers(const VkBuffer& vertexBuffer, const uint32_t& vertexBufferBindId, const VkBuffer& indexBuffer,
                                           const VkBuffer& instanceBuffer, const uint32_t& instanceBufferBindId, const VkBuffer& drawCommandBuffer);

    static void runRenderLoop();

    ~Renderer();

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
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineCache       m_pipelineCache       = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            m_pipeline            = VK_NULL_HANDLE;
    VkCommandPool         m_renderCommandPool   = VK_NULL_HANDLE;
    VkCommandPool         m_transferCommandPool = VK_NULL_HANDLE;

#ifdef VALIDATION_ENABLED
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
#endif

    std::unique_ptr<Image>     m_depthImage;
    std::unique_ptr<Swapchain> m_swapchain;

    VkExtent2D                       m_surfaceExtent                  = {};
    VkPhysicalDeviceProperties       m_physicalDeviceProperties       = {};
    VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties = {};

    std::vector<VkFramebuffer>   m_framebuffers;
    std::vector<VkCommandBuffer> m_renderCommandBuffers;
    std::vector<VkSemaphore>     m_renderFinishedSemaphores;
    std::vector<VkSemaphore>     m_imageAvailableSemaphores;
    std::vector<VkFence>         m_inFlightFences;
    std::vector<VkFence>         m_imagesInFlight;

    uint32_t m_queueFamilyIndex    = UINT32_MAX;
    uint32_t m_swapchainImageCount = UINT32_MAX;

    void init(const char* vertexShaderPath, const char* fragmentShaderPath, const VkPipelineVertexInputStateCreateInfo& pipelineVertexInputStateCreateInfo);
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
    void createGraphicsPipeline(const char* vertexShaderPath, const char* fragmentShaderPath,
                                const VkPipelineVertexInputStateCreateInfo& vertexInputStateCreateInfo);
    void createRenderCommandPoolsAndBuffers();
    void createSyncObjects();
    void recordRenderCommandBuffer(const uint32_t& frameIndex, const VkBuffer& vertexBuffer, const uint32_t& vertexBufferBindId, const VkBuffer& indexBuffer,
                                   const VkBuffer& instanceBuffer, const uint32_t& instanceBufferBindId, const VkBuffer& drawCommandBuffer) const;

    const uint32_t       getGenericQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const;
    const VkShaderModule loadShader(const char* pathToSource) const;
    const VkCommandPool  createCommandPool();

    void renderLoop();

#ifdef VALIDATION_ENABLED
    static VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/);
#endif

    Renderer();

    static Renderer* m_object_instance;
};