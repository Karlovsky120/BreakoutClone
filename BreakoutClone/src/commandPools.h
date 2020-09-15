#pragma once

#include "common.h"

#pragma warning(push, 0)
#define VK_ENABLE_BETA_EXTENSIONS
#include "volk.h"
#pragma warning(pop)

VkCommandPool createCommandPool(const VkDevice device, const uint32_t queueFamilyIndex);