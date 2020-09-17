#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

#include "sharedStructures.h"

layout(location = 0) in vec2 vertex;

layout(location = 1) in vec3 instancePosition;
layout(location = 2) in vec2 instanceScale;
layout(location = 3) in uint textureIndex;
layout(location = 4) in uint health;

layout(location = 0) out uint textureIndexFrag;

void main() {
    if (health == 0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    textureIndexFrag = textureIndex;

    vec3 screenSizeInverse = vec3(1/1280.0, 1/720.0, 1);

    vec3 vertexPositioned = vec3(vertex * instanceScale, 0.0) + instancePosition;
    vec3 vertexScaledToClipSpace = vertexPositioned * screenSizeInverse * vec3(2.0, 2.0, 1.0) - vec3(1.0, 1.0, 0.0);

    gl_Position = vec4(vertexScaledToClipSpace, 1.0); // * pc.pd.cameraTransformation;


}
