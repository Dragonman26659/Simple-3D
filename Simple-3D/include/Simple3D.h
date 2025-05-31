#pragma once

#ifdef SDL_WINDOW
#else
#ifdef _GLFW_H_ 
#error "GLFW must not be included before Simple3D"
#endif
#define GLFW_INCLUDE_VULKAN
#endif

#include "Renderer.h"
#include "SimpleCore.h"
#include "Component/Tools/Material.h"