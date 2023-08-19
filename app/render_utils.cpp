#include "app/render_utils.hpp"
#include "engine/src/render/render.hpp"

namespace ty
{
namespace Grass
{

Handle<render::Shader> MakeShaderFromAsset(Handle<asset::Shader> hAsset, render::ShaderType type)
{
    ASSERT(hAsset.IsValid());
    asset::Shader& asset = asset::shaders[hAsset];
    return render::MakeShader(type, asset.size, asset.data);
}

Handle<render::Texture> MakeTextureFromAsset(Handle<asset::Image> hAsset, render::ImageUsageFlags usage)
{
    ASSERT(hAsset.IsValid());
    asset::Image& asset = asset::images[hAsset];
    u64 assetSize = asset.width * asset.height * asset.channels;
    //TODO(caio): Figure out a better way to handle this besides
    // creating a new staging buffer every time
    Handle<render::Buffer> hStaging = render::MakeBuffer(
            render::BUFFER_TYPE_STAGING, 
            assetSize, 
            assetSize,
            asset.data);
    render::TextureDesc desc = {};
    desc.type = render::IMAGE_TYPE_2D; //TODO(caio): Hardcoded
    desc.width = asset.width;
    desc.height = asset.height;
    desc.mipLevels = render::GetMaxMipLevels(asset.width, asset.height);
    desc.format = render::FORMAT_RGBA8_SRGB; //TODO(caio): Hardcoded
    desc.usageFlags = usage;
    desc.layout = render::IMAGE_LAYOUT_UNDEFINED;
    Handle<render::Texture> result = render::MakeTexture(desc);

    // Upload data through staging buffer
    Handle<render::CommandBuffer> hCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_IMMEDIATE);
    render::BeginCommandBuffer(hCmd);
    render::Barrier barrier = {};
    barrier.srcAccess = render::MEMORY_ACCESS_NONE;
    barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.srcStage = render::PIPELINE_STAGE_TOP;
    barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    render::CmdPipelineBarrierTextureLayout(hCmd, 
            result, 
            render::IMAGE_LAYOUT_TRANSFER_DST, 
            barrier);
    render::CmdCopyBufferToTexture(hCmd, hStaging, result);
    barrier.srcAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_SHADER_READ;
    barrier.srcStage = render::PIPELINE_STAGE_TRANSFER;
    barrier.dstStage = render::PIPELINE_STAGE_VERTEX_SHADER;
    render::CmdPipelineBarrierTextureLayout(hCmd, 
            result, 
            render::IMAGE_LAYOUT_SHADER_READ_ONLY, 
            barrier);
    render::EndCommandBuffer(hCmd);
    render::SubmitImmediate(hCmd);

    return result;
}

Handle<render::Buffer> MakeVbFromAsset(Handle<asset::Model> hAsset)
{
    asset::Model& asset = asset::models[hAsset];
    return render::MakeBuffer(render::BUFFER_TYPE_VERTEX, 
            asset.vertices.count * sizeof(f32), 
            sizeof(f32),
            asset.vertices.data);
}

Handle<render::Buffer> MakeIbFromAsset(Handle<asset::Model> hAsset)
{
    //TODO(caio): No proper materials, just using index 0 from model asset
    asset::Model& asset = asset::models[hAsset];
    return render::MakeBuffer(render::BUFFER_TYPE_INDEX, 
            asset.groups[0].indices.count * sizeof(u32), 
            sizeof(u32),
            asset.groups[0].indices.data);
}

void InitDefaultRenderResources()
{
    render::SamplerDesc desc = {};
    hSamplerLinear = render::MakeSampler(desc);
}

};  // namespace Grass
};  // namespace ty
