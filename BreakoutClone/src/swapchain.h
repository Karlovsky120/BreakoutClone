#pragma once
#include "common.h"
#include "commonExternal.h"

struct SDL_Window;

class Swapchain {
  public:
    Swapchain(SDL_Window* window, const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice, const VkDevice& device,
              const uint32_t& queueFamilyIndex);

    ~Swapchain();

    const VkExtent2D&               getSurfaceExtent() const;
    const VkSurfaceFormatKHR&       getSurfaceFormat() const;
    const VkSwapchainKHR&           get() const;
    const std::vector<VkImageView>& getImageViews() const;
    const uint32_t&                 getImageCount() const;

  private:
    SDL_Window* m_window;

    const VkPhysicalDevice& m_physicalDevice;
    const VkDevice&         m_device;
    const VkSurfaceKHR&     m_surface;
    VkSwapchainKHR          m_swapchain;

    VkSurfaceFormatKHR m_surfaceFormat;
    VkExtent2D         m_surfaceExtent;

    uint32_t m_swapchainImageCount;

    std::vector<VkImage>     m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    const bool              surfaceFormatSupported() const;
    void                    setSwapchainImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
    void                    setSurfaceExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
    const VkPresentModeKHR& getPresentMode() const;
    void                    createSwapchain(const uint32_t& queueFamilyIndex);
    void                    createSwapchainImageViews();
};
