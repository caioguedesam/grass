#include "app/render_utils.hpp"
#include "engine/src/core/base.hpp"
#include "engine/src/core/debug.hpp"
#include "engine/src/core/profile.hpp"
#include "engine/src/core/memory.hpp"
#include "engine/src/core/string.hpp"
#include "engine/src/core/math.hpp"
#include "engine/src/core/time.hpp"
#include "engine/src/core/input.hpp"
#include "engine/src/core/file.hpp"
#include "engine/src/core/async.hpp"
#include "engine/src/core/ds.hpp"
#include "engine/src/asset/asset.hpp"
#include "engine/src/render/window.hpp"
#include "engine/src/render/render.hpp"
#include "engine/src/render/egui.hpp"

#include "app/terrain.hpp"
#include "app/grass.hpp"

// Single compilation unit
#include "app/camera.cpp"
#include "app/state.cpp"
#include "app/render_utils.cpp"
#include "app/terrain.cpp"
#include "app/grass.cpp"

// TODO_LIST:
// App:
// - ...
// Engine:
// - Indirect draw for GPU driven rendering
// - Persistent buffer mapping

#define TERRAIN_SIZE 256

namespace ty
{
namespace Grass
{

render::Window window = {};
Handle<render::RenderTarget> hRenderTargetMain;
Handle<render::RenderPass> hRenderPassUI;

void AppInit()
{
    // Engine system initialization
    time::Init();
    input::Init();
    asset::Init();

    render::MakeWindow(&window, appWidth, appHeight, "Grass");
    render::Init(&window);

    // Default state
    InitState();
    InitDefaultRenderResources();

    // Render outputs
    render::RenderTargetDesc renderTargetMainDesc = {};
    renderTargetMainDesc.width = appWidth;
    renderTargetMainDesc.height = appHeight;
    renderTargetMainDesc.colorImageCount = 1;
    renderTargetMainDesc.colorImageFormats[0] = render::FORMAT_RGBA8_SRGB;
    renderTargetMainDesc.depthImageFormat = render::FORMAT_D32_FLOAT;
    hRenderTargetMain = render::MakeRenderTarget(renderTargetMainDesc);

    // App systems
    InitTerrain(hRenderTargetMain); 
    InitGrass(hRenderTargetMain);

    render::RenderPassDesc renderPassUIDesc = {};
    renderPassUIDesc.loadOp = render::LOAD_OP_LOAD;
    renderPassUIDesc.storeOp = render::STORE_OP_STORE;
    renderPassUIDesc.initialLayout = render::IMAGE_LAYOUT_COLOR_OUTPUT;
    renderPassUIDesc.finalLayout = render::IMAGE_LAYOUT_TRANSFER_SRC;
    hRenderPassUI = render::MakeRenderPass(renderPassUIDesc, hRenderTargetMain);
    egui::Init(&window, hRenderPassUI);

    // App settings
    input::SetMouseLock(true);
    input::SetMouseHide(true);
}

void AppShutdown()
{
    ShutdownGrass();

    egui::Shutdown();
    render::Shutdown();
    render::DestroyWindow(&window);

    mem::DestroyHeapAllocator(&appHeap);
}

void AppUpdate()
{
    window.PollMessages();

    input::Update();
    if(input::IsKeyDown(input::KEY_ESCAPE))
    {
        window.state = render::WINDOW_CLOSED;
        return;
    }
    if(input::IsKeyJustDown(input::KEY_P))
    {
        input::ToggleMouseHide();
        input::ToggleMouseLock();
    }

    if(input::IsMouseLocked())
    {
        math::v3f cameraInputPos = {0,0,0};
        if(input::IsKeyDown(input::KEY_W)) cameraInputPos.z += 1;
        if(input::IsKeyDown(input::KEY_S)) cameraInputPos.z -= 1;
        if(input::IsKeyDown(input::KEY_A)) cameraInputPos.x -= 1;
        if(input::IsKeyDown(input::KEY_D)) cameraInputPos.x += 1;
        math::v2f cameraInputRot = input::GetMouseDelta();
        RotateCamera(appCamera, cameraInputRot, TO_RAD(360.f) / 4.f, deltaTime);
        MoveCamera(appCamera, cameraInputPos, 50.f, deltaTime);
    }
}

void AppRender()
{
    // Frame setup
    Handle<render::CommandBuffer> hCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_FRAME, currentFrame);
    render::BeginFrame(currentFrame);
    render::BeginCommandBuffer(hCmd);
    egui::BeginFrame();

