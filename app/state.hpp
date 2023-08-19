#pragma once
#include "engine/src/core/memory.hpp"
#include "engine/src/core/time.hpp"

#include "app/camera.hpp"

#define SHADER_PATH "app/shaders/"
#define MODEL_PATH "resources/models/"
#define IMAGE_PATH "resources/textures/"

namespace ty
{
namespace Grass
{

const i32 appWidth = 1920;
const i32 appHeight = 1080;

inline mem::HeapAllocator appHeap = {};

inline Camera appCamera = {};

inline i32 currentFrame = 0;
inline time::Timer worldTimer = {};
inline f32 worldTime = 0;
inline time::Timer frameTimer = {};
inline f32 deltaTime = 0;

void InitState();
void AdvanceState();

};  // namespace Grass
};  // namespace ty
