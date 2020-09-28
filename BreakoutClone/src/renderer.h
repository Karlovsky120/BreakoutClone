#pragma once
#include "swapchain.h"

#include "common.h"
#include "commonExternal.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

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

/// <summary>
/// Structure holding all relevant data for a vulkan buffer.
/// </summary>
struct Buffer {

    /// <summary>
    /// Handle of the buffer.
    /// </summary>
    VkBuffer buffer;

    /// <summary>
    /// Handle of the underlying buffer memory.
    /// </summary>
    VkDeviceMemory memory;

    Buffer(const VkDevice& device, const VkBuffer& buffer, const VkDeviceMemory& memory) : buffer(buffer), memory(memory), device(device) {}
    ~Buffer() {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

  private:
    /// <summary>
    /// Device that the buffer was created with.
    /// </summary>
    VkDevice device;
};

/// <summary>
/// Structure holding all relevant data for a vulkan buffer.
/// </summary>
struct Image {

    /// <summary>
    /// Handle of the image.
    /// </summary>
    VkImage image;

    /// <summary>
    /// Handle of the underlying buffer memory.
    /// </summary>
    VkDeviceMemory memory;

    /// <summary>
    /// Handle of the image view.
    /// </summary>
    VkImageView view;

    /// <summary>
    /// Dimensions of the image.
    /// </summary>
    VkExtent2D size;

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
    /// <summary>
    /// Device that the buffer was created with.
    /// </summary>
    VkDevice device;
};

/// <summary>
/// Designated for handling window and surface operations, rendering and vulkan resource creation.
/// </summary>
class Renderer {
  public:
    Renderer();
    ~Renderer();

    /// <summary>
    /// Window is initialized hidden, this shows the window.
    /// </summary>
    void showWindow() const;

    /// <summary>
    /// Pauses the CPU until all GPU operations are complete.
    /// </summary>
    void waitIdle() const;

    /// <summary>
    /// Sets the title of the main window.
    /// </summary>
    /// <param name="title">Title to be set.</param>
    void setWindowTitle(const char* title);

    /// <summary>
    /// Acquires an image from the swapchain to be used for next render.
    /// </summary>
    void acquireImage();

    /// <summary>
    /// Sends the command buffer to the GPU and starts the rendering process. Instructs the GPU to present image once render is complete.
    /// </summary>
    void renderAndPresentImage();

    /// <summary>
    /// Records render command buffers with relevant instance data.
    /// </summary>
    /// <param name="instanceBuffer">Instance buffer to be used with this render.</param>
    /// <param name="instanceCount">Number of elements in the instance buffer.</param>
    void recordRenderCommandBuffers(const VkBuffer& instanceBuffer, const uint32_t& instanceCount);

    /// <summary>
    /// Updates texture information on the GPU. Needs to be called after a new texture is loaded to be usable.
    /// </summary>
    /// <param name="textures">Vector of images to be used in the rendering.</param>
    void updateTextureArray(const std::vector<std::unique_ptr<Image>>& textures);

    /// <summary>
    /// Returns the uniform buffer pointer.
    /// </summary>
    /// <returns>The uniform buffer pointer.</returns>
    const Buffer* const getUniformBuffer() const;

    /// <summary>
    /// Creates a vulkan image with any supporting structures. Images are always allocated on the device memory.
    /// </summary>
    /// <param name="imageSize">Dimensions of the image in pixels</param>
    /// <param name="imageUsageFlags">Usage flags of the image.</param>
    /// <param name="imageFormat">Format of the image.</param>
    /// <param name="aspectMask">Aspect mask of the image.</param>
    /// <param name="name">Name of the image (only used with validation).</param>
    /// <returns>A pointer to a wrapper of the created image.</returns>
    std::unique_ptr<Image> createImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat,
                                       const VkImageAspectFlags& aspectMask, const char* name);

