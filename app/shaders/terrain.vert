#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(push_constant) uniform uConstantBlock
{
    mat4 view;
    mat4 proj;
} uConstants;

layout(location = 0) out struct
{
    vec4 Color;
} VOut;

void main()
{
    gl_Position = uConstants.proj * uConstants.view * vec4(aPosition, 1);
    VOut.Color = vec4(aPosition, 1);
}
