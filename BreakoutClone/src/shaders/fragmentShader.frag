#version 450

#extension GL_ARB_separate_shader_objects : require

layout(location = 0) flat in int textureIndex;
layout(location = 1) in vec2 uvCoords;

layout(location = 0) out vec4 outColor;

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(set = 0, binding = 1) uniform sampler2D textures[3];

void main() {
    outColor = vec4(texture(textures[textureIndex], uvCoords));
}