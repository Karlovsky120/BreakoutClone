#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "sharedStructures.h"

layout(location = 0) in vec2 vertex;

layout(location = 1) in vec3 instancePosition;
layout(location = 2) in vec2 instanceScale;
layout(location = 3) in int  textureIndex;
layout(location = 4) in vec2 uvOffset;
layout(location = 5) in vec2 uvScale;
layout(location = 6) in uint health;

layout(location = 0) out int textureIndexFrag;
layout(location = 1) out vec2 uvCoords;

layout(set = 0, binding = 0) readonly buffer UnifromBuffer {
    UniformData data;
} ub;

void main() {
    if (health == 0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    textureIndexFrag = textureIndex;

    uvCoords = (vertex + vec2(0.5)) * uvScale + uvOffset;

    vec3 vertexPositioned = vec3(vertex * instanceScale, 0.0) + instancePosition;
    vec3 vertexScaledToClipSpace = vertexPositioned * vec3(ub.data.inverseWindowWidth, ub.data.inverseWindowHeight, 1);
    vec3 vertexPositionedIntoClipSpace = vertexScaledToClipSpace * vec3(2.0, 2.0, 1.0) - vec3(1.0, 1.0, 0.0);

    gl_Position = vec4(vertexPositionedIntoClipSpace, 1.0);
}
