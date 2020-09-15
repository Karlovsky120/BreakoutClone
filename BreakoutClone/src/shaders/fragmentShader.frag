#version 450

#extension GL_ARB_separate_shader_objects : require

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 dFdxPos = dFdx(worldPos);
    vec3 dFdyPos = dFdy(worldPos);

    vec3 normal = normalize(cross(dFdxPos, dFdyPos));

    normal.y = -normal.y;

    vec3 color = (normal + 3) * 0.25 * abs(normal);
    outColor = vec4(color, 1.0);
}