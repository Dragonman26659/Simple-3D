#pragma once
// ONLY THINGS THAT ARE NEEDED FOR EVERY FILE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#define GLM_ENABLE_EXPERIMENTAL 
 

// change definition based on if compiling for SDL or not
#ifndef DEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


// Changes depending on if we are using SDL2 or not
#ifdef SDL_WINDOW
#include "SDL.h"
#include <SDL_vulkan.h>
#else
#ifdef _GLFW_H_ 
#error "GLFW must not be included before Simple3D"
#endif
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#endif

#ifdef USEIMGUI
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#ifdef SDL_WINDOW
#include "backends/imgui_impl_sdl2.h"
#else
#include "backends/imgui_impl_glfw.h"
#endif

#endif // USEIMGUI


// 3rd party libs
#include "vulkan/vulkan.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>


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
#include <chrono>
#include <map>
#include <unordered_map>



// Common structs
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};


namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1) ^
                (hash<glm::vec3>()(vertex.normal) << 1);
        }
    };
}


// Transform Object
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;


    //uint32_t ModelID;
};


const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const int MAX_FRAMES_IN_FLIGHT = 2;


// Helper function for error messages
std::string vkResultToString(VkResult result);