#version 450

#extension GL_GOOGLE_include_directive : require
//#extension GL_EXT_scalar_block_layout : require

#include "sharedStructures.h"

layout(location = 0) in vec2  vertex;
layout(location = 1) in vec2  instancePosition;
layout(location = 2) in float instanceDepth;
layout(location = 3) in vec2  instanceScale;
layout(location = 4) in uint  textureIndex;
layout(location = 5) in float textureAlpha;
layout(location = 6) in vec2  uvOffset;
layout(location = 7) in vec2  uvScale;
layout(location = 8) in uint  health;
layout(location = 9) in uint  maxHealth;

layout(location = 0) out uint  textureIndexFrag;
layout(location = 1) out float textureAlphaFrag;
layout(location = 2) out vec2  uvCoordsFrag;
layout(location = 3) out uint  healthFrag;
layout(location = 4) out uint  maxHealthFrag;

layout(set = 0, binding = 0) uniform UnifromBuffer {
    UniformData data;
} ub;

void main() {
    if (health == 0) {
        gl_Position = vec4(-10.0, -10.0, -10.0, 1.0);
        return;
    }

    textureIndexFrag = textureIndex;
    textureAlphaFrag = textureAlpha;
    healthFrag = health;
    maxHealthFrag = maxHealth;

    uvCoordsFrag = (vertex + vec2(0.5)) * uvScale + uvOffset;

    vec2 vertexPositioned = (vertex * instanceScale) + instancePosition;
    vec2 vertexScaledToClipSpace = vertexPositioned * ub.data.inversedWindowDimensions;
    vec2 vertexPositionedInClipSpace = vertexScaledToClipSpace * vec2(2.0) - vec2(1.0);

    gl_Position = vec4(vertexPositionedInClipSpace, instanceDepth, 1.0);
}
