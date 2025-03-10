#pragma once
#include "SimpleCore.h"


// Simple 3D
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"


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
			SDL_SysWMinfo wmi;
			SDL_VERSION(&wmi.version);
			SDL_GetWindowWMInfo(window, &wmi);
			WindowExtensions = SDL_Vulkan_GetInstanceExtensions(&WindowExtensionCount);

			// Get window dimensions
			int width, height;
			SDL_GetWindowSize(window, &width, &height);

			// Create Instance
			CreateInstance(EngineName, ApplicationName);

			// Create Window Surface
			VkResult result = vkCreateWin32SurfaceKHR(instance, &wmi.info.win, nullptr, &surface);
			if (result != VK_SUCCESS) {
				printf("failed to create window surface!");
				throw std::runtime_error("failed to create window surface!");
			}

			// Create device & swapchain
			RenderDevice = new Device(instance, surface);
			swapChain = new SwapChain(*RenderDevice, surface, width, height);

			// Create render pass
			CreateRenderPass();

			// Create pipelines
			pipeline = new Pipeline(*RenderDevice);
	}
		
#else
		Renderer(GLFWwindow* window, std::string EngineName, std::string ApplicationName) {
			// Get Information from window
			int width, height;

			WindowExtensions = glfwGetRequiredInstanceExtensions(&WindowExtensionCount);
			glfwGetFramebufferSize(window, &width, &height);

			// Create Instance
			CreateInstance(EngineName, ApplicationName);

			// Create Window Surface
			if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) { 
				printf("failed to create window surface!");
				throw std::runtime_error("failed to create window surface!"); 
			}

			// Create device & swapchain
			RenderDevice = new Device(instance, surface);
			swapChain = new SwapChain(*RenderDevice, surface, width, height);

			// Create render pass
			CreateRenderPass();

			// Create pipelines
			pipeline = new Pipeline(*RenderDevice, renderPass);
		}
#endif



		~Renderer();


		void Render();


	private:
		// Vulkan Instance
		VkInstance instance;

		// Debug messenger (for validation layers)
		VkDebugUtilsMessengerEXT debugMessenger;

		// Vulkan surface
		VkSurfaceKHR surface;

		// Command pool + buffer
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;


		// Parts that are not large enough for there own class but probably should be

		// Render pass
		VkRenderPass renderPass;

		// Framebuffer
		std::vector<VkFramebuffer> swapChainFramebuffers;


		// Device, swapchain and pipeline (ik useful comment)
		Device* RenderDevice;
		SwapChain* swapChain;
		Pipeline* pipeline;

		// Information gathered from windows
		const char** WindowExtensions;
		uint32_t WindowExtensionCount = 0;




		// Create instance
		void CreateInstance(std::string EngineName, std::string ApplicationName);


		// Create command pool and command buffer
		void createCommandPool();
		void createCommandBuffer();
		void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

		// Debug info
		bool useValidationLayers = enableValidationLayers;
		void setupDebugMessenger();
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

		// Initalise Vulkan
		std::vector<const char*> getRequiredExtensions();
		bool checkValidationLayerSupport();


		// Parts that are not large enough for there own class but probably should be
		void CreateRenderPass();
		void createFramebuffers();
	};
}