#pragma once
#include "engine/src/asset/asset.hpp"
#include "engine/src/render/render.hpp"

#include "app/state.hpp"

namespace ty
{
namespace Grass
{

Handle<render::Shader> MakeShaderFromAsset(Handle<asset::Shader> hAsset, render::ShaderType type);
Handle<render::Texture> MakeTextureFromAsset(Handle<asset::Image> hAsset, render::ImageUsageFlags usage);
Handle<render::Buffer> MakeVbFromAsset(Handle<asset::Model> hAsset);
Handle<render::Buffer> MakeIbFromAsset(Handle<asset::Model> hAsset);

void InitDefaultRenderResources();

inline Handle<render::Sampler> hSamplerLinear;

};  // namespace Grass
};  // namespace ty
