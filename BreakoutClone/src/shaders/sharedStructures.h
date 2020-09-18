#ifdef CPP_SHADER_STRUCTURE
#pragma once

#pragma warning(push, 0)
#define GLM_FORCE_RADIANS
#define GLM_FORCE_XYZW_ONLY
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#pragma warning(pop)

#define vec3 glm::vec3
#define vec2 glm::vec2
#define uint uint32_t
#define int int32_t
#endif

struct Vertex {
    vec2 position;
};

struct Instance {
    vec3 position;
    int  textureIndex;
    vec2 scale;
    vec2 uvOffset;
    vec2 uvScale;
    uint health;
};

struct UniformData {
    float inverseWindowWidth;
    float inverseWindowHeight;
};

#ifdef CPP_SHADER_STRUCTURE
#undef int
#undef uint
#undef vec2
#undef vec3
#endif
