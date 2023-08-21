#include "app\terrain.hpp"
#include "app\state.hpp"
#include "app\render_utils.hpp"

namespace ty
{
namespace Grass
{

// Base terrain quad on x-z plane
f32 terrainQuadData[] =
{
    // position (x, y, z), normal (x, y, z), uv (u, v)
    0.f, 0.f, 0.f,   0.f, 1.f, 0.f,  0.f, 0.f,
    1.f, 0.f, 0.f,   0.f, 1.f, 0.f,  0.f, 0.f,
    1.f, 0.f, 1.f,   0.f, 1.f, 0.f,  0.f, 0.f,
    0.f, 0.f, 1.f,   0.f, 1.f, 0.f,  0.f, 0.f,
};

u32 terrainQuadIndices[] =
{
    0, 1, 2, 0, 2, 3,
};

void InitTerrain(Handle<render::RenderTarget> hRenderTarget)
{
    // Asset
    hAssetVsTerrain = asset::LoadShader(file::MakePath(IStr(SHADER_PATH"terrain.vert")));
    hAssetPsTerrain = asset::LoadShader(file::MakePath(IStr(SHADER_PATH"terrain.frag")));

    // Graphics resources
    hVsTerrain = MakeShaderFromAsset(hAssetVsTerrain, render::SHADER_TYPE_VERTEX);
    hPsTerrain = MakeShaderFromAsset(hAssetPsTerrain, render::SHADER_TYPE_PIXEL);

    hVbTerrain = render::MakeBuffer(render::BUFFER_TYPE_VERTEX,
            ARR_LEN(terrainQuadData) * sizeof(f32),
            sizeof(f32),
            terrainQuadData);
    hIbTerrain = render::MakeBuffer(render::BUFFER_TYPE_INDEX,
            ARR_LEN(terrainQuadIndices) * sizeof(u32),
            sizeof(u32),
            terrainQuadIndices);
    terrainConstants = {};

    // Render pipeline
    render::RenderPassDesc renderPassTerrainRenderDesc = {};
    renderPassTerrainRenderDesc.loadOp = render::LOAD_OP_LOAD;
    renderPassTerrainRenderDesc.storeOp = render::STORE_OP_STORE;
    renderPassTerrainRenderDesc.initialLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    renderPassTerrainRenderDesc.finalLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    hRenderPassTerrainRender = render::MakeRenderPass(renderPassTerrainRenderDesc, hRenderTarget);

    render::VertexAttribute vertexAttributesTerrainRender[] =
    {
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V2F,
    };
    hVertexLayoutTerrainRender = render::MakeVertexLayout(ARR_LEN(vertexAttributesTerrainRender), vertexAttributesTerrainRender);

    render::GraphicsPipelineDesc pipelineTerrainDesc = {};
    pipelineTerrainDesc.hVertexLayout = hVertexLayoutTerrainRender;
    pipelineTerrainDesc.hShaderVertex = hVsTerrain;
    pipelineTerrainDesc.hShaderPixel = hPsTerrain;
    pipelineTerrainDesc.pushConstantRangeCount = 1;
    pipelineTerrainDesc.pushConstantRanges[0].offset = 0;
    pipelineTerrainDesc.pushConstantRanges[0].size = sizeof(TerrainConstantBlock);
    pipelineTerrainDesc.pushConstantRanges[0].shaderStages = render::SHADER_TYPE_VERTEX;
    hGraphicsPipelineTerrainRender = render::MakeGraphicsPipeline(hRenderPassTerrainRender, pipelineTerrainDesc, 0, NULL);

}

void ShutdownTerrain()
{
}

void UpdateTerrainConstants()
{
    terrainConstants.view = math::Transpose(appCamera.GetView());
    terrainConstants.proj = math::Transpose(appCamera.GetProjection());
    //TODO(caio): Update via egui
    terrainConstants.terrainSize = 256;
}

void RenderTerrain(Handle<render::CommandBuffer> hCmd)
{
    render::BeginRenderPass(hCmd, hRenderPassTerrainRender);
    render::CmdBindGraphicsPipeline(hCmd, hGraphicsPipelineTerrainRender);
    render::CmdUpdatePushConstantRange(hCmd, 0, &terrainConstants, hGraphicsPipelineTerrainRender);
    render::CmdSetViewport(hCmd, hRenderPassTerrainRender);
    render::CmdSetScissor(hCmd, hRenderPassTerrainRender);
    render::CmdBindVertexBuffer(hCmd, hVbTerrain);
    render::CmdBindIndexBuffer(hCmd, hIbTerrain);
    render::CmdDrawIndexed(hCmd, hIbTerrain, 1);
    render::EndRenderPass(hCmd, hRenderPassTerrainRender);
}

}
}
