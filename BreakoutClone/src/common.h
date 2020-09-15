#pragma once

#pragma warning(disable : 26812) // The enum type * is unscoped. Prefer 'enum class' over 'enum'.

#include <cassert>
#include <cstdint>

#define CPP_SHADER_STRUCTURE

#ifdef _DEBUG
#define VALIDATION_ENABLED

#define VK_CHECK(call)                                                                                                                                         \
    {                                                                                                                                                          \
        VkResult result = call;                                                                                                                                \
        assert(result == VK_SUCCESS);                                                                                                                          \
    }
#else
#define VK_CHECK(call) call
#endif