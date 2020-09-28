#pragma once
// All vulkan enums emmit this warninig
#pragma warning(disable : 26812) // The enum type * is unscoped. Prefer 'enum class' over 'enum'.

#pragma warning(push, 0)
#pragma warning(disable : 26819) // Dereferencing NULL pointer.'*' contains the same NULL value as '*' did.

#include "SDL.h"
#include "SDL_vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/glm.hpp"
#pragma warning(enable : 26819)
#pragma warning(pop)
