#pragma once
#include "engine/src/core/math.hpp"
#include "engine/src/asset/asset.hpp"
#include "engine/src/render/render.hpp"

namespace ty
{
namespace Grass
{

struct TerrainConstantBlock
{
    math::m4f view = {};
    math::m4f proj = {};
    f32 terrainSize = 256;
};

// Assets
inline Handle<asset::Shader> hAssetVsTerrain;
inline Handle<asset::Shader> hAssetPsTerrain;

// Render resources
inline Handle<render::Shader> hVsTerrain;
inline Handle<render::Shader> hPsTerrain;
inline Handle<render::Buffer> hVbTerrain;
inline Handle<render::Buffer> hIbTerrain;
inline TerrainConstantBlock terrainConstants;
//TODO(caio): Heightmap stuff

// Terrain render pass
inline Handle<render::VertexLayout> hVertexLayoutTerrainRender;
inline Handle<render::RenderPass> hRenderPassTerrainRender;
//inline Handle<render::ResourceSetLayout> hResourceLayoutTerrainRender;
//inline Handle<render::ResourceSet> hResourceSetTerrainRender;
inline Handle<render::GraphicsPipeline> hGraphicsPipelineTerrainRender;
    
void InitTerrain(Handle<render::RenderTarget> hRenderTarget);
void ShutdownTerrain();

void UpdateTerrainConstants();
void RenderTerrain(Handle<render::CommandBuffer> hCmd);

};  // namespace Grass
};  // namespace ty
