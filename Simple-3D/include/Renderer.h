#pragma once
#include "SimpleCore.h"


// Simple 3D

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"


// Components
#include "Component/Renderable/Model.h"


// Test for now (will replace with model class later on)
const std::vector<Vertex> TriangleVertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> TriangleIndices = {
	0, 1, 2, 2, 3, 0
};

namespace Simple3D {
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};


	const int MAX_FRAMES_IN_FLIGHT = 2;


	class Renderer {
	public:
// Changes based on if you use SDL or GLFW for windowing
#ifdef SDL_WINDOW
		Renderer(SDL_Window* window, std::string EngineName, std::string ApplicationName) 
			: window(window) {
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
			pipeline = new Pipeline(*RenderDevice, renderPass);

			// Create frame buffers
			createFramebuffers();


			// Create Command Buffers
			createCommandPool();
			createCommandBuffer();

			// Create sync objects
			createSyncObjects();
		}


		int GetWindowWidth() {
			return 1;
		}

		int GetWindowHeight() {
			return 1;
		}
		
	private:
		SDL_Window* window;
	public:

#else
		// GLFW specific Constructor
		Renderer(GLFWwindow* window, std::string EngineName, std::string ApplicationName) 
			: window(window) {
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

			// Create frame buffers
			createFramebuffers();


			// Create Command Buffers
			createCommandPool();
			createCommandBuffer();

			// Create sync objects
			createSyncObjects();
		}


		int GetWindowWidth() {
			int width;
			glfwGetFramebufferSize(window, &width, nullptr);
			return width;
		}

		int GetWindowHeight() {
			int height;
			glfwGetFramebufferSize(window, nullptr, &height);
			return height;
		}


		// GLFW window
		private:
		GLFWwindow* window;
		public:
#endif



		~Renderer();


		void Render();
		void WaitToFinish();
		void RecreateSwapChain();
		void SumbitModelToFrame(Model* model);


	private:
		// Vulkan Instance
		VkInstance instance;

		// Debug messenger (for validation layers)
		VkDebugUtilsMessengerEXT debugMessenger;

		// Vulkan surface
		VkSurfaceKHR surface;

		// Command pool + buffer
		VkCommandPool commandPool;
		std::vector<VkCommandBuffer> commandBuffers;


		// Vulkan sync objects
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;


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

		// Store the current frame we are on
		uint32_t currentFrame = 0;

		// Models
		std::vector<Model*> ModelsThisFrame;

		// Create instance
		void CreateInstance(std::string EngineName, std::string ApplicationName);


		// Create command pool and command buffer
		void createCommandPool();
		void createCommandBuffer();
		void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);


		// Sync objects
		void createSyncObjects();

		// Debug info
		void setupDebugMessenger();
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

		// Initalise Vulkan
		std::vector<const char*> getRequiredExtensions();
		bool checkValidationLayerSupport();


		// Parts that are not large enough for there own class but probably should be
		void CreateRenderPass();
		void createFramebuffers();

		// Clean up all objects related to swapchain
		void cleanupSwapChain();
	};
}