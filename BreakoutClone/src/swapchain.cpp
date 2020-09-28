#include "swapchain.h"

Swapchain::Swapchain(SDL_Window* window, const VkSurfaceKHR& surface, const VkPhysicalDevice& physicalDevice, const VkDevice& device,
                     const uint32_t& queueFamilyIndex, const VkSurfaceFormatKHR& surfaceFormat)
    : m_window(window), m_surface(surface), m_physicalDevice(physicalDevice), m_device(device), m_surfaceFormat(surfaceFormat) {

    if (!surfaceFormatSupported()) {
        throw std::runtime_error("Requested surface format not supported!");
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities));

    setSwapchainImageCount(surfaceCapabilities);
    setSurfaceExtent(surfaceCapabilities);
    setPresentMode();

    createSwapchain(queueFamilyIndex);

    m_swapchainImages = std::vector<VkImage>(m_swapchainImageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, m_swapchainImages.data()));

    createSwapchainImageViews();
}

Swapchain::~Swapchain() {
    for (VkImageView& imageView : m_swapchainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

const VkSwapchainKHR& Swapchain::get() const { return m_swapchain; }

const VkExtent2D& Swapchain::getSurfaceExtent() const { return m_surfaceExtent; }

const VkSurfaceFormatKHR& Swapchain::getSurfaceFormat() const { return m_surfaceFormat; }

const std::vector<VkImageView>& Swapchain::getImageViews() const { return m_swapchainImageViews; }

const uint32_t& Swapchain::getImageCount() const { return m_swapchainImageCount; }

const bool Swapchain::surfaceFormatSupported() const {
    uint32_t surfaceFormatsCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatsCount, 0));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatsCount, surfaceFormats.data()));

    for (VkSurfaceFormatKHR surfaceFormat : surfaceFormats) {
        if (surfaceFormat.format == m_surfaceFormat.format && surfaceFormat.colorSpace == m_surfaceFormat.colorSpace) {
            return true;
        }
    }

    return false;
}

void Swapchain::setSwapchainImageCount(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
    if (surfaceCapabilities.maxImageCount < 2 && surfaceCapabilities.maxImageCount != 0) {
        throw std::runtime_error("Couldn't get enough swapchain images!");
    }

    m_swapchainImageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && m_swapchainImageCount > surfaceCapabilities.maxImageCount) {
        m_swapchainImageCount = surfaceCapabilities.minImageCount;
    }
}

void Swapchain::setSurfaceExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) {
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
        m_surfaceExtent = surfaceCapabilities.currentExtent;
    }

    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);

    m_surfaceExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

const VkPresentModeKHR Swapchain::setPresentMode() const {
    uint32_t presentModesCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, 0));

    std::vector<VkPresentModeKHR> presentModes(presentModesCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, presentModes.data()));

    for (VkPresentModeKHR presentMode : presentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

void Swapchain::createSwapchain(const uint32_t& queueFamilyIndex) {
    VkSwapchainCreateInfoKHR m_swapchainCreateInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    m_swapchainCreateInfo.surface                  = m_surface;
    m_swapchainCreateInfo.minImageCount            = m_swapchainImageCount;
    m_swapchainCreateInfo.queueFamilyIndexCount    = 1;
    m_swapchainCreateInfo.pQueueFamilyIndices      = &queueFamilyIndex;
    m_swapchainCreateInfo.preTransform             = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    m_swapchainCreateInfo.presentMode              = setPresentMode();
    m_swapchainCreateInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    m_swapchainCreateInfo.imageFormat              = m_surfaceFormat.format;
    m_swapchainCreateInfo.imageColorSpace          = m_surfaceFormat.colorSpace;
    m_swapchainCreateInfo.imageExtent              = m_surfaceExtent;
    m_swapchainCreateInfo.imageArrayLayers         = 1;
    m_swapchainCreateInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    m_swapchainCreateInfo.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    m_swapchainCreateInfo.oldSwapchain             = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &m_swapchainCreateInfo, nullptr, &m_swapchain));
}

void Swapchain::createSwapchainImageViews() {
    m_swapchainImageViews = std::vector<VkImageView>(m_swapchainImageCount);

    VkImageViewCreateInfo m_swapchainImageViewCreateInfo           = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    m_swapchainImageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    m_swapchainImageViewCreateInfo.format                          = m_surfaceFormat.format;
    m_swapchainImageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_swapchainImageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_swapchainImageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_swapchainImageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
    m_swapchainImageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    m_swapchainImageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    m_swapchainImageViewCreateInfo.subresourceRange.levelCount     = 1;
    m_swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    m_swapchainImageViewCreateInfo.subresourceRange.layerCount     = 1;

    for (size_t i = 0; i < m_swapchainImageCount; ++i) {
        m_swapchainImageViewCreateInfo.image = m_swapchainImages[i];
        VK_CHECK(vkCreateImageView(m_device, &m_swapchainImageViewCreateInfo, nullptr, &m_swapchainImageViews[i]));
    }
}
