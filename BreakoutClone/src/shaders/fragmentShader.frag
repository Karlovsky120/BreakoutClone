#version 450

#extension GL_ARB_separate_shader_objects : require

layout(location = 0) flat in uint textureIndex;
layout(location = 1) in vec2 uvCoords;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D textures[3];

void main() {
    outColor = vec4(texture(textures[textureIndex], uvCoords));
}