    /// <summary>
    /// Creates a vulkan buffer with any supporting structures.
    /// </summary>
    /// <param name="bufferSize">Size of the buffer in bytes.</param>
    /// <param name="bufferUsageFlags">Usage flags of the buffer.</param>
    /// <param name="memoryPropertyFlags">Memory properties of the buffer.</param>
    /// <param name="name">Name of the buffer (only used with validation).</param>
    /// <returns>A pointer to a wrapper of the created buffer.</returns>
    std::unique_ptr<Buffer> createBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags,
                                         const VkMemoryPropertyFlags& memoryPropertyFlags, const char* name);

    /// <summary>
    /// Uploads data to device local image. Use for uploading texture data.
    /// </summary>
    /// <param name="data">Pointer to the data for the upload.</param>
    /// <param name="imageSize">Size of the data in bytes.</param>
    /// <param name="deviceImage">Image the data is being uploaded to.</param>
    /// <param name="initialLayout">Layout the image was in before this call.</param>
    /// <param name="finalLayout">Desired image layout once the upload is complete.</param>
    void uploadToDeviceLocalImage(const void* data, const uint32_t& imageSize, const Image& deviceImage, const VkImageLayout& initialLayout,
                                  const VkImageLayout& finalLayout);

    /// <summary>
    /// Uploads data to host visible buffer.
    /// </summary>
    /// <param name="data">Pointer to the data for the upload.</param>
    /// <param name="bufferSize">Size of the data in bytes.</param>
    /// <param name="memory">Undelying buffer memory.</param>
    void uploadToHostVisibleBuffer(const void* data, const uint32_t& bufferSize, const VkDeviceMemory& memory);

    /// <summary>
    /// Uploads data to device local buffer.
    /// </summary>
    /// <param name="data">Pointer to the data for the upload.</param>
    /// <param name="bufferSize">Size of the data in bytes.</param>
    /// <param name="deviceBuffer">Buffer the data is to be uploaded to.</param>
    void uploadToDeviceLocalBuffer(const void* data, const uint32_t& bufferSize, const VkBuffer& deviceBuffer);

    Renderer(Renderer const&) = delete;
    void operator=(Renderer const&) = delete;

  private:
    /// <summary>
    /// Handle of the main window.
    /// </summary>
    SDL_Window* m_window = nullptr;

    /// <summary>
    /// Vulkan instance.
    /// </summary>
    VkInstance m_instance = VK_NULL_HANDLE;

    /// <summary>
    /// Rendering surface.
    /// </summary>
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    /// <summary>
    /// Selected physical device.
    /// </summary>
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    /// <summary>
    /// Logical device.
    /// </summary>
    VkDevice m_device = VK_NULL_HANDLE;

    /// <summary>
    /// Submission queue for all command buffers.
    /// </summary>
    VkQueue m_queue = VK_NULL_HANDLE;

    /// <summary>
    /// Render pass.
    /// </summary>
    VkRenderPass m_renderPass = VK_NULL_HANDLE;

    /// <summary>
    /// Nearest neighbour sampler for all textures.
    /// </summary>
    VkSampler m_sampler = VK_NULL_HANDLE;

    /// <summary>
    /// Layout of the descriptor set.
    /// </summary>
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

    /// <summary>
    /// Pool for allocating descriptor sets.
    /// </summary>
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    /// <summary>
    /// Allocated descriptor set.
    /// </summary>
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    /// <summary>
    /// Pipeline cache.
    /// </summary>
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

    /// <summary>
    /// Layout of the rendering pipeline.
    /// </summary>
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    /// <summary>
    /// Rendering pipeline.
    /// </summary>
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    /// <summary>
    /// Command pool for all command buffer allocations.
    /// </summary>
    VkCommandPool m_commandPool = VK_NULL_HANDLE;

