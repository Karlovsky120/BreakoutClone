#pragma once

#include "common.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

#include <vector>

#define FRAMERATE_CAPPED 1

struct SDL_Window;

class Swapchain {
  public:
    VkExtent2D update();

    Swapchain(SDL_Window* window, const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice, const VkDevice& device,
              const uint32_t& queueFamilyIndex);

    ~Swapchain();

    const VkExtent2D&               getSurfaceExtent() const;
    const VkSurfaceFormatKHR&       getSurfaceFormat() const;
    const VkSwapchainKHR&           get() const;
    const std::vector<VkImage>&     getImages() const;
    const std::vector<VkImageView>& getImageViews() const;
    const uint32_t&                 getImageCounts() const;

  private:
    SDL_Window* m_window = nullptr;

    const VkPhysicalDevice m_physicalDevice;
    const VkDevice         m_device;
    const VkSurfaceKHR     m_surface;
    VkSwapchainKHR         m_swapchain = VK_NULL_HANDLE;

    VkSwapchainCreateInfoKHR m_swapchainCreateInfo          = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    VkImageViewCreateInfo    m_swapchainImageViewCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkSurfaceFormatKHR       m_surfaceFormat;
    VkExtent2D               m_surfaceExtent;

    uint32_t m_swapchainImageCount = UINT32_MAX;

    std::vector<VkImage>     m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    const bool surfaceFormatSupported() const;

    void setSwapchainImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
    void setSurfaceExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

    const VkPresentModeKHR getPresentMode() const;

    void createSwapchain(const uint32_t& queueFamilyIndex);
    void createSwapchainImageViews();
};
