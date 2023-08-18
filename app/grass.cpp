#include "app/grass.hpp"
#include "engine/src/asset/asset.hpp"
#include "engine/src/core/debug.hpp"
#include "engine/src/core/ds.hpp"
#include "engine/src/core/file.hpp"
#include "engine/src/core/math.hpp"
#include "engine/src/core/time.hpp"
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

    //grassInstanceData = MakeArrayAlign<GrassInstanceDataBlock>(
            //maxGrassInstances * RENDER_CONCURRENT_FRAMES,
            //maxGrassInstances * RENDER_CONCURRENT_FRAMES,
            //{},
            //render::GetBufferTypeAlignment(render::BUFFER_TYPE_STORAGE));

    // Initializing graphics resources
    hAssetCsGrassPositions = asset::LoadShader(file::MakePath(IStr("app/shaders/grass_positions.comp")));
    asset::Shader& assetShader = asset::shaders[hAssetCsGrassPositions];
    hCsGrassPositions = render::MakeShader(render::SHADER_TYPE_COMPUTE, assetShader.size, assetShader.data);

    hAssetVsGrass = asset::LoadShader(file::MakePath(IStr("app/shaders/grass.vert")));
    assetShader = asset::shaders[hAssetVsGrass];
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

    //hSbGrassInstanceData = render::MakeBuffer(render::BUFFER_TYPE_STORAGE,
            //sizeof(GrassInstanceDataBlock) * grassInstanceData.count, 
            //sizeof(GrassInstanceDataBlock) * maxGrassInstances);
    hSbGrassInstanceData = render::MakeBuffer(render::BUFFER_TYPE_STORAGE, 
            sizeof(GrassInstanceDataBlock) * maxGrassInstances, 
            sizeof(GrassInstanceDataBlock) * maxGrassInstances);

    grassRenderUniforms = {};
    grassRenderUniforms.windTiling = 0.1f;
    grassRenderUniforms.windStrength = 3.f;
    hUbGrassRenderUniforms = render::MakeBuffer(render::BUFFER_TYPE_UNIFORM, sizeof(GrassRenderUniformBlock), sizeof(GrassRenderUniformBlock), &grassRenderUniforms);

    // MAKING WIND TEXTURE
    // //TODO(caio): ABSTRACT THIS LATER
    hAssetWindNoise = asset::LoadImageFile(file::MakePath(IStr("resources/textures/wind_noise.png")));
    asset::Image& assetWindNoise = asset::images[hAssetWindNoise];
    //TODO(caio): Move this staging process somewhere else for more general functions
    u64 imageSize = assetWindNoise.width * assetWindNoise.height * assetWindNoise.channels;
    hStagingTexWindNoise = render::MakeBuffer(render::BUFFER_TYPE_STAGING, 
            imageSize, 
            imageSize,
            assetWindNoise.data);
    render::TextureDesc texWindNoiseDesc = {};
    texWindNoiseDesc.type = render::IMAGE_TYPE_2D;
    texWindNoiseDesc.width = assetWindNoise.width;
    texWindNoiseDesc.height = assetWindNoise.height;
    texWindNoiseDesc.mipLevels = render::GetMaxMipLevels(assetWindNoise.width, assetWindNoise.height);
    //TODO(caio): This should likely be 1 channel alpha? Instead of 4 channels rgba
    texWindNoiseDesc.format = render::FORMAT_RGBA8_SRGB;
    texWindNoiseDesc.usageFlags = ENUM_FLAGS(render::ImageUsageFlags,
            render::IMAGE_USAGE_SAMPLED | render::IMAGE_USAGE_TRANSFER_DST);
    hTexWindNoise = render::MakeTexture(texWindNoiseDesc);

    Handle<render::CommandBuffer> hCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_IMMEDIATE);
    render::BeginCommandBuffer(hCmd);
    render::Barrier barrier = {};
    barrier.srcAccess = render::MEMORY_ACCESS_NONE;
    barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.srcStage = render::PIPELINE_STAGE_TOP;
    barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    render::CmdPipelineBarrierTextureLayout(hCmd, 
            hTexWindNoise, 
            render::IMAGE_LAYOUT_TRANSFER_DST, 
            barrier);
    render::CmdCopyBufferToTexture(hCmd, hStagingTexWindNoise, hTexWindNoise);
    barrier.srcAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_SHADER_READ;
    barrier.srcStage = render::PIPELINE_STAGE_TRANSFER;
    barrier.dstStage = render::PIPELINE_STAGE_VERTEX_SHADER;
    render::CmdPipelineBarrierTextureLayout(hCmd, 
            hTexWindNoise, 
            render::IMAGE_LAYOUT_SHADER_READ_ONLY, 
            barrier);
    render::EndCommandBuffer(hCmd);
    render::SubmitImmediate(hCmd);

    render::SamplerDesc samplerLinearDesc = {};
    hSamplerLinear = render::MakeSampler(samplerLinearDesc);

    render::VertexAttribute vertexAttributesGrass[] =
    {
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V2F,
    };
    hVertexLayoutGrassRender = render::MakeVertexLayout(ARR_LEN(vertexAttributesGrass), vertexAttributesGrass);

    render::RenderPassDesc renderPassGrassDesc = {};
    renderPassGrassDesc.loadOp = render::LOAD_OP_LOAD;
    renderPassGrassDesc.storeOp = render::STORE_OP_STORE;
    renderPassGrassDesc.initialLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    renderPassGrassDesc.finalLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    hRenderPassGrassRender = render::MakeRenderPass(renderPassGrassDesc, hRenderTarget);

    grassPassConstants = {};

    render::ResourceSetLayout::Entry grassPositionsResourceLayoutEntries[] =
    {
        {
            .resourceType = render::RESOURCE_STORAGE_BUFFER,
            .shaderStages = render::SHADER_TYPE_COMPUTE
        },
    };
    hResourceLayoutGrassPositions = render::MakeResourceSetLayout(ARR_LEN(grassPositionsResourceLayoutEntries), 
            grassPositionsResourceLayoutEntries);
    render::ResourceSet::Entry grassPositionsResourceSetEntries[] =
    {
        {
            .binding = 0,
            .resourceType = render::RESOURCE_STORAGE_BUFFER,
            .hBuffer = hSbGrassInstanceData
        },
    };
    hResourceSetGrassPositions = render::MakeResourceSet(hResourceLayoutGrassPositions, 
            ARR_LEN(grassPositionsResourceSetEntries), 
            grassPositionsResourceSetEntries);

    render::ResourceSetLayout::Entry grassRenderResourceLayoutEntries[] =
    {
        {
            .resourceType = render::RESOURCE_STORAGE_BUFFER,
            .shaderStages = render::SHADER_TYPE_VERTEX
        },
        {
            .resourceType = render::RESOURCE_UNIFORM_BUFFER,
            .shaderStages = render::SHADER_TYPE_VERTEX
        },
        {
            .resourceType = render::RESOURCE_SAMPLED_TEXTURE,
            .shaderStages = render::SHADER_TYPE_VERTEX
        },
    };
    hResourceLayoutGrassRender = render::MakeResourceSetLayout(ARR_LEN(grassRenderResourceLayoutEntries), 
            grassRenderResourceLayoutEntries);
    render::ResourceSet::Entry grassRenderResourceSetEntries[] =
    {
        {
            .binding = 0,
            .resourceType = render::RESOURCE_STORAGE_BUFFER,
            .hBuffer = hSbGrassInstanceData
        },
        {
            .binding = 1,
            .resourceType = render::RESOURCE_UNIFORM_BUFFER,
            .hBuffer = hUbGrassRenderUniforms
        },
        {
            .binding = 2,
            .resourceType = render::RESOURCE_SAMPLED_TEXTURE,
            .hTexture = hTexWindNoise,
            .hSampler = hSamplerLinear
        },
    };
    hResourceSetGrassRender = render::MakeResourceSet(hResourceLayoutGrassRender, 
            ARR_LEN(grassRenderResourceSetEntries), 
            grassRenderResourceSetEntries);

    render::ComputePipelineDesc pipelineGrassPositionsDesc = {};
    pipelineGrassPositionsDesc.pushConstantRangeCount = 1;
    pipelineGrassPositionsDesc.pushConstantRanges[0] =
    {
        .offset = 0,
        .size = sizeof(GrassPositionConstantBlock),
        .shaderStages = render::SHADER_TYPE_COMPUTE,
    };
    pipelineGrassPositionsDesc.hShaderCompute = hCsGrassPositions;
    hComputePipelineGrassPositions = render::MakeComputePipeline(pipelineGrassPositionsDesc, 1, &hResourceLayoutGrassPositions);

    render::GraphicsPipelineDesc pipelineGrassRenderDesc = {};
    pipelineGrassRenderDesc.hVertexLayout = hVertexLayoutGrassRender;
    pipelineGrassRenderDesc.hShaderVertex = hVsGrass;
    pipelineGrassRenderDesc.hShaderPixel = hPsGrass;
    pipelineGrassRenderDesc.pushConstantRangeCount = 1;
    pipelineGrassRenderDesc.pushConstantRanges[0] =
    {
        .offset = 0,
        .size = sizeof(GrassRenderConstantBlock),
        .shaderStages = render::SHADER_TYPE_VERTEX,
    };
    hGraphicsPipelineGrass = render::MakeGraphicsPipeline(hRenderPassGrassRender, pipelineGrassRenderDesc, 1, &hResourceLayoutGrassRender);

    InitGrassPositions();
}

