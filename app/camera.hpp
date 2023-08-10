#pragma once
#include "engine/src/core/math.hpp"

namespace Grass
{
using namespace ty;

struct Camera
{
    // Variables
    math::v3f position = {0, 0, 0};
    math::v3f axisFront = {0, 0, 0};
    math::v3f axisRight = {0, 0, 0};
    math::v3f axisUp = {0, 0, 0};

    f32 pitch   = 0.f;  // Rad
    f32 yaw     = 0.f;  // Rad
    f32 fov     = 0.f;  // Rad
    f32 aspect  = 0.f;
    //TODO(caio): Near/far planes

    math::m4f GetView();
    math::m4f GetProjection();
};

Camera MakeCamera(math::v3f pos, math::v3f facing, f32 fov, f32 aspect);
void MoveCamera(Camera& cam, math::v3f moveInput, f32 speed, f32 dt);
void RotateCamera(Camera& cam, math::v2f rotateInput, f32 angularVelocity, f32 dt);

};
