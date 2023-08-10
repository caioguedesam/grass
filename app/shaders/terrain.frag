#version 460 core
layout(location = 0) in struct
{
    vec4 Color;
} PIn;

layout(location = 0) out vec4 oColor;

void main()
{
    oColor = PIn.Color;
}
