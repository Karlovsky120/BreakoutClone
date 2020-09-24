#pragma once
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

#define STAGING_BUFFER_SIZE (1 << 25) // 32MB

class TextureManager;

struct Buffer {
    VkBuffer       buffer;
    VkDeviceMemory memory;

    Buffer(const VkDevice& device, const VkBuffer& buffer, const VkDeviceMemory& memory) : buffer(buffer), memory(memory), device(device) {}
    ~Buffer() {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

  private:
    VkDevice device;
};

struct Image {
    VkImage        image;
    VkDeviceMemory memory;
    VkImageView    view;
    VkExtent2D     size;

    Image(const VkDevice& device, const VkImage& image, const VkDeviceMemory& memory, const VkImageView& view, const VkExtent2D& size)
        : device(device), image(image), memory(memory), view(view), size(size) {}
    ~Image() {
        vkDestroyImageView(device, view, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

  private:
    VkDevice device;
};

class Renderer {
  public:
    Renderer();
    ~Renderer();

    void showWindow();
    void waitIdle() const;
    void setWindowTitle(const char* title);
    void acquireImage();
    void renderAndPresentImage();
    void recordRenderCommandBuffers(const VkBuffer& instanceBuffer, const uint32_t& instanceCount);
    void updateTextureArray(const std::vector<std::unique_ptr<Image>>& textures);

    const Buffer* const getUniformBuffer() const;

    std::unique_ptr<Image>  createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
                                        const VkImageAspectFlags& aspectMask, const char* name);
    std::unique_ptr<Buffer> createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                         const VkMemoryPropertyFlags& memoryPropertyFlags, const char* name);
    void                    uploadToDeviceLocalImage(const void* data, const uint32_t& imageSize, const Image& deviceImage, const VkImageLayout& initialLayout,
                                                     const VkImageLayout& finalLayout);
    void                    uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& memory);
    void                    uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const VkBuffer& deviceBuffer);

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

    std::unique_ptr<Buffer>    m_vertexBuffer;
    std::unique_ptr<Buffer>    m_indexBuffer;
    std::unique_ptr<Buffer>    m_stagingBuffer;
    std::unique_ptr<Buffer>    m_uniformBuffer;
    std::unique_ptr<Image>     m_depthImage;
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

    bool m_renderCommandBuffersRecorded = false;

    VkPipelineStageFlags m_renderSubmitWaitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    TextureManager* m_textureManager;

    void initSDL();

#ifdef VALIDATION_ENABLED
    void setupDebugUtils();
#endif

    void createInstance();
    void pickPhysicalDevice();
    void createDevice();
    void createRenderPass();
    void createFramebuffers();
    void createSampler();
    void createDescriptorLayout();
    void createPipelineCache();
    void createPipelineLayout();
    void createPipeline();
    void createDescriptorPool();
    void allocateDescriptorSet();
    void allocateRenderCommandBuffers();
    void createSyncObjects();
    void setupRenderLoop();
    void writeDescriptorSet();
    void resetRenderCommandBuffers();
    void recordRenderCommandBuffer(const uint32_t& frameIndex, const VkBuffer& instanceBuffer, const uint32_t& instanceCount) const;

    const VkCommandPool  createCommandPool();
    const uint32_t       getGenericQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const;
    const VkShaderModule loadShader(const char* pathToSource) const;

#ifdef VALIDATION_ENABLED
    void nameObject(void* handle, const VkObjectType& type, const char* name);
#endif

    const VkImage              createVkImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat);
    const VkImageView          createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask);
    const VkImageMemoryBarrier createImageMemoryBarrier(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout);
    const VkBuffer             createVkBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags);
    const uint32_t             findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags);
    const VkDeviceMemory       allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags);

    void createVertexAndIndexBuffers();

#ifdef VALIDATION_ENABLED
    static VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/);
#endif
};