#ifdef VALIDATION_ENABLED
    /// <summary>
    /// Debug messenger.
    /// </summary>
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
#endif

    /// <summary>
    /// Buffer with vertex data for a rectangle.
    /// </summary>
    std::unique_ptr<Buffer> m_vertexBuffer;

    /// <summary>
    /// Buffer with index data for a rectangle.
    /// </summary>
    std::unique_ptr<Buffer> m_indexBuffer;

    /// <summary>
    /// Staging buffer used for transfering data from the CPU to GPU local, CPU invisible buffers.
    /// </summary>
    std::unique_ptr<Buffer> m_stagingBuffer;

    /// <summary>
    /// Uniform buffer for CPU dependant, but static shader data.
    /// </summary>
    std::unique_ptr<Buffer> m_uniformBuffer;

    /// <summary>
    /// Image used for the depth buffer.
    /// </summary>
    std::unique_ptr<Image> m_depthImage;

    /// <summary>
    /// Swapchain.
    /// </summary>
    std::unique_ptr<Swapchain> m_swapchain;

    /// <summary>
    /// Size of the rendering surface.
    /// </summary>
    VkExtent2D m_surfaceExtent = {};

    /// <summary>
    /// Properties of the physical device.
    /// </summary>
    VkPhysicalDeviceProperties m_physicalDeviceProperties = {};

    /// <summary>
    /// Properties of the physical device memory.
    /// </summary>
    VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties = {};

    /// <summary>
    /// Submit info for submitting the render command buffers.
    /// </summary>
    VkSubmitInfo m_renderSubmitInfo = {};

    /// <summary>
    /// Present info for presenting rendered images.
    /// </summary>
    VkPresentInfoKHR m_presentInfo = {};

    /// <summary>
    /// Framebuffers used for rendering, one per frame.
    /// </summary>
    std::vector<VkFramebuffer> m_framebuffers;

    /// <summary>
    /// Render command buffers, one per frame.
    /// </summary>
    std::vector<VkCommandBuffer> m_renderCommandBuffers;

    /// <summary>
    /// Render finished semaphores, one per frame in flight.
    /// </summary>
    std::vector<VkSemaphore> m_renderFinishedSemaphores;

    /// <summary>
    /// Image presented semaphores, one per frame in flight.
    /// </summary>
    std::vector<VkSemaphore> m_imageAvailableSemaphores;

    /// <summary>
    /// Fences to prevent to many frames being generated for rendering, one per frame in flight.
    /// </summary>
    std::vector<VkFence> m_inFlightFences;

    /// <summary>
    /// In flight fences tied to particular frame, one per frame.
    /// </summary>
    std::vector<VkFence> m_imagesInFlight;

    /// <summary>
    /// Index of the used queue family.
    /// </summary>
    uint32_t m_queueFamilyIndex = UINT32_MAX;

    /// <summary>
    /// Number of images in the swapchain.
    /// </summary>
    uint32_t m_swapchainImageCount = UINT32_MAX;

    /// <summary>
    /// Index of the image currently being rendered to.
    /// </summary>
    uint32_t m_currentImageIndex = UINT32_MAX;

    /// <summary>
    /// Current frame index.
    /// </summary>
    uint32_t m_currentFrame = 0;

    /// <summary>
    /// Boolean used to see if old render command buffers need to be released when new ones are created.
    /// </summary>
    bool m_renderCommandBuffersRecorded = false;

    /// <summary>
    /// Render wait stages.
    /// </summary>
    VkPipelineStageFlags m_renderSubmitWaitStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    /// <summary>
    /// Initializes SDL and creates the main window.
    /// </summary>
    void initSDL();

#ifdef VALIDATION_ENABLED
    /// <summary>
    /// Sets up the callback for any validation errors, warnings and messages.
    /// </summary>
    void setupDebugUtils();
#endif

    /// <summary>
    /// Creates the instance.
    /// </summary>
    void createInstance();

    /// <summary>
    /// Picks a Vulkan capable device.
    /// </summary>
    void pickPhysicalDevice();

    /// <summary>
    /// Creates a logical device based on the picked physical device.
    /// </summary>
    void createDevice();

    /// <summary>
    /// Creates the command pool used for all operations.
    /// </summary>
    void createCommandPool();

    /// <summary>
    /// Creates a render pass to be used with the render pipeline.
    /// </summary>
    void createRenderPass();

    /// <summary>
    /// Creates the framebuffers to be used with the render pipeline.
    /// </summary>
    void createFramebuffers();

    /// <summary>
    /// Creates a sampler used for all texture sampling.
    /// </summary>
    void createSampler();

    /// <summary>
    /// Creates a descriptor set layout.
    /// </summary>
    void createDescriptorLayout();

    /// <summary>
    /// Creates the pipeline cache.
    /// </summary>
    void createPipelineCache();

    /// <summary>
    /// Creates the layout of the rendering pipeline.
    /// </summary>
    void createPipelineLayout();

    /// <summary>
    /// Creates the rendering pipeline.
    /// </summary>
    void createPipeline();

    /// <summary>
    /// Creates a pool for allocating descriptors.
    /// </summary>
    void createDescriptorPool();

    /// <summary>
    /// Allocates a descriptor set to be bound to the rendering pipeline.
    /// </summary>
    void allocateDescriptorSet();

    /// <summary>
    /// Allocates as many rendering command buffers as there are swapchain images.
    /// </summary>
    void allocateRenderCommandBuffers();

    /// <summary>
    /// Creates required semaphores and fences.
    /// </summary>
    void createSyncObjects();

    /// <summary>
    /// Sets up structures that are resued in the main rendering loop.
    /// </summary>
    void setupRenderLoop();

    /// <summary>
    /// Binds the uniform buffer to the descriptor set.
    /// </summary>
    void writeDescriptorSet();

    /// <summary>
    /// Destroys and recreates the render command buffers so they can be recorded again.
    /// </summary>
    void resetRenderCommandBuffers();

    /// <summary>
    /// Records a a render command buffer.
    /// </summary>
    /// <param name="frameIndex">Index of a render frame. Each frame needs its own command buffer.</param>
    /// <param name="instanceBuffer">Instance buffer to be used for rendering.</param>
    /// <param name="instanceCount">Number of elements in the instance buffer.</param>
    void recordRenderCommandBuffer(const uint32_t& frameIndex, const VkBuffer& instanceBuffer, const uint32_t& instanceCount) const;

    /// <summary>
    /// Gets a queue family index of a family that supports graphics, transfer and presentation operations.
    /// </summary>
    /// <param name="physicalDevice">Physical device used.</param>
    /// <returns>Index of a family that supports graphics, transfer and presentation operations.</returns>
    const uint32_t getGenericQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const;

    /// <summary>
    /// Loads a shader into memory and creates a shader module.
    /// </summary>
    /// <param name="pathToSource">Path to source, relative to the executable location.</param>
    /// <returns>A shader module.</returns>
    const VkShaderModule loadShader(const char* pathToSource) const;

