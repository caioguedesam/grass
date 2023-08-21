#pragma once
#include "engine/src/core/base.hpp"
#include "engine/src/core/debug.hpp"
#include "engine/src/core/math.hpp"
#include "engine/src/core/ds.hpp"
#include "engine/src/asset/asset.hpp"
#include "engine/src/render/render.hpp"

#include "app/camera.hpp"

namespace ty
{
namespace Grass
{

const i32 maxGrassInstances = 256 * 256 * 16;

struct GrassInstanceDataBlock
{
    math::v3f position = {};
    f32 padding0 = 0;
    math::v2f uv = {};
    f32 padding1[2];
};

struct GrassConstantBlock
{
    math::m4f view = {};
    math::m4f proj = {};
    f32 worldTime = 0;
    f32 deltaTime = 0;
};

struct GrassUniformBlock
{
    f32 terrainSize = 256;    // Size of the terrain quad side in units
    f32 grassDensity = 0.5;   // How much grass blades per square unit
    math::v2f windDirection = {1, 1};   // Wind direction and magnitude. Affects wind noise tiling and grass blade deformation.
};

// Assets
inline Handle<asset::Model> hAssetModelGrass;
inline Handle<asset::Shader> hAssetCsGrassPositions;
inline Handle<asset::Shader> hAssetVsGrass;
inline Handle<asset::Shader> hAssetPsGrass;
inline Handle<asset::Image> hAssetWindNoise;

// Render resources
inline Handle<render::Shader> hCsGrassPositions;
inline Handle<render::Shader> hVsGrass;
inline Handle<render::Shader> hPsGrass;
inline Handle<render::Buffer> hVbGrass;
inline Handle<render::Buffer> hIbGrass;
inline Handle<render::Buffer> hSbGrassInstanceData;
inline GrassConstantBlock grassConstants;
inline GrassUniformBlock grassUniforms;
inline Handle<render::Buffer> hUbGrass;
inline Handle<render::Texture> hTexWindNoise;
inline Handle<render::Buffer> hStagingTexWindNoise;

// Grass position compute
inline Handle<render::ResourceSetLayout> hResourceLayoutGrassPositions;
inline Handle<render::ResourceSet> hResourceSetGrassPositions;
inline Handle<render::ComputePipeline> hComputePipelineGrassPositions;

// Grass render pass
inline Handle<render::VertexLayout> hVertexLayoutGrassRender;
inline Handle<render::RenderPass> hRenderPassGrassRender;
inline Handle<render::ResourceSetLayout> hResourceLayoutGrassRender;
inline Handle<render::ResourceSet> hResourceSetGrassRender;
inline Handle<render::GraphicsPipeline> hGraphicsPipelineGrassRender;

void InitGrass(Handle<render::RenderTarget> hRenderTarget);
void ShutdownGrass();

void InitGrassPositions();
void UpdateGrassConstants();
void UpdateGrassUniforms();
void RenderGrassInstances(Handle<render::CommandBuffer> hCmd);

};  // namespace Grass
};  // namespace ty
