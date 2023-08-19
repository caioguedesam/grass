#include "app/state.hpp"
#include "app/camera.hpp"
#include "engine/src/core/memory.hpp"

namespace ty
{
namespace Grass
{

void InitState()
{
    appHeap = mem::MakeHeapAllocator(GB(1));

    math::v3f initialCameraPos = {-5, 24, -5};
    math::v3f initialCameraTarget = {256, 0, 256};
    f32 cameraFov = TO_RAD(60.f);
    f32 cameraAspect = (f32)appWidth/(f32)appHeight;
    appCamera = MakeCamera(initialCameraPos, initialCameraTarget, cameraFov, cameraAspect);

    worldTimer.Start();
    frameTimer.Start();
}

void AdvanceState()
{
    worldTimer.Stop();
    frameTimer.Stop();
    worldTime = worldTimer.GetElapsedS();
    deltaTime = frameTimer.GetElapsedS();
    currentFrame += 1;
    frameTimer.Start();
}

};  // namespace Grass
};  // namespace ty
