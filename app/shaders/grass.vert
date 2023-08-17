#version 460 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(push_constant) uniform uConstantBlock
{
    mat4 view;
    mat4 proj;
} uConstants;

struct GrassInstanceData
{
    vec3 position;
};

layout(std140, set = 0, binding = 0) readonly buffer InstanceDataBlock
{
    GrassInstanceData data[];
} uInstances;

layout(location = 0) out struct
{
    vec2 UV;
} VOut;

void main()
{
    GrassInstanceData instanceData = uInstances.data[gl_InstanceIndex];
    mat4 instanceTranslation = mat4(
            vec4(1, 0, 0, 0),
            vec4(0, 1, 0, 0),
            vec4(0, 0, 1, 0),
            vec4(instanceData.position, 1)
            );
    //gl_Position = uConstants.proj * uConstants.view * vec4(aPosition, 1);
    gl_Position = uConstants.proj * uConstants.view * instanceTranslation * vec4(aPosition, 1);
    VOut.UV = aUV;
}
