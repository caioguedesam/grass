#include "app/camera.hpp"
#include "engine/src/core/math.hpp"

namespace ty
{
namespace Grass
{

Camera MakeCamera(math::v3f pos, math::v3f facing, f32 fov, f32 aspect)
{
    math::v3f cameraZ = math::Normalize(facing);
    math::v3f cameraX = math::Normalize(math::Cross({0, 1, 0}, cameraZ));
    math::v3f cameraY = math::Normalize(math::Cross(cameraZ, cameraX));
    Camera result = 
    {
        .position = pos,
        .axisFront = cameraZ,
        .axisRight = cameraX,
        .axisUp = cameraY,
        .fov = fov,
        .aspect = aspect,
    };
    return result;
}

math::m4f Camera::GetView()
{
    return math::LookAtLH(position, position + axisFront, axisUp);
}

math::m4f Camera::GetProjection()
{
    //TODO(caio): Variable zNear and zFar
    return math::PerspectiveLH(fov, aspect, 0.1f, 1000.f);
}

void MoveCamera(Camera &cam, math::v3f moveInput, f32 speed, f32 dt)
{
    // Moves camera on local xyz axis
    // TODO(caio): Global axis movement?
    cam.position = cam.position + (cam.axisRight * moveInput.x * speed * dt);
    cam.position = cam.position + (cam.axisUp * moveInput.y * speed * dt);
    cam.position = cam.position + (cam.axisFront * moveInput.z * speed * dt);
}

void RotateCamera(Camera &cam, math::v2f rotateInput, f32 angularVelocity, f32 dt)
{
    using namespace math;

    m4f rotationY = RotationMatrix(rotateInput.x * angularVelocity * dt, cam.axisUp);
    m4f rotationX = RotationMatrix(rotateInput.y * angularVelocity * dt, cam.axisRight);
    // First rotate on Y axis
    cam.axisFront = TransformDirection(cam.axisFront, rotationY);
    // Then rotate on X axis
    cam.axisFront = TransformDirection(cam.axisFront, rotationX);

    // Then reconstruct camera coordinate space
    cam.axisRight = Normalize(Cross({0,1,0}, cam.axisFront));
    cam.axisUp = Normalize(Cross(cam.axisFront, cam.axisRight));
}

};  // namespace Grass
};  // namespace ty
