#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(push_constant) uniform uConstantBlock
{
    mat4 view;
    mat4 proj;
    float worldTime;
    float deltaTime;
} uConstants;

struct GrassInstanceData
{
    vec3 position;
    vec2 uv;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceDataBlock
{
    GrassInstanceData data[];
} uInstances;

layout(std140, set = 0, binding = 1) uniform UniformBlock
{
    float terrainSize;
    float grassDensity;
    vec2 windDirection;
} uUniforms;

layout(set = 0, binding = 2) uniform sampler2D texWindNoise;

layout(location = 0) out struct
{
    vec2 UV;
    float windDisplacement;
    float height;
} VOut;

void main()
{
    GrassInstanceData instanceData = uInstances.data[gl_InstanceIndex];

    // Wind displaces vertices based on their height, so bases stay intact
    //TODO(caio): This should bend instead of just translating vertices
    float windStrength = length(uUniforms.windDirection);
    vec2 windDirection = normalize(uUniforms.windDirection);
    vec2 windUV = instanceData.uv + (uConstants.worldTime * uUniforms.windDirection);
    float windDisplacement = aPosition.y
        * windStrength
        * texture(texWindNoise, windUV).r;
    
    vec3 finalPosition = instanceData.position
        + (windDisplacement * vec3(windDirection.x, 0, windDirection.y));
    
    mat4 instanceTranslation = mat4(
            vec4(1, 0, 0, 0),
            vec4(0, 1, 0, 0),
            vec4(0, 0, 1, 0),
            vec4(finalPosition, 1)
            );
    gl_Position = uConstants.proj * uConstants.view * instanceTranslation * vec4(aPosition, 1);
    VOut.UV = aUV;
    VOut.windDisplacement = windDisplacement;
    VOut.height = aPosition.y / 10;
}
