#version 460 core
layout(location = 0) in struct
{
    vec2 UV;
    float windDisplacement;
    float height;
} PIn;

layout(location = 0) out vec4 oColor;

void main()
{
    vec3 baseColor = vec3(0, 0.05, 0.02);
    vec3 tipColor = vec3(0.15, 0.9, 0);
    vec3 windColor = vec3(0.15, 0.75, 0.05);

    vec3 finalColor = mix(tipColor, windColor, PIn.windDisplacement / 10);
    finalColor = mix(baseColor, finalColor, PIn.height);
    oColor = vec4(finalColor, 1);
    //oColor = vec4(1, 1, 1, 1);
    //oColor = oColor * (PIn.windDisplacement / 10);
}
