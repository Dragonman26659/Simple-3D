#pragma once
#include "SimpleCore.h"


// Simple 3D

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"


// Components
#include "Component/Renderable/Model.h"
#include "Component/Tools/Camera.h"


namespace Simple3D {
	class Renderer {
	public:
// Changes based on if you use SDL or GLFW for windowing
#ifdef SDL_WINDOW
		Renderer(SDL_Window* window, std::string EngineName, std::string ApplicationName) 
			: window(window) {

		}


		int GetWindowWidth() {
			return 1;
		}

		int GetWindowHeight() {
			return 1;
		}

		bool isMinimised() {
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

			// Create Command Buffers
			createCommandPool();

			// Create depth buffer
			depthBuffer = new DepthBuffer(RenderDevice, swapChain, &commandPool);

			// Create frame buffers
			createFramebuffers();

			createCommandBuffer();

			// Create sync objects
			createSyncObjects();
		}



		void InitaliseRenderer();


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

		bool isMinimised() {
			return glfwGetWindowAttrib(window, GLFW_ICONIFIED);
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

		Material* CreateMaterial(MaterialInfo info);
		void SumbitModelToFrame(Model* model);
		void SubmitMainCamera(Camera* cam);

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


		// Store one pipeline per material
		std::unordered_map<Material*, Pipeline*> materials;


		// DepthBuffer
		DepthBuffer* depthBuffer;


		// Camera
		Camera* mainCamera;

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