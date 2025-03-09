#pragma once
#include "SimpleCore.h"


// Simple 3D
#include "Internal/Device.h"
#include "Internal/SwapChain.h"



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

			// Create VkSurfaceKHR
			if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) { 
				printf("failed to create window surface!");
				throw std::runtime_error("failed to create window surface!"); 
			}

			// Create device
			RenderDevice = new Device(instance, surface);

			// Create Swapchain
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			swapChain = new SwapChain(*RenderDevice, surface, width, height);



			printf("Renderer Generated");
		}
#endif



		~Renderer();


		void Render();


	private:
		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;

		// Classes
		Device* RenderDevice;
		SwapChain* swapChain;
		VkSurfaceKHR surface;

		// Information gathered from windows
		const char** WindowExtensions;
		uint32_t WindowExtensionCount = 0;

		void CreateInstance(std::string EngineName, std::string ApplicationName);
		//void setupDebugMessenger();




		std::vector<const char*> getRequiredExtensions();
		bool checkValidationLayerSupport();
	};
}