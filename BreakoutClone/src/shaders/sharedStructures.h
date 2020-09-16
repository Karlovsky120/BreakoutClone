#ifdef CPP_SHADER_STRUCTURE
#pragma once

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/fwd.hpp"
#include "glm/mat4x4.hpp"
#pragma warning(pop)

#define mat4 glm::mat4
#define vec3 glm::vec3
#define vec2 glm::vec2
#define uint uint32_t
#endif

struct RasterPushData {
    mat4 cameraTransformation;

    // Perspective parameters for reverse z
    float oneOverTanOfHalfFov;
    float oneOverAspectRatio;
    float near;
};

struct InstanceData {
    vec3 position;
    vec2 scale;
    uint textureIndex;
};

#ifdef CPP_SHADER_STRUCTURE
#undef uint
#undef vec2
#undef vec3
#undef mat4
#endif
