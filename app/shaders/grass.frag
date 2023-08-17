#version 460 core
layout(location = 0) in struct
{
    vec2 UV;
} PIn;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = vec4(PIn.UV, 0, 1);
}