    // Frame commands
    // Clear render output
    render::Barrier barrier = {};
    barrier.srcAccess = render::MEMORY_ACCESS_NONE;
    barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.srcStage = render::PIPELINE_STAGE_TOP;
    barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    render::CmdPipelineBarrierTextureLayout(hCmd, render::GetColorOutput(hRenderTargetMain, 0), render::IMAGE_LAYOUT_TRANSFER_DST, barrier);
    render::CmdClearColorTexture(hCmd, render::GetColorOutput(hRenderTargetMain, 0), 1, 0.5, 0.3, 1);
    barrier.srcAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_COLOR_OUTPUT_WRITE;
    barrier.srcStage = render::PIPELINE_STAGE_TRANSFER;
    barrier.dstStage = render::PIPELINE_STAGE_COLOR_OUTPUT;
    render::CmdPipelineBarrierTextureLayout(hCmd, render::GetColorOutput(hRenderTargetMain, 0), render::IMAGE_LAYOUT_COLOR_OUTPUT, barrier);

    UpdateTerrainConstants();
    UpdateGrassConstants();
    UpdateGrassUniforms();

    RenderTerrain(hCmd);
    PopulateGrassPositions(hCmd);
    barrier.srcAccess = render::MEMORY_ACCESS_SHADER_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_SHADER_READ;
    barrier.srcStage = render::PIPELINE_STAGE_COMPUTE_SHADER;
    barrier.dstStage = render::PIPELINE_STAGE_VERTEX_SHADER;
    render::CmdPipelineBarrier(hCmd, barrier);
    RenderGrassInstances(hCmd);

    // Render GUI
    render::BeginRenderPass(hCmd, hRenderPassUI);
    egui::DrawFrame(hCmd);
    render::EndRenderPass(hCmd, hRenderPassUI);

    // Copy to swap chain image and end frame
    barrier.srcAccess = render::MEMORY_ACCESS_COLOR_OUTPUT_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_READ;
    barrier.srcStage = render::PIPELINE_STAGE_COLOR_OUTPUT;
    barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    render::CmdPipelineBarrierTextureLayout(hCmd, render::GetColorOutput(hRenderTargetMain, 0), render::IMAGE_LAYOUT_TRANSFER_SRC, barrier);
    render::CmdCopyToSwapChain(hCmd, render::GetColorOutput(hRenderTargetMain, 0));
    render::EndCommandBuffer(hCmd);
    render::EndFrame(currentFrame, hCmd);

    // Present
    render::Present(currentFrame);
}

};  // namespace Grass
};  // namespace ty

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR pCmdLine, int nCmdShow)
{
    using namespace ty;
    using namespace Grass;

    AppInit();

    while(window.state != render::WINDOW_CLOSED)
    {
        AppUpdate();
        AppRender();
        AdvanceState();
    }

    AppShutdown();
    return 0;
}

int main()
{
    return wWinMain(GetModuleHandle(NULL), NULL, GetCommandLineW(), SW_SHOWNORMAL);
}

