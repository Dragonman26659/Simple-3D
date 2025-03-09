#pragma once
// ONLY THINGS THAT ARE NEEDED FOR EVERY FILE



#ifndef DEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


// Changes depending on if we are using SDL2 or not
#ifdef SDL_WINDOW
#include "SDL.h"
#else
#ifdef _GLFW_H_ 
#error "GLFW must not be included before Simple3D"
#endif

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#endif

#include "vulkan/vulkan.h"


// Standard
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <set>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>

// Helper function for error messages
std::string vkResultToString(VkResult result);