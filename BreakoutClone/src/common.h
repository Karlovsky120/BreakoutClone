#pragma once

#pragma warning(disable : 26812) // The enum type * is unscoped. Prefer 'enum class' over 'enum'.

#include <cassert>
#include <cstdint>

#define CPP_SHADER_STRUCTURE

#define VECTOR_SIZE_BYTES(vector) (static_cast<uint32_t>(vector.size()) * sizeof(vector[0]))

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