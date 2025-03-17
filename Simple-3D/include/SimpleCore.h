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

// 3rd party libs
#include "vulkan/vulkan.h"
#include <glm/glm.hpp>


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
#include <array>


// Common structs
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;


        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Texture

        return attributeDescriptions;
    }
};



// Helper function for error messages
std::string vkResultToString(VkResult result);