#include "app/grass.hpp"
#include "engine/src/asset/asset.hpp"
#include "engine/src/core/debug.hpp"
#include "engine/src/core/ds.hpp"
#include "engine/src/core/file.hpp"
#include "engine/src/core/math.hpp"
#include "engine/src/render/render.hpp"

namespace ty
{
namespace Grass
{

f32 grassVertices[] =
{
    //TODO(caio): Change this to use grass blade mesh
    // position (x, y, z), normal (x, y, z), uv (u, v)
    -0.05f,  1.f,    -0.05f,      0.f, 1.f, 0.f, 0.f, 0.f,
    0.05f,   1.f,    -0.05f,      0.f, 1.f, 0.f, 0.f, 0.f,
    0.05f,   1.f,    0.05f,       0.f, 1.f, 0.f, 0.f, 0.f,
    -0.05f,  1.f,    0.05f,       0.f, 1.f, 0.f, 0.f, 0.f,
};

u32 grassIndices[] =
{
    //TODO(caio): Change this to use grass blade mesh
    0, 1, 2, 0, 2, 3,
};

void InitGrassSystem(Handle<render::RenderTarget> hRenderTarget)
{
    ASSERT(IS_ALIGNED(sizeof(GrassInstanceDataBlock), render::GetBufferTypeAlignment(render::BUFFER_TYPE_STORAGE)));

    grassInstanceData = MakeArrayAlign<GrassInstanceDataBlock>(
            maxGrassInstances * RENDER_CONCURRENT_FRAMES,
            maxGrassInstances * RENDER_CONCURRENT_FRAMES,
            {},
            render::GetBufferTypeAlignment(render::BUFFER_TYPE_STORAGE));

    // Initializing graphics resources
    hAssetVsGrass = asset::LoadShader(file::MakePath(IStr("app/shaders/grass.vert")));
    asset::Shader& assetShader = asset::shaders[hAssetVsGrass];
    hVsGrass = render::MakeShader(render::SHADER_TYPE_VERTEX, assetShader.size, assetShader.data);

    hAssetPsGrass = asset::LoadShader(file::MakePath(IStr("app/shaders/grass.frag")));
    assetShader = asset::shaders[hAssetPsGrass];
    hPsGrass = render::MakeShader(render::SHADER_TYPE_PIXEL, assetShader.size, assetShader.data);

    hAssetModelGrass = asset::LoadModelOBJ(file::MakePath(IStr("resources/models/grass/grass.obj")));
    asset::Model& assetModelGrass = asset::models[hAssetModelGrass];

    hVbGrass = render::MakeBuffer(render::BUFFER_TYPE_VERTEX, 
            assetModelGrass.vertices.count * sizeof(f32), 
            sizeof(f32),
            assetModelGrass.vertices.data);
    hIbGrass = render::MakeBuffer(render::BUFFER_TYPE_INDEX, 
            assetModelGrass.groups[0].indices.count * sizeof(u32), 
            sizeof(u32),
            assetModelGrass.groups[0].indices.data);

    //hVbGrass = render::MakeBuffer(render::BUFFER_TYPE_VERTEX, 
            //ARR_LEN(grassVertices) * sizeof(f32), 
            //sizeof(f32),
            //grassVertices);
    //hIbGrass = render::MakeBuffer(render::BUFFER_TYPE_INDEX, 
            //ARR_LEN(grassIndices) * sizeof(u32), 
            //sizeof(u32),
            //grassIndices);

    hSbGrassInstanceData = render::MakeBuffer(render::BUFFER_TYPE_STORAGE,
            sizeof(GrassInstanceDataBlock) * grassInstanceData.count, 
            sizeof(GrassInstanceDataBlock) * maxGrassInstances);
            //sizeof(GrassInstanceDataBlock));

    render::VertexAttribute vertexAttributesGrass[] =
    {
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V2F,
    };
    hVertexLayoutGrass = render::MakeVertexLayout(ARR_LEN(vertexAttributesGrass), vertexAttributesGrass);

    render::RenderPassDesc renderPassGrassDesc = {};
    renderPassGrassDesc.loadOp = render::LOAD_OP_LOAD;
    renderPassGrassDesc.storeOp = render::STORE_OP_STORE;
    renderPassGrassDesc.initialLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    renderPassGrassDesc.finalLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    hRenderPassGrass = render::MakeRenderPass(renderPassGrassDesc, hRenderTarget);

    grassPassConstants = {};

    render::ResourceSetLayout::Entry grassResourceLayoutEntries[] =
    {
        {
            .resourceType = render::RESOURCE_DYNAMIC_STORAGE_BUFFER,
            .shaderStages = render::SHADER_TYPE_VERTEX
        },
    };
    hGrassResourceLayout = render::MakeResourceSetLayout(ARR_LEN(grassResourceLayoutEntries), 
            grassResourceLayoutEntries);
    render::ResourceSet::Entry grassResourceEntries[] =
    {
        {
            .binding = 0,
            .resourceType = render::RESOURCE_DYNAMIC_STORAGE_BUFFER,
            .hBuffer = hSbGrassInstanceData
        },
    };
    hGrassResourceSet = render::MakeResourceSet(hGrassResourceLayout, 
            ARR_LEN(grassResourceEntries), 
            grassResourceEntries);

    render::GraphicsPipelineDesc pipelineGrassDesc = {};
    pipelineGrassDesc.hVertexLayout = hVertexLayoutGrass;
    pipelineGrassDesc.hShaderVertex = hVsGrass;
    pipelineGrassDesc.hShaderPixel = hPsGrass;
    pipelineGrassDesc.pushConstantRangeCount = 1;
    pipelineGrassDesc.pushConstantRanges[0] =
    {
        .offset = 0,
        .size = sizeof(GrassPassConstantBlock),
        .shaderStages = render::SHADER_TYPE_VERTEX,
    };
    hGraphicsPipelineGrass = render::MakeGraphicsPipeline(hRenderPassGrass, pipelineGrassDesc, 1, &hGrassResourceLayout);
}

void ShutdownGrassSystem()
{
    DestroyArray(&grassInstanceData);
}

void PopulateGrassInstances(i32 frame, f32 terrainSize, Camera* camera)
{
    i32 frameOffset = (frame % RENDER_CONCURRENT_FRAMES) * maxGrassInstances;

    i32 grassInstancesPerSide = sqrtf(maxGrassInstances);
    f32 step = terrainSize / (f32)grassInstancesPerSide;
    for(i32 x = 0; x < grassInstancesPerSide; x++)
    {
        for(i32 y = 0; y < grassInstancesPerSide; y++)
        {
            GrassInstanceDataBlock instanceData = {};
            instanceData.position =
            {
                step * x,
                0,
                step * y,
            };
            i32 instanceIndex = x * grassInstancesPerSide + y + frameOffset;
            grassInstanceData[instanceIndex] = instanceData;
        }
    }

}

void UploadGrassInstances(i32 frame)
{
    u64 grassDataOffset = (frame % RENDER_CONCURRENT_FRAMES) * maxGrassInstances * sizeof(GrassInstanceDataBlock);
    u64 grassDataSize = maxGrassInstances * sizeof(GrassInstanceDataBlock);
    void* grassDataStart = (void*)((u64)grassInstanceData.data + grassDataOffset);
    render::CopyMemoryToBuffer(hSbGrassInstanceData, grassDataOffset, grassDataSize, grassDataStart);
}

void RenderGrassInstances(Handle<render::CommandBuffer> hCmd, i32 frame, Camera* camera)
{
    render::BeginRenderPass(hCmd, hRenderPassGrass);
    render::CmdBindGraphicsPipeline(hCmd, hGraphicsPipelineGrass);
    GrassPassConstantBlock constants = {};
    constants.view = math::Transpose(camera->GetView());
    constants.proj = math::Transpose(camera->GetProjection());
    render::CmdUpdatePushConstantRange(hCmd, 0, &constants, hGraphicsPipelineGrass);
    render::CmdSetViewport(hCmd, hRenderPassGrass);
    render::CmdSetScissor(hCmd, hRenderPassGrass);
    render::CmdBindVertexBuffer(hCmd, hVbGrass);
    render::CmdBindIndexBuffer(hCmd, hIbGrass);
    u32 resourceDynamicOffsets[] =
    {
        (frame % RENDER_CONCURRENT_FRAMES) * maxGrassInstances * (u32)sizeof(GrassInstanceDataBlock),
    };
    render::CmdBindGraphicsResources(hCmd, 
            hGraphicsPipelineGrass, 
            hGrassResourceSet, 0,
            ARR_LEN(resourceDynamicOffsets), resourceDynamicOffsets);
    //TODO(caio): Get instance count from culling instead of passing max every time
    render::CmdDrawIndexed(hCmd, hIbGrass, maxGrassInstances);
    render::EndRenderPass(hCmd, hRenderPassGrass);
}

};  // namespace Grass
};  // namespace ty
