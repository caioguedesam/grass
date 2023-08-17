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
//const i32 maxGrassInstances = 256;

struct GrassInstanceDataBlock
{
    math::v3f position = {};
    f32 padding0 = 0;
};

struct GrassPassConstantBlock
{
    math::m4f view = {};
    math::m4f proj = {};
};

inline Array<GrassInstanceDataBlock> grassInstanceData;
inline GrassPassConstantBlock grassPassConstants;

// Assets
inline Handle<asset::Model> hAssetModelGrass;
inline Handle<asset::Shader> hAssetVsGrass;
inline Handle<asset::Shader> hAssetPsGrass;

// Render structures
inline Handle<render::Shader> hVsGrass;
inline Handle<render::Shader> hPsGrass;
inline Handle<render::Buffer> hVbGrass;
inline Handle<render::Buffer> hIbGrass;
inline Handle<render::Buffer> hSbGrassInstanceData;

inline Handle<render::VertexLayout> hVertexLayoutGrass;
inline Handle<render::RenderPass> hRenderPassGrass;
inline Handle<render::ResourceSetLayout> hGrassResourceLayout;
inline Handle<render::ResourceSet> hGrassResourceSet;
inline Handle<render::GraphicsPipeline> hGraphicsPipelineGrass;

void InitGrassSystem(Handle<render::RenderTarget> hRenderTarget);
void ShutdownGrassSystem();

void PopulateGrassInstances(i32 frame, f32 terrainSize, Camera* camera);
void UploadGrassInstances(i32 frame);
void RenderGrassInstances(Handle<render::CommandBuffer> hCmd, i32 frame, Camera* camera);

};  // namespace Grass
};  // namespace ty
