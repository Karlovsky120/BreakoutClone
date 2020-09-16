#version 450

#extension GL_GOOGLE_include_directive : require

#include "sharedStructures.h"

layout(location = 0) in vec2 vertex;

layout(location = 1) in vec3 instancePosition;
layout(location = 2) in vec2 instanceScale;
layout(location = 3) in uint  textureIndex;

layout(location = 0) out uint textureIndexFrag;

layout(push_constant) uniform PushConstants {
	RasterPushData pd;
} pc;

vec2 positions[3] = vec2[](
    vec2(-0.02888, 0.951388),
    vec2(-0.02888, 0.993055),
    vec2(0.02888, 0.951388)
);

/*void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    textureIndexFrag = gl_VertexIndex;
}*/

void main() { 
    vec3 screenSizeInverse = vec3(1/1280.0f, 1/720.0f, 1);

    vec3 vertexPositioned = vec3(vertex * instanceScale, 0.0) + instancePosition;
    vec3 vertexScaledToClipSpace = vertexPositioned * screenSizeInverse * vec3(2.0f, 2.0f, 1.0f) - vec3(1.0f, 1.0f, 0.0f);

    gl_Position = vec4(vertexScaledToClipSpace, 1.0); // * pc.pd.cameraTransformation;

    textureIndexFrag = textureIndex;

    /*gl_Position.x *= pc.pd.oneOverTanOfHalfFov * pc.pd.oneOverAspectRatio;
    gl_Position.y *= pc.pd.oneOverTanOfHalfFov;
    gl_Position.w = -gl_Position.z; 
    gl_Position.z = pc.pd.near;*/


    //vec4 position = vec4(vec3(vertex * instanceScale, 0.0) + instancePosition, 1.0);
    //vec4 position = vec4(vertex.x * 100, vertex.y * 100, 0.5, 1.0);
    //gl_Position = position;
    //gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
