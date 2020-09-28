#pragma once
#include "common.h"
#include "commonExternal.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

struct SDL_Window;

/// <summary>
/// Envelops surface and swapchain operations.
/// </summary>
class Swapchain {
  public:
    /// <summary>
    /// Creates a new swapchian
    /// </summary>
    /// <param name="window">Window the swapchain is to be used with.</param>
    /// <param name="surface">Surface that is being rendererd to.</param>
    /// <param name="physicalDevice">Physical device used for rendering.</param>
    /// <param name="device">Logical device used for rendering.</param>
    /// <param name="queueFamilyIndex">Index of a queue family that supports graphics and present operations.</param>
    Swapchain(SDL_Window* window, const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice, const VkDevice& device, const uint32_t& queueFamilyIndex,
              const VkSurfaceFormatKHR& surfaceFormat);

    ~Swapchain();

    /// <summary>
    /// Getter of the handle of the swapchain.
    /// </summary>
    /// <returns>Handle of the swapchain</returns>
    const VkSwapchainKHR& get() const;

    /// <summary>
    /// Getter of the dimensions of the rendering surface.
    /// </summary>
    /// <returns>Dimensions of the rendering surface</returns>
    const VkExtent2D& getSurfaceExtent() const;

    /// <summary>
    /// Getter of the surface format.
    /// </summary>
    /// <returns>The surface format.</returns>
    const VkSurfaceFormatKHR& getSurfaceFormat() const;

    /// <summary>
    /// Getter of the image views of the swapchain images.
    /// </summary>
    /// <returns>Image views of the swapchain images.</returns>
    const std::vector<VkImageView>& getImageViews() const;

    /// <summary>
    /// Getter of swapchain image count.
    /// </summary>
    /// <returns>Swapchain image count.</returns>
    const uint32_t& getImageCount() const;

  private:
    /// <summary>
    /// Handle of the main window.
    /// </summary>
    SDL_Window* m_window;

    /// <summary>
    /// Handle of the selected physical device.
    /// </summary>
    const VkPhysicalDevice& m_physicalDevice;

    /// <summary>
    /// Handle of the logical device.
    /// </summary>
    const VkDevice& m_device;

    /// <summary>
    /// Handle of the rendering surface.
    /// </summary>
    const VkSurfaceKHR& m_surface;

    /// <summary>
    /// Swapchain handle.
    /// </summary>
    VkSwapchainKHR m_swapchain;

    /// <summary>
    /// Used surface format.
    /// </summary>
    const VkSurfaceFormatKHR m_surfaceFormat;

    /// <summary>
    /// Rendering surface dimensions.
    /// </summary>
    VkExtent2D m_surfaceExtent;

    /// <summary>
    /// Number of images in the swapchain.
    /// </summary>
    uint32_t m_swapchainImageCount;

    /// <summary>
    /// Swapchains images.
    /// </summary>
    std::vector<VkImage> m_swapchainImages;

    /// <summary>
    /// Swapchain image views.
    /// </summary>
    std::vector<VkImageView> m_swapchainImageViews;

    /// <summary>
    /// Checks the support for the selected surface format.
    /// </summary>
    /// <returns></returns>
    const bool surfaceFormatSupported() const;

    /// <summary>
    /// Sets the number of swapchain images based on the sufrace capabilities.
    /// </summary>
    /// <param name="surfaceCapabilities">Surface capabilites containing info about min and max swapchain image count.</param>
    void setSwapchainImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

    /// <summary>
    /// Sets the size of rendering surface based on its capabilites.
    /// </summary>
    /// <param name="surfaceCapabilities">Surface capabilites containing info about size of the enveloping window.</param>
    void setSurfaceExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);

    /// <summary>
    /// Sets the present mode.
    /// </summary>
    /// <returns>Present mode set.</returns>
    const VkPresentModeKHR setPresentMode() const;

    /// <summary>
    /// Creates the vulkan swapchain.
    /// </summary>
    /// <param name="queueFamilyIndex">Index of queue family to be used with the swapchain.</param>
    void createSwapchain(const uint32_t& queueFamilyIndex);

    /// <summary>
    /// Creates image views for swapchain images.
    /// </summary>
    void createSwapchainImageViews();
};
