#pragma once

#ifdef SDL_WINDOW
#else
#ifdef _GLFW_H_ 
#error "GLFW must not be included before Simple3D"
#endif
#define GLFW_INCLUDE_VULKAN
#endif

#include "SimpleCore.h"
#include "Renderer.h"

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"
#include "Internal/RenderTexture.h"
#include "Internal/RenderGraph.h"


// Components
#include "Component/Renderable/Model.h"
#include "Component/Tools/Camera.h"
#include "Component/Tools/Lights.h"
#include "Internal/Tex3D.h"