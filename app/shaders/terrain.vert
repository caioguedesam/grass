#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(push_constant) uniform uConstantBlock
{
    mat4 view;
    mat4 proj;
    float terrainSize;
} uConstants;

layout(location = 0) out struct
{
    vec4 Color;
} VOut;

void main()
{
    mat4 worldMatrix = mat4(
            vec4(uConstants.terrainSize, 0, 0, 0),
            vec4(0, uConstants.terrainSize, 0, 0),
            vec4(0, 0, uConstants.terrainSize, 0),
            vec4(0, 0, 0, 1));
    gl_Position = uConstants.proj * uConstants.view * worldMatrix * vec4(aPosition, 1);
    VOut.Color = vec4(aPosition, 1);
}