// For reference: spinny cows (07/2023)
//int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR pCmdLine, int nCmdShow)
//{
    //using namespace ty;

    ////TestMemory();
    ////ASSERT(0);

    //time::Init();
    //asset::Init();

    //const u32 appWidth = 1920;
    //const u32 appHeight = 1080;

    //render::Window window;
    //render::MakeWindow(&window, appWidth, appHeight, "Typheus");

    //render::Init(&window);

    //mem::HeapAllocator generalHeap = mem::MakeHeapAllocator(MB(128));

    //// Shaders
    //file::Path vsPath = file::MakePath(IStr("./resources/shaders/default_quad.vert"));
    //file::Path psPath = file::MakePath(IStr("./resources/shaders/default_quad.frag"));
    //file::Path csPath = file::MakePath(IStr("./resources/shaders/update_instances.comp"));
    //Handle<asset::Shader> hAssetVs = asset::LoadShader(vsPath);
    //Handle<asset::Shader> hAssetPs = asset::LoadShader(psPath);
    //Handle<asset::Shader> hAssetCs = asset::LoadShader(csPath);
    //asset::Shader& assetVs = asset::shaders[hAssetVs];
    //asset::Shader& assetPs = asset::shaders[hAssetPs];
    //asset::Shader& assetCs = asset::shaders[hAssetCs];
    //Handle<render::Shader> hVsDefault = render::MakeShader(render::SHADER_TYPE_VERTEX, assetVs.size, assetVs.data);
    //Handle<render::Shader> hPsDefault = render::MakeShader(render::SHADER_TYPE_PIXEL, assetPs.size, assetPs.data);
    //Handle<render::Shader> hCsDefault = render::MakeShader(render::SHADER_TYPE_COMPUTE, assetCs.size, assetCs.data);

    //// Spot texture
    //file::Path spotTexturePath = file::MakePath(IStr("./resources/textures/viking_room.png"));
    //Handle<asset::Image> hSpotTextureAsset = asset::LoadImageFile(file::MakePath(IStr("./resources/models/spot/spot_texture.png")));
    //asset::Image& spotTextureAsset = asset::images[hSpotTextureAsset];
    //u64 spotTextureSize = spotTextureAsset.width * spotTextureAsset.height * 4;

    ////  Create staging buffer to store image memory from CPU
    //Handle<render::Buffer> hSpotTextureStagingBuffer = render::MakeBuffer(
            //render::BUFFER_TYPE_STAGING,
            //spotTextureSize,
            //spotTextureSize,
            //spotTextureAsset.data);
    //render::CopyMemoryToBuffer(hSpotTextureStagingBuffer, spotTextureSize, spotTextureAsset.data);

    ////  Create GPU texture
    //render::TextureDesc spotTextureDesc = {};
    //spotTextureDesc.type = render::IMAGE_TYPE_2D;
    //spotTextureDesc.width = spotTextureAsset.width;
    //spotTextureDesc.height = spotTextureAsset.height;
    //spotTextureDesc.mipLevels = render::GetMaxMipLevels(spotTextureAsset.width, spotTextureAsset.height);
    //spotTextureDesc.format = render::FORMAT_RGBA8_SRGB;
    //spotTextureDesc.viewType = render::IMAGE_VIEW_TYPE_2D;
    //spotTextureDesc.layout = render::IMAGE_LAYOUT_UNDEFINED;
    //spotTextureDesc.usageFlags = ENUM_FLAGS(render::ImageUsageFlags,
            //render::IMAGE_USAGE_SAMPLED
            //| render::IMAGE_USAGE_TRANSFER_DST
            //| render::IMAGE_USAGE_TRANSFER_SRC);
    //Handle<render::Texture> hSpotTexture = render::MakeTexture(spotTextureDesc);

    ////  Copy texture memory from CPU on staging buffer to GPU texture
    //Handle<render::CommandBuffer> hUploadCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_IMMEDIATE);
    //render::BeginCommandBuffer(hUploadCmd);
    //render::Barrier barrier = {};
    //barrier.srcAccess = render::MEMORY_ACCESS_NONE;
    //barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    //barrier.srcStage = render::PIPELINE_STAGE_TOP;
    //barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    //render::CmdPipelineBarrierTextureLayout(
            //hUploadCmd,
            //hSpotTexture, 
            //ty::render::IMAGE_LAYOUT_TRANSFER_DST,
            //barrier);
    //render::CmdCopyBufferToTexture(hUploadCmd, hSpotTextureStagingBuffer, hSpotTexture);
    //render::CmdGenerateMipmaps(hUploadCmd, hSpotTexture);
    //barrier.srcAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    //barrier.dstAccess = render::MEMORY_ACCESS_SHADER_READ;
    //barrier.srcStage = render::PIPELINE_STAGE_TRANSFER;
    //barrier.dstStage = render::PIPELINE_STAGE_FRAGMENT_SHADER;
    //render::CmdPipelineBarrierTextureLayout(
            //hUploadCmd,
            //hSpotTexture, 
            //ty::render::IMAGE_LAYOUT_SHADER_READ_ONLY,
            //barrier);
    //render::EndCommandBuffer(hUploadCmd);
    //render::SubmitImmediate(hUploadCmd);

    //// Default sampler
    //render::SamplerDesc defaultSamplerDesc = {};
    //Handle<render::Sampler> hDefaultSampler = MakeSampler(defaultSamplerDesc);

    //file::Path spotModelPath = file::MakePath(IStr("./resources/models/spot/spot.obj"));
    //Handle<asset::Model> hSpotModelAsset = asset::LoadModelOBJ(spotModelPath);
    //asset::Model& spotModelAsset = asset::models[hSpotModelAsset];
    //Handle<render::Buffer> hSpotVB = render::MakeBuffer(
            //render::BUFFER_TYPE_VERTEX,
            //spotModelAsset.vertices.count * sizeof(f32),
            //sizeof(f32),
            //spotModelAsset.vertices.data);
    //Handle<render::Buffer> hSpotIB = render::MakeBuffer(
            //render::BUFFER_TYPE_INDEX,
            //spotModelAsset.groups[0].indices.count * sizeof(u32),
            //sizeof(u32),
            //spotModelAsset.groups[0].indices.data);

    //struct SceneData
    //{
        //math::m4f view = {};
        //math::m4f proj = {};
    //};
    //ASSERT(IS_ALIGNED(sizeof(SceneData), render::GetBufferTypeAlignment(render::BUFFER_TYPE_UNIFORM)));

    //mem::SetContext(&generalHeap);
    //Array<SceneData> sceneData = MakeArrayAlign<SceneData>(RENDER_CONCURRENT_FRAMES, render::GetBufferTypeAlignment(render::BUFFER_TYPE_UNIFORM));
    //for(i32 i = 0; i < RENDER_CONCURRENT_FRAMES; i++)
    //{
        //SceneData sceneDataFrame;
        //sceneDataFrame.view = math::Transpose(math::LookAt({0, 0, -5}, {0, 0, 0}, {0, 1, 0}));
        //sceneDataFrame.proj = math::Transpose(math::Perspective(TO_RAD(45.f), (f32)appWidth/(f32)appHeight, 0.1f, 1000.f));
        //sceneData.Push(sceneDataFrame);
    //}

    //struct PerInstanceData
    //{
        //math::m4f world = {};
        //math::v4f color = {};
    //};
    //const i32 instanceCount = 4096;
    //Array<PerInstanceData> instanceData = MakeArray<PerInstanceData>(instanceCount, instanceCount, {});
    //// Initializing all instances to have random rotation and color
    //for(i32 i = 0; i < instanceCount; i++)
    //{
        //math::v3f randomPosition =
        //{
            ////(f32)i * 0.01f, 
            ////(f32)i * 0.01f,
            ////(f32)i * 0.01f,
            //math::RandomUniformF32(-10.f, 10.f),
            //math::RandomUniformF32(-10.f, 10.f),
            //math::RandomUniformF32(10.f, 5.f),
        //};
        ////instanceData[i].world = math::Transpose(math::TranslationMatrix(randomPosition) * math::RotationMatrix(randomRotationAngle, randomRotationAxis));
        //instanceData[i].world = math::Transpose(math::TranslationMatrix(randomPosition));
        //instanceData[i].color =
        //{
            ////1.f - ((f32)i / 200.f),
            ////1.f - ((f32)i / 200.f),
            ////1.f - ((f32)i / 200.f),
            //math::RandomUniformF32(),
            //math::RandomUniformF32(),
            //math::RandomUniformF32(),
            //1,
        //};
    //}

    //Handle<render::Buffer> hSceneDataBuffer = render::MakeBuffer(
            //render::BUFFER_TYPE_UNIFORM, 
            //sceneData.count * sizeof(SceneData),
            //sizeof(SceneData), 
            //sceneData.data);
    //Handle<render::Buffer> hInstanceDataBuffer = render::MakeBuffer(
            ////render::BUFFER_TYPE_UNIFORM, 
            //render::BUFFER_TYPE_STORAGE, 
            //sizeof(PerInstanceData) * instanceCount,
            ////sizeof(PerInstanceData), 
            //sizeof(PerInstanceData) * instanceCount, 
            //instanceData.data);

    //// Resource binding layouts
    //render::ResourceSetLayout::Entry resourceLayoutEntries[3];
    //resourceLayoutEntries[0].resourceType = render::RESOURCE_DYNAMIC_UNIFORM_BUFFER;
    //resourceLayoutEntries[0].shaderStages = render::SHADER_TYPE_VERTEX;
    //resourceLayoutEntries[1].resourceType = render::RESOURCE_SAMPLED_TEXTURE;
    //resourceLayoutEntries[1].shaderStages = render::SHADER_TYPE_PIXEL;
    //resourceLayoutEntries[2].resourceType = render::RESOURCE_DYNAMIC_STORAGE_BUFFER;
    //resourceLayoutEntries[2].shaderStages = ENUM_FLAGS(render::ShaderType, render::SHADER_TYPE_VERTEX | render::SHADER_TYPE_PIXEL);
    //Handle<render::ResourceSetLayout> hMainRenderPassResourceLayout = render::MakeResourceSetLayout(3, resourceLayoutEntries);
    //resourceLayoutEntries[0].resourceType = render::RESOURCE_DYNAMIC_STORAGE_BUFFER;
    //resourceLayoutEntries[0].shaderStages = render::SHADER_TYPE_COMPUTE;
    //Handle<render::ResourceSetLayout> hComputePassResourceLayout = render::MakeResourceSetLayout(1, resourceLayoutEntries);

    //// Resource binding sets
    //render::ResourceSet::Entry resourceSetEntries[3];
    //resourceSetEntries[0].binding = 0;
    //resourceSetEntries[0].resourceType = render::RESOURCE_DYNAMIC_UNIFORM_BUFFER;
    //resourceSetEntries[0].hBuffer = hSceneDataBuffer;
    //resourceSetEntries[1].binding = 1;
    //resourceSetEntries[1].resourceType = render::RESOURCE_SAMPLED_TEXTURE;
    //resourceSetEntries[1].hTexture = hSpotTexture;
    //resourceSetEntries[1].hSampler = hDefaultSampler;
    //resourceSetEntries[2].binding = 2;
    //resourceSetEntries[2].resourceType = render::RESOURCE_DYNAMIC_STORAGE_BUFFER;
    //resourceSetEntries[2].hBuffer = hInstanceDataBuffer;
    //Handle<render::ResourceSet> hMainRenderPassResources = render::MakeResourceSet(hMainRenderPassResourceLayout, 3, resourceSetEntries);
    //resourceSetEntries[0].binding = 0;
    //resourceSetEntries[0].resourceType = render::RESOURCE_DYNAMIC_STORAGE_BUFFER;
    //resourceSetEntries[0].hBuffer = hInstanceDataBuffer;
    //Handle<render::ResourceSet> hComputePassResources = render::MakeResourceSet(hComputePassResourceLayout, 1, resourceSetEntries);
    
    //// Main render pass (output will be copied to swap chain image)
    //render::RenderPassDesc mainRenderPassDesc = {};
    //mainRenderPassDesc.width = appWidth;
    //mainRenderPassDesc.height = appHeight;
    //mainRenderPassDesc.loadOp = render::LOAD_OP_LOAD;
    //mainRenderPassDesc.storeOp = render::STORE_OP_STORE;
    //mainRenderPassDesc.initialLayout = render::IMAGE_LAYOUT_TRANSFER_DST;
    //mainRenderPassDesc.finalLayout = render::IMAGE_LAYOUT_TRANSFER_SRC;
    //render::Format mainRenderPassColorFormats[] =
    //{
        //render::FORMAT_RGBA8_SRGB,
    //};
    //Handle<render::RenderPass> hRenderPassMain = render::MakeRenderPass(mainRenderPassDesc, 1, mainRenderPassColorFormats, render::FORMAT_D32_FLOAT);

    //// Graphics pipeline 
    //render::VertexAttribute defaultVertexAttributes[] =
    //{
        //render::VERTEX_ATTR_V3F,
        //render::VERTEX_ATTR_V3F,
        //render::VERTEX_ATTR_V2F,
    //};
    //Handle<render::VertexLayout> hVertexLayoutDefault = render::MakeVertexLayout(ARR_LEN(defaultVertexAttributes), defaultVertexAttributes);

    //render::GraphicsPipelineDesc defaultPipelineDesc = {};
    //defaultPipelineDesc.hVertexLayout = hVertexLayoutDefault;
    //defaultPipelineDesc.hShaderVertex = hVsDefault;
    //defaultPipelineDesc.hShaderPixel = hPsDefault;
    //defaultPipelineDesc.frontFace = render::FRONT_FACE_CCW;
    //defaultPipelineDesc.cullMode = render::CULL_MODE_BACK;
    //defaultPipelineDesc.primitive = render::PRIMITIVE_TRIANGLE_LIST;
    //defaultPipelineDesc.fillMode = render::FILL_MODE_SOLID;
    //Handle<render::GraphicsPipeline> hGraphicsPipelineDefault = render::MakeGraphicsPipeline(hRenderPassMain,
            //defaultPipelineDesc,
            //1,
            //&hMainRenderPassResourceLayout);

    //// Compute pipeline
    //struct ComputePushConstants
    //{
        //math::m4f rotation;
        //f32 dt;
    //};
    //render::ComputePipelineDesc computePipelineDesc = {};
    //computePipelineDesc.hShaderCompute = hCsDefault;
    //computePipelineDesc.pushConstantRangeCount = 1;
    //computePipelineDesc.pushConstantRanges[0].offset = 0;
    //computePipelineDesc.pushConstantRanges[0].size = sizeof(ComputePushConstants);
    //computePipelineDesc.pushConstantRanges[0].shaderStages = render::SHADER_TYPE_COMPUTE;
    //Handle<render::ComputePipeline> hComputePipelineDefault = render::MakeComputePipeline(computePipelineDesc, 1, &hComputePassResourceLayout);

    ////egui::Init(hRenderPassMain);

    //i32 frame = 0;
    //time::Timer frameTimer;
    //f32 deltaTime = 0;
    //math::v3f cameraPos = {0, 0, -5};
    ////sceneData[i].view = math::Transpose(math::LookAt({0, 0, -5}, {0, 0, 0}, {0, 1, 0}));
    //while(window.state != render::WINDOW_CLOSED)
    //{
        //PROFILE_FRAME;
        //frameTimer.Start();

        //window.PollMessages();

        //// Frame setup
        //Handle<render::CommandBuffer> hCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_FRAME, frame);
        //render::BeginFrame(frame);
        //render::BeginCommandBuffer(hCmd);

        ////egui::BeginFrame();


        //// Updating frame data
        //cameraPos = cameraPos + (math::v3f{0, 0, -1} * deltaTime);
        //SceneData& sceneDataFrame = sceneData[frame % RENDER_CONCURRENT_FRAMES];
        //sceneDataFrame.view = math::Transpose(math::LookAt(cameraPos, {0, 0, 0}, {0, 1, 0}));
        //CopyMemoryToBuffer(hSceneDataBuffer, sceneData.count * sizeof(SceneData), sceneData.data);

        //// Frame commands 
        //Handle<render::Texture> hRenderPassMainOutput = render::GetRenderPassOutput(hRenderPassMain, 0);

        //render::Barrier barrier = {};
        //barrier.srcAccess = render::MEMORY_ACCESS_NONE;
        //barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
        //barrier.srcStage = render::PIPELINE_STAGE_TOP;
        //barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
        //render::CmdPipelineBarrierTextureLayout(hCmd, hRenderPassMainOutput, 
                //ty::render::IMAGE_LAYOUT_TRANSFER_DST,
                //barrier);
        //math::v3f clearColor =
        //{
            //math::Lerp(1, 0, (f32)(frame % 5000)/5000.f),
            //math::Lerp(0, 1, (f32)(frame % 5000)/5000.f),
            //1,
        //};
        //render::CmdClearColorTexture(hCmd, hRenderPassMainOutput, clearColor.r, clearColor.g, clearColor.b, 1);

        //// Update colors with compute pass
        //ComputePushConstants pushConstants = {};
        //pushConstants.rotation = math::RotationMatrix(TO_RAD(45.f * deltaTime), {0,1,0});
        //pushConstants.dt = deltaTime;
        //render::CmdBindComputePipeline(hCmd, hComputePipelineDefault);
        //u32 computeDynamicOffsets[] = {0};
        //render::CmdBindComputeResources(hCmd, hComputePipelineDefault, hComputePassResources, 0, ARR_LEN(computeDynamicOffsets), computeDynamicOffsets);
        //render::CmdUpdatePushConstantRange(hCmd, 0, &pushConstants, hComputePipelineDefault);
        //render::CmdDispatch(hCmd, instanceCount / 16, 1, 1);

        //barrier.srcAccess = render::MEMORY_ACCESS_SHADER_WRITE;
        //barrier.srcStage = render::PIPELINE_STAGE_COMPUTE_SHADER;
        //barrier.dstAccess = render::MEMORY_ACCESS_SHADER_READ;
        //barrier.dstStage = render::PIPELINE_STAGE_VERTEX_SHADER;
        //render::CmdPipelineBarrier(hCmd, barrier);

        //render::BeginRenderPass(hCmd, hRenderPassMain);
        //render::CmdBindGraphicsPipeline(hCmd, hGraphicsPipelineDefault);
        //render::CmdSetViewport(hCmd, hRenderPassMain);
        //render::CmdSetScissor(hCmd, hRenderPassMain);
        //u32 mainRenderPassDynamicOffsets[] = {(u32)sizeof(SceneData) * (frame % RENDER_CONCURRENT_FRAMES), 0};
        //render::CmdBindGraphicsResources(hCmd, hGraphicsPipelineDefault, hMainRenderPassResources, 0, ARR_LEN(mainRenderPassDynamicOffsets), mainRenderPassDynamicOffsets);

        //// Draw all instances
        //render::CmdBindVertexBuffer(hCmd, hSpotVB);
        //render::CmdBindIndexBuffer(hCmd, hSpotIB);
        //render::CmdDrawIndexed(hCmd, hSpotIB, instanceCount);

        ////egui::DrawFrame(cmd);
        //render::EndRenderPass(hCmd, hRenderPassMain);

        //barrier.srcAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
        //barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_READ;
        //barrier.srcStage = render::PIPELINE_STAGE_TRANSFER;
        //barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
        //render::CmdPipelineBarrierTextureLayout(hCmd, hRenderPassMainOutput, 
                //ty::render::IMAGE_LAYOUT_TRANSFER_SRC,
                //barrier);
        //render::CmdCopyToSwapChain(hCmd, hRenderPassMainOutput);

        //// Frame teardown
        //render::EndCommandBuffer(hCmd);
        //render::EndFrame(frame, hCmd);

        //// Present
        //render::Present(frame);

        //frameTimer.Stop();
        //LOGF("Frame %d: %.4lf ms", frame, (f32)frameTimer.GetElapsedMS());
        //deltaTime = (f32)frameTimer.GetElapsedS();
        //frame++;
    //}

    ////egui::Shutdown();
    //render::Shutdown();
    //render::DestroyWindow(&window);

    //mem::SetContext(&generalHeap);
    //DestroyArray(&instanceData);
    //mem::DestroyHeapAllocator(&generalHeap);

    //PROFILE_END;
    //return 0;
//}
