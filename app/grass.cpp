#include "app/grass.hpp"
#include "engine/src/asset/asset.hpp"
#include "engine/src/core/debug.hpp"
#include "engine/src/core/ds.hpp"
#include "engine/src/core/file.hpp"
#include "engine/src/core/math.hpp"
#include "engine/src/core/time.hpp"
#include "engine/src/render/render.hpp"

#include "app/state.hpp"
#include "app/render_utils.hpp"

namespace ty
{
namespace Grass
{

void InitGrass(Handle<render::RenderTarget> hRenderTarget)
{
    ASSERT(IS_ALIGNED(sizeof(GrassInstanceDataBlock), render::GetBufferTypeAlignment(render::BUFFER_TYPE_STORAGE)));

    // Loading assets
    hAssetCsGrassPositions = asset::LoadShader(file::MakePath(IStr("app/shaders/grass_positions.comp")));
    hAssetVsGrass = asset::LoadShader(file::MakePath(IStr("app/shaders/grass.vert")));
    hAssetPsGrass = asset::LoadShader(file::MakePath(IStr("app/shaders/grass.frag")));
    hAssetModelGrass = asset::LoadModelOBJ(file::MakePath(IStr("resources/models/grass/grass.obj")));
    hAssetWindNoise = asset::LoadImageFile(file::MakePath(IStr("resources/textures/wind_noise.png")));

    // Initializing graphics resources
    hCsGrassPositions = MakeShaderFromAsset(hAssetCsGrassPositions, render::SHADER_TYPE_COMPUTE);
    hVsGrass = MakeShaderFromAsset(hAssetVsGrass, render::SHADER_TYPE_VERTEX);
    hPsGrass = MakeShaderFromAsset(hAssetPsGrass, render::SHADER_TYPE_PIXEL);
    hVbGrass = MakeVbFromAsset(hAssetModelGrass);
    hIbGrass = MakeIbFromAsset(hAssetModelGrass);
    hTexWindNoise = MakeTextureFromAsset(hAssetWindNoise,
            ENUM_FLAGS(render::ImageUsageFlags, render::IMAGE_USAGE_SAMPLED | render::IMAGE_USAGE_TRANSFER_DST));

    hSbGrassInstanceData = render::MakeBuffer(render::BUFFER_TYPE_STORAGE, 
            sizeof(GrassInstanceDataBlock) * maxGrassInstances, 
            sizeof(GrassInstanceDataBlock) * maxGrassInstances);

    grassRenderUniforms = {};
    grassRenderUniforms.windTiling = 0.1f;
    grassRenderUniforms.windStrength = 3.f;
    hUbGrassRenderUniforms = render::MakeBuffer(render::BUFFER_TYPE_UNIFORM, sizeof(GrassRenderUniformBlock), sizeof(GrassRenderUniformBlock), &grassRenderUniforms);

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

    grassRenderConstants = {};

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
    hGraphicsPipelineGrassRender = render::MakeGraphicsPipeline(hRenderPassGrassRender, pipelineGrassRenderDesc, 1, &hResourceLayoutGrassRender);

    InitGrassPositions();
}

void ShutdownGrass()
{
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


void RenderGrassInstances(Handle<render::CommandBuffer> hCmd)
{
    render::BeginRenderPass(hCmd, hRenderPassGrassRender);
    render::CmdBindGraphicsPipeline(hCmd, hGraphicsPipelineGrassRender);
    GrassRenderConstantBlock constants = {};
    constants.view = math::Transpose(appCamera.GetView());
    constants.proj = math::Transpose(appCamera.GetProjection());
    constants.worldTime = worldTime;
    constants.deltaTime = deltaTime;
    render::CmdUpdatePushConstantRange(hCmd, 0, &constants, hGraphicsPipelineGrassRender);
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
            hGraphicsPipelineGrassRender, 
            hResourceSetGrassRender, 0,
            0, NULL);
    render::CmdDrawIndexed(hCmd, hIbGrass, maxGrassInstances);
    render::EndRenderPass(hCmd, hRenderPassGrassRender);
}

};  // namespace Grass
};  // namespace ty
