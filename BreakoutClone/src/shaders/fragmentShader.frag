#version 450

#extension GL_ARB_separate_shader_objects : require

layout(location = 0) flat in uint textureIndex;
layout(location = 1)      in vec2 uvCoords;
layout(location = 2) flat in uint health;
layout(location = 3) flat in uint maxHealth;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D textures[16];

void main() {
    outColor = texture(textures[textureIndex], uvCoords);
    if (health < maxHealth) {
        vec4 crackTexture = texture(textures[1], uvCoords);
        crackTexture.w = crackTexture.w * (1 - float(health) / float(maxHealth));
        outColor = (1 - crackTexture.w) * outColor + crackTexture.w * crackTexture;
    }
}