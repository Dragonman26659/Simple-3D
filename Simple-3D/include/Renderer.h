#pragma once
#include "Device.h"
#include "vulkan/vulkan.h"




// Standard
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

// Changes depending on if 
#ifdef SDL_WINDOW
#include "SDL.h"
#else
#include "GLFW/glfw3.h"
#endif




namespace Simple3D {
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};









	/*
	* Handles rendering
	* 
	* 
	* 
	*/
	class Renderer {
	public:
// Changes based on if you use SDL or GLFW for windowing
#ifdef SDL_WINDOW
		Renderer(SDL_Window* window, std::string EngineName, std::string ApplicationName) {
			// Get Information from window

			// Create instance
			CreateInstance(EngineName, ApplicationName);
		}
		
#else
		Renderer(GLFWwindow* window, std::string EngineName, std::string ApplicationName) {
			// Get Information from window
			WindowExtensions = glfwGetRequiredInstanceExtensions(&WindowExtensionCount);

			// Create instance
			CreateInstance(EngineName, ApplicationName);
			//setupDebugMessenger(); -- Validation Layers not supported :(


			printf("Finished Creating Instance");
		}
#endif



		~Renderer();


		void Render();


	private:
		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;

		// Information gathered from windows
		const char** WindowExtensions;
		uint32_t WindowExtensionCount = 0;

		void CreateInstance(std::string EngineName, std::string ApplicationName);
		//void setupDebugMessenger();





		std::vector<const char*> getRequiredExtensions();
		bool checkValidationLayerSupport();
	};
}