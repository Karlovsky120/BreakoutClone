#ifdef CPP_SHADER_STRUCTURE
#pragma once

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/fwd.hpp"
#include "glm/mat4x4.hpp"
#pragma warning(pop)

#define mat4 glm::mat4
#endif

struct RasterPushData {
    mat4 cameraTransformation;

    // Perspective parameters for reverse z
    float oneOverTanOfHalfFov;
    float oneOverAspectRatio;
    float near;
};

struct RayTracingPushData {
    mat4 cameraTransformationInverse;

    float oneOverTanOfHalfFov;
};

#ifdef CPP_SHADER_STRUCTURE
#undef mat4
#endif
