#pragma once

#include "common.h"

#include "resources.h"
#include "sharedStructures.h"
#include "swapchain.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"

#include "SDL.h"
#include "SDL_vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/fwd.hpp"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#pragma warning(pop)

#include <map>
#include <memory>
#include <vector>

struct KeyState {
    bool    pressed     = false;
    uint8_t transitions = 0;
};

struct Camera {
    glm::vec2 orientation = glm::vec2();
    glm::vec3 position    = glm::vec3();
    glm::vec3 velocity    = glm::vec3();
};

class Breakout {
  public:
    void run();

    ~Breakout();

  private:
    SDL_Window* m_window = nullptr;

    VkInstance            m_instance                 = VK_NULL_HANDLE;
    VkSurfaceKHR          m_surface                  = VK_NULL_HANDLE;
    VkPhysicalDevice      m_physicalDevice           = VK_NULL_HANDLE;
    VkDevice              m_device                   = VK_NULL_HANDLE;
    VkRenderPass          m_renderPass               = VK_NULL_HANDLE;
    VkDeviceMemory        m_depthImageMemory         = VK_NULL_HANDLE;
    VkImageView           m_depthImageView           = VK_NULL_HANDLE;
    VkImage               m_depthImage               = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool           = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout      = VK_NULL_HANDLE;
    VkPipelineCache       m_pipelineCache            = VK_NULL_HANDLE;
    VkPipelineLayout      m_rasterPipelineLayout     = VK_NULL_HANDLE;
    VkPipeline            m_rasterPipeline           = VK_NULL_HANDLE;
    VkPipelineLayout      m_rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipeline            m_rayTracingPipeline       = VK_NULL_HANDLE;
    VkCommandPool         m_transferCommandPool      = VK_NULL_HANDLE;

#ifdef VALIDATION_ENABLED
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
#endif

    VkExtent2D                       m_surfaceExtent                  = {};
    VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties = {};

    std::unique_ptr<Swapchain> m_swapchain;

    Buffer m_vertexBuffer = {};
    Buffer m_indexBuffer  = {};

    std::map<SDL_Keycode, KeyState> m_keyStates;
    glm::ivec2                       m_mousePosition;

    Camera         m_camera         = {};
    RasterPushData m_rasterPushData = {};

    std::vector<VkFramebuffer>   m_framebuffers;
    std::vector<VkDescriptorSet> m_descriptorSets;
    std::vector<VkCommandPool>   m_commandPools;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkFence>         m_inFlightFences;
    std::vector<VkSemaphore>     m_renderFinishedSemaphores;
    std::vector<VkSemaphore>     m_imageAvailableSemaphores;

    uint32_t m_queueFamilyIndex    = UINT32_MAX;
    uint32_t m_swapchainImageCount = UINT32_MAX;

    const VkInstance                 createInstance() const;
    const uint32_t                   getGraphicsQueueFamilyIndex(const VkPhysicalDevice& physicalDevice) const;
    const VkPhysicalDevice           pickPhysicalDevice() const;
    const VkRenderPass               createRenderPass() const;
    const std::vector<VkFramebuffer> createFramebuffers() const;
    const VkShaderModule             loadShader(const char* pathToSource) const;
    const VkPipeline                 createRasterPipeline(const VkShaderModule& vertexShader, const VkShaderModule& fragmentShader) const;
    void                             recordRasterCommandBuffer(const uint32_t& frameIndex, const uint32_t& indexCount) const;
    void                             updateCameraAndPushData(const uint32_t& frameTime);
    //void                             updateSurfaceDependantStructures();

    static VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/);
};
