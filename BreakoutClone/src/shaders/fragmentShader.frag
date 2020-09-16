#version 450

#extension GL_ARB_separate_shader_objects : require

layout(location = 0) flat in uint textureIndex;

layout(location = 0) out vec4 outColor;

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    outColor = vec4(colors[textureIndex], 1.0);
}