#version 450

#extension GL_GOOGLE_include_directive : require

#include "sharedStructures.h"

layout(location = 0) flat in uint  textureIndex;
layout(location = 1) flat in float textureAlpha;
layout(location = 2)      in vec2  uvCoords;
layout(location = 3) flat in uint  health;
layout(location = 4) flat in uint  maxHealth;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D textures[MAX_TEXTURE_COUNT];

void main() {
    outColor = textureAlpha * texture(textures[textureIndex], uvCoords);
    if (health < maxHealth) {
        vec4 crackTexture = texture(textures[0], uvCoords);
        crackTexture.w = crackTexture.w * (1 - float(health) / float(maxHealth));
        outColor = (1 - crackTexture.w) * outColor + crackTexture.w * crackTexture;
    }
}