void ShutdownGrassSystem()
{
    //DestroyArray(&grassInstanceData);
}

void InitGrassPositions()
{
    // Submit once at init time to populate SSBO with all grass positions
    Handle<render::CommandBuffer> hCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_IMMEDIATE);
    render::BeginCommandBuffer(hCmd);
    render::CmdBindComputePipeline(hCmd, hComputePipelineGrassPositions);
    GrassPositionConstantBlock constants = {};
    //TODO(caio): Use this value from elsewhere...
    constants.terrainSize = 256;
    constants.maxGrassBladeCount = maxGrassInstances;
    render::CmdUpdatePushConstantRange(hCmd, 0, &constants, hComputePipelineGrassPositions);
    render::CmdBindComputeResources(hCmd, hComputePipelineGrassPositions, hResourceSetGrassPositions, 0);
    const i32 bladesPerSide = (i32)(sqrt(maxGrassInstances));
    const i32 localSizeX = 16;
    const i32 localSizeY = 16;
    render::CmdDispatch(hCmd, bladesPerSide/localSizeX, bladesPerSide/localSizeY, 1);
    //render::CmdDispatch(hCmd, maxGrassInstances, 1, 1);
    render::EndCommandBuffer(hCmd);
    time::Timer timer = {};
    timer.Start();
    render::SubmitImmediate(hCmd);
    timer.Stop();
    LOGLF("GRASS", "InitGrassPositions: %.4f ms", timer.GetElapsedMS());
}


