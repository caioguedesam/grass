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

#include "app/camera.hpp"

// Single compilation unit
#include "app/camera.cpp"

// TODO_LIST:
// App:
// - Calculate grass positions on GPU
// - Jitter grass positions on GPU
// - ...
// - GUI render pass
// Engine:
// - Rethink string library
//      - Support strf
// - Revisit all coordinate system stuff (for camera app, math matrices)
//      - Canonical for typheus: Left handed Y up X right Z forward
//      - Vulkan NDC: Right handed Y down X right Z forward
//      - Always send matrices to shaders as transposed and transformed to Vk coordinates
// - Separation of render target from render pass
//      - This is so multiple render passes can render to the same render targets
// - Indirect draw?

#define SHADER_PATH "./app/shaders/"

#define APP_W 1920
#define APP_H 1080
#define TERRAIN_SIZE 256

namespace Grass
{

using namespace ty;

// App data
// Base terrain quad on x-z plane
f32 terrainQuadData[] =
{
    // position (x, y, z), normal (x, y, z), uv (u, v)
    -0.5f, 0.f, -0.5f,  0.f, 1.f, 0.f, 0.f, 0.f,
    0.5f, 0.f, -0.5f,   0.f, 1.f, 0.f, 0.f, 0.f,
    0.5f, 0.f, 0.5f,    0.f, 1.f, 0.f, 0.f, 0.f,
    -0.5f, 0.f, 0.5f,   0.f, 1.f, 0.f, 0.f, 0.f,
};

u32 terrainQuadIndices[] =
{
    0, 2, 1, 0, 3, 2,
    //TODO(caio): CONTINUE
    // - Why does this work as front when I have front as CCW? Verify
    // - Compute shader for sampling uniform points on terrain
    // - Render sampled points
    // - Introduce noise in sampling
};

// App assets
Handle<asset::Shader> hAssetVsTerrain;
Handle<asset::Shader> hAssetPsTerrain;

// App resources
Handle<render::Shader> hVsTerrain;
Handle<render::Shader> hPsTerrain;
Handle<render::Buffer> hVbTerrain;
Handle<render::Buffer> hIbTerrain;
struct ConstantBlockTerrain
{
    math::m4f view = {};
    math::m4f proj = {};
};
ConstantBlockTerrain constantsTerrain;

// App render pipeline
Handle<render::RenderPass> hRenderPassMain;
Handle<render::VertexLayout> hVertexLayoutTerrain;
Handle<render::GraphicsPipeline> hPipelineTerrain;

// App global state
render::Window window = {};
mem::HeapAllocator generalHeap = {};
//math::v3f cameraPos = {0,2,-1};
//math::v3f cameraTarget = {0,0,0};
//Camera camera = MakeCamera({0, 2, -1}, math::Normalize(math::v3f{0, -2, 1}), TO_RAD(45.f), (f32)APP_W/(f32)APP_H);
Camera camera = {};

i32 frame = 0;
f32 dt = 0;
time::Timer frameTimer = {};

void InitShader(render::ShaderType type, file::Path assetPath, Handle<asset::Shader>* handle, Handle<render::Shader>* resource)
{
    ASSERT(handle && resource);
    *handle = asset::LoadShader(assetPath);
    asset::Shader& assetShader = asset::shaders[*handle];
    *resource = render::MakeShader(type, assetShader.size, assetShader.data);
}

void AppInit()
{
    // Systems
    time::Init();
    input::Init();
    asset::Init();

    render::MakeWindow(&window, APP_W, APP_H, "Grass");
    render::Init(&window);

    generalHeap = mem::MakeHeapAllocator(MB(128));

    // Assets and resources
    InitShader(render::SHADER_TYPE_VERTEX, file::MakePath(IStr(SHADER_PATH"terrain.vert")), &hAssetVsTerrain, &hVsTerrain);
    InitShader(render::SHADER_TYPE_PIXEL, file::MakePath(IStr(SHADER_PATH"terrain.frag")), &hAssetPsTerrain, &hPsTerrain);
    hVbTerrain = render::MakeBuffer(render::BUFFER_TYPE_VERTEX,
            ARR_LEN(terrainQuadData) * sizeof(f32),
            sizeof(f32),
            terrainQuadData);
    hIbTerrain = render::MakeBuffer(render::BUFFER_TYPE_INDEX,
            ARR_LEN(terrainQuadIndices) * sizeof(u32),
            sizeof(u32),
            terrainQuadIndices);

    // Render pipeline
    render::RenderPassDesc renderPassMainDesc = {};
    renderPassMainDesc.width = APP_W;
    renderPassMainDesc.height = APP_H;
    renderPassMainDesc.loadOp = render::LOAD_OP_LOAD;
    renderPassMainDesc.storeOp = render::STORE_OP_STORE;
    renderPassMainDesc.initialLayout = render::IMAGE_LAYOUT_TRANSFER_DST;
    renderPassMainDesc.finalLayout = render::IMAGE_LAYOUT_TRANSFER_SRC;
    render::Format renderPassMainColorFormats[] =
    {
        render::FORMAT_RGBA8_SRGB,
    };
    hRenderPassMain = render::MakeRenderPass(renderPassMainDesc, 1, renderPassMainColorFormats, render::FORMAT_D32_FLOAT);

    render::VertexAttribute vertexAttributesTerrain[] =
    {
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V3F,
        render::VERTEX_ATTR_V2F,
    };
    hVertexLayoutTerrain = render::MakeVertexLayout(ARR_LEN(vertexAttributesTerrain), vertexAttributesTerrain);

    render::GraphicsPipelineDesc pipelineTerrainDesc = {};
    pipelineTerrainDesc.hVertexLayout = hVertexLayoutTerrain;
    pipelineTerrainDesc.hShaderVertex = hVsTerrain;
    pipelineTerrainDesc.hShaderPixel = hPsTerrain;
    pipelineTerrainDesc.pushConstantRangeCount = 1;
    pipelineTerrainDesc.pushConstantRanges[0].offset = 0;
    pipelineTerrainDesc.pushConstantRanges[0].size = sizeof(ConstantBlockTerrain);
    pipelineTerrainDesc.pushConstantRanges[0].shaderStages = render::SHADER_TYPE_VERTEX;
    hPipelineTerrain = render::MakeGraphicsPipeline(hRenderPassMain, pipelineTerrainDesc, 0, NULL);

    egui::Init(hRenderPassMain);

    //camera = MakeCamera({0, 2, -1}, 0, TO_RAD(-90.f), TO_RAD(45.f), (f32)APP_W/(f32)APP_H);
    camera = MakeCamera({0, 2, -1}, math::Normalize(math::v3f{0,-2,1}), TO_RAD(45.f), (f32)APP_W/(f32)APP_H);

    input::SetMouseLock(true);
    input::SetMouseHide(true);
}

void AppShutdown()
{
    egui::Shutdown();
    render::Shutdown();
    render::DestroyWindow(&window);

    mem::DestroyHeapAllocator(&generalHeap);
}

void AppUpdate()
{
    window.PollMessages();
    // TODO(caio): Set camera movement

    input::Update();
    if(input::IsKeyDown(input::KEY_ESCAPE))
    {
        window.state = render::WINDOW_CLOSED;
        return;
    }

    math::v3f cameraInputPos = {0,0,0};
    if(input::IsKeyDown(input::KEY_W)) cameraInputPos.z += 1;
    if(input::IsKeyDown(input::KEY_S)) cameraInputPos.z -= 1;
    if(input::IsKeyDown(input::KEY_A)) cameraInputPos.x += 1;
    if(input::IsKeyDown(input::KEY_D)) cameraInputPos.x -= 1;
    math::v2f cameraInputRot = input::GetMouseDelta();
    cameraInputRot.x *= -1.f;
    RotateCamera(camera, cameraInputRot, TO_RAD(360.f) * 2.f, dt);
    MoveCamera(camera, cameraInputPos, 5.f, dt);

    //constantsTerrain.view = math::Transpose(math::LookAt(cameraPos, cameraTarget, {0,1,0}));
    //constantsTerrain.proj = math::Transpose(math::Perspective(TO_RAD(45.f), (f32)APP_W/(f32)APP_H, 0.1f, 100.f));
    constantsTerrain.view = math::Transpose(camera.GetView());
    constantsTerrain.proj = math::Transpose(camera.GetProjection());
}

void AppRender(i32 frame)
{
    // Frame setup
    Handle<render::CommandBuffer> hCmd = render::GetAvailableCommandBuffer(render::COMMAND_BUFFER_FRAME, frame);
    render::BeginFrame(frame);
    render::BeginCommandBuffer(hCmd);
    egui::BeginFrame();

    // Frame commands
    // Clear render output
    render::Barrier barrier = {};
    barrier.srcAccess = render::MEMORY_ACCESS_NONE;
    barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.srcStage = render::PIPELINE_STAGE_TOP;
    barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    render::CmdPipelineBarrierTextureLayout(hCmd, render::GetRenderPassOutput(hRenderPassMain, 0), render::IMAGE_LAYOUT_TRANSFER_DST, barrier);
    render::CmdClearColorTexture(hCmd, render::GetRenderPassOutput(hRenderPassMain, 0), 1, 0.5, 0.3, 1);
    barrier.srcAccess = render::MEMORY_ACCESS_TRANSFER_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_COLOR_OUTPUT_WRITE;
    barrier.srcStage = render::PIPELINE_STAGE_TRANSFER;
    barrier.dstStage = render::PIPELINE_STAGE_FRAGMENT_SHADER;
    render::CmdPipelineBarrierTextureLayout(hCmd, render::GetRenderPassOutput(hRenderPassMain, 0), render::IMAGE_LAYOUT_COLOR_OUTPUT, barrier);

    // Render terrain
    render::BeginRenderPass(hCmd, hRenderPassMain);
    render::CmdBindGraphicsPipeline(hCmd, hPipelineTerrain);
    render::CmdUpdatePushConstantRange(hCmd, 0, &constantsTerrain, hPipelineTerrain);
    render::CmdSetViewport(hCmd, hRenderPassMain);
    render::CmdSetScissor(hCmd, hRenderPassMain);
    render::CmdBindVertexBuffer(hCmd, hVbTerrain);
    render::CmdBindIndexBuffer(hCmd, hIbTerrain);
    render::CmdDrawIndexed(hCmd, hIbTerrain, 1);

    char testbuf[2048];
    //sprintf(testbuf, "pitch %.2f", camera.pitch);
    //egui::Text(IStr(testbuf));
    //sprintf(testbuf, "yaw %.2f", camera.yaw);
    //egui::Text(IStr(testbuf));
    sprintf(testbuf, "camera axisRight %.2f %.2f %.2f", camera.axisRight.x, camera.axisRight.y, camera.axisRight.z);
    egui::Text(IStr(testbuf));
    sprintf(testbuf, "camera axisUp %.2f %.2f %.2f", camera.axisUp.x, camera.axisUp.y, camera.axisUp.z);
    egui::Text(IStr(testbuf));
    sprintf(testbuf, "camera axisFront %.2f %.2f %.2f", camera.axisFront.x, camera.axisFront.y, camera.axisFront.z);
    egui::Text(IStr(testbuf));
    egui::DrawFrame(hCmd);
    render::EndRenderPass(hCmd, hRenderPassMain);

    // Copy to swap chain image and end frame
    barrier.srcAccess = render::MEMORY_ACCESS_COLOR_OUTPUT_WRITE;
    barrier.dstAccess = render::MEMORY_ACCESS_TRANSFER_READ;
    barrier.srcStage = render::PIPELINE_STAGE_COLOR_OUTPUT;
    barrier.dstStage = render::PIPELINE_STAGE_TRANSFER;
    render::CmdPipelineBarrierTextureLayout(hCmd, render::GetRenderPassOutput(hRenderPassMain, 0), render::IMAGE_LAYOUT_TRANSFER_SRC, barrier);
    render::CmdCopyToSwapChain(hCmd, render::GetRenderPassOutput(hRenderPassMain, 0));
    render::EndCommandBuffer(hCmd);
    render::EndFrame(frame, hCmd);

    // Present
    render::Present(frame);
}

};  // namespace Grass

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR pCmdLine, int nCmdShow)
{
    using namespace Grass;

    AppInit();

    while(window.state != render::WINDOW_CLOSED)
    {
        frameTimer.Start();
        AppUpdate();
        AppRender(frame);
        frameTimer.Stop();
        dt = (f32)frameTimer.GetElapsedS();
        frame++;
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