#ifdef VALIDATION_ENABLED
    /// <summary>
    /// Names an object for reference in validation layers and Nvidia nsight.
    /// </summary>
    /// <param name="handle">Handle of the object.</param>
    /// <param name="type">Type of the object.</param>
    /// <param name="name">Name to be given to the object.</param>
    void nameObject(void* handle, const VkObjectType& type, const char* name);
#endif

    /// <summary>
    /// Creates a vulkan image.
    /// </summary>
    /// <param name="imageSize">Dimensions of the image in pixels</param>
    /// <param name="imageUsageFlags">Image usage flags.</param>
    /// <param name="imageFormat">Format of the image.</param>
    /// <returns>Handle for the created image</returns>
    const VkImage createVkImage(const VkExtent2D& imageSize, const VkImageUsageFlags& imageUsageFlags, const VkFormat& imageFormat);

    /// <summary>
    /// Creates a image view for an image.
    /// </summary>
    /// <param name="image">The image to create an image view for.</param>
    /// <param name="format">Format of the image.</param>
    /// <param name="aspectMask">Aspect mask of the image.</param>
    /// <returns>Handle for the created image view.</returns>
    const VkImageView createImageView(const VkImage& image, const VkFormat& format, const VkImageAspectFlags& aspectMask);

    /// <summary>
    /// Creates an image memory barrier that transitions image from one layout to the other.
    /// </summary>
    /// <param name="image">Image to be transitioned.</param>
    /// <param name="oldLayout">Layout the image was in previously.</param>
    /// <param name="newLayout">Layout to be transitioned to. </param>
    /// <returns>Image memory barrier struct.</returns>
    const VkImageMemoryBarrier createImageMemoryBarrier(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout);

    /// <summary>
    /// Creates a vulkan buffer.
    /// </summary>
    /// <param name="bufferSize">Size of the buffer in bytes.</param>
    /// <param name="bufferUsageFlags">Buffer usage flags.</param>
    /// <returns>Handle for the created buffer.</returns>
    const VkBuffer createVkBuffer(const VkDeviceSize& bufferSize, const VkBufferUsageFlags& bufferUsageFlags);

    /// <summary>
    /// Find a suitable memory type based on memory type mask and property flags.
    /// </summary>
    /// <param name="memoryTypeBits">Mask of the available memory types.</param>
    /// <param name="memoryPropertyFlags">Additional requested memory flags.</param>
    /// <returns>The index of memory type, if none can be found throws a runtime error.</returns>
    const uint32_t findMemoryType(const uint32_t& memoryTypeBits, const VkMemoryPropertyFlags& memoryPropertyFlags);

    /// <summary>
    /// Allocates memory for a vulkan object.
    /// </summary>
    /// <param name="memoryRequirements">Requirements to be meet for the memory.</param>
    /// <param name="memoryPropertyFlags">Additional requested memory properties.</param>
    /// <returns>Handle to allocated device memory.</returns>
    const VkDeviceMemory allocateVulkanObjectMemory(const VkMemoryRequirements& memoryRequirements, const VkMemoryPropertyFlags& memoryPropertyFlags);

    /// <summary>
    /// Creates a vertex and index buffer for drawing a unit square centered at the origin.
    /// </summary>
    void createVertexAndIndexBuffers();

#ifdef VALIDATION_ENABLED
    static VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/);
#endif
};