void RenderGrassInstances(Handle<render::CommandBuffer> hCmd, Camera* camera, f32 worldTime, f32 dt)
{
    render::BeginRenderPass(hCmd, hRenderPassGrassRender);
    render::CmdBindGraphicsPipeline(hCmd, hGraphicsPipelineGrass);
    GrassRenderConstantBlock constants = {};
    constants.view = math::Transpose(camera->GetView());
    constants.proj = math::Transpose(camera->GetProjection());
    constants.worldTime = worldTime;
    constants.deltaTime = dt;
    render::CmdUpdatePushConstantRange(hCmd, 0, &constants, hGraphicsPipelineGrass);
    render::CmdSetViewport(hCmd, hRenderPassGrassRender);
    render::CmdSetScissor(hCmd, hRenderPassGrassRender);
    render::CmdBindVertexBuffer(hCmd, hVbGrass);
    render::CmdBindIndexBuffer(hCmd, hIbGrass);
    //u32 resourceDynamicOffsets[] =
    //{
        //(frame % RENDER_CONCURRENT_FRAMES) * maxGrassInstances * (u32)sizeof(GrassInstanceDataBlock),
    //};
    //render::CmdBindGraphicsResources(hCmd, 
            //hGraphicsPipelineGrass, 
            //hResourceSetGrassRender, 0,
            //ARR_LEN(resourceDynamicOffsets), resourceDynamicOffsets);
    render::CmdBindGraphicsResources(hCmd, 
            hGraphicsPipelineGrass, 
            hResourceSetGrassRender, 0,
            0, NULL);
    render::CmdDrawIndexed(hCmd, hIbGrass, maxGrassInstances);
    render::EndRenderPass(hCmd, hRenderPassGrassRender);
}

};  // namespace Grass
};  // namespace ty
