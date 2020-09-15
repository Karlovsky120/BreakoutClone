#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_shader_16bit_storage : require

#include "sharedStructures.h"

layout(set = 0, binding = 0, scalar) readonly buffer Vertices {
    float vertices[];
};

layout(set = 0, binding = 1) readonly buffer Indices {
    uint16_t indices[];
};

layout(location = 0) out vec3 worldPos;

layout(push_constant) uniform PushConstants {
	RasterPushData pd;
} pc;

void main() {
    ivec3 indices = ivec3((int(indices[gl_VertexIndex]) * 3) + 0,
                          (int(indices[gl_VertexIndex]) * 3) + 1,
                          (int(indices[gl_VertexIndex]) * 3) + 2);

    vec3 vertex = vec3(vertices[indices.x],
                       vertices[indices.y],
                       vertices[indices.z]);

    worldPos = vertex;

    gl_Position = vec4(vertex, 1.0) * pc.pd.cameraTransformation;

    gl_Position.x *= pc.pd.oneOverTanOfHalfFov * pc.pd.oneOverAspectRatio;
    gl_Position.y *= pc.pd.oneOverTanOfHalfFov;
    gl_Position.w = -gl_Position.z; 
    gl_Position.z = pc.pd.near;
}
