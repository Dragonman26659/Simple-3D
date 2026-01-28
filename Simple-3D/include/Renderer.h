#pragma once
#include "SimpleCore.h"


// Simple 3D

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

// RenderPasses
#include "Internal/RenderPasses/GeometryPass.h"





namespace Simple3D {
	class Renderer {
	public:
		// Changes based on if you use SDL or GLFW for windowing
#ifdef SDL_WINDOW
		Renderer(SDL_Window* window, std::string EngineName, std::string ApplicationName, bool useValidation = true)
			: window(window), VALIDATION_LAYERS_ENABLED(useValidation) {
			// Get Information from window
			int width, height;
			SDL_GetWindowSize(window, &width, &height);
			// Get required extensions from SDL

			// Step 1: Query the number of extensions needed
			if (!SDL_Vulkan_GetInstanceExtensions(window, &WindowExtensionCount, nullptr)) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
					"Failed to get Vulkan extension count: %s",
					SDL_GetError());
				throw std::runtime_error("Failed to get Vulkan extension count");
			}

			// Step 2: Allocate array and get extensions
			WindowExtensions = new const char* [WindowExtensionCount];
			if (!SDL_Vulkan_GetInstanceExtensions(window, &WindowExtensionCount, WindowExtensions)) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
					"Failed to get Vulkan extensions: %s",
					SDL_GetError());
				delete[] WindowExtensions;
				throw std::runtime_error("Failed to get Vulkan extensions");
			}

			// Create Instance
			CreateInstance(EngineName, ApplicationName);

			// Create Window Surface
			if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
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

			// Create frame buffers
			createFramebuffers();

			createCommandBuffer();

			// Create sync objects
			createSyncObjects();

			lastWidth = width;
			lastHeight = height;
		}

		int GetWindowWidth() {
			int width;
			SDL_GetWindowSize(window, &width, nullptr);
			return width;
		}

		int GetWindowHeight() {
			int height;
			SDL_GetWindowSize(window, nullptr, &height);
			return height;
		}

		bool isMinimised() {
			return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;
		}

	private:
		SDL_Window* window;
	public:
#else // ---------- GLFW WINDOWING ----------

		Renderer(GLFWwindow* window, std::string EngineName, std::string ApplicationName)
			: window(window)
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			WindowExtensions = glfwGetRequiredInstanceExtensions(&WindowExtensionCount);

			CreateInstance(EngineName, ApplicationName);

			if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create window surface!");
			}

			RenderDevice = new Device(instance, surface);
			swapChain = new SwapChain(*RenderDevice, surface, width, height);

			CreateRenderPass();
			createCommandPool();
			depthBuffer = new DepthBuffer(RenderDevice, swapChain, &commandPool);
			createFramebuffers();
			createCommandBuffer();
			createSyncObjects();

			lastWidth = width;
			lastHeight = height;
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

		bool isMinimised() {
			return glfwGetWindowAttrib(window, GLFW_ICONIFIED);
		}

		bool WindowResized() {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);
			if (width != lastWidth || height != lastHeight) {
				lastWidth = width;
				lastHeight = height;
				return true;
			}
			return false;
		}

	private:
		GLFWwindow* window;
	public:
#endif



		~Renderer();


		void Render(RenderData& data);
		void WaitToFinish();
		void WindoResize();

		// In header due to links with preprossessor
		void RecreateSwapChain();

		Material* CreateMaterial(MaterialInfo info);
		ShaderSet* CreateShaderSet(std::string name);


		RenderGraph* CreateRenderGraph(std::string name);
		void BuildRenderGraphs();
		void BuildGraph(RenderGraph* graph);

		TextureBinding CreateTexture(std::string filepath);
		TextureCube* CreateTextureCube(std::vector<std::string> filepath);

		// Dosent expose vulkan directly but alows for renderTargets to be made
		SwapChain* GetSwapChain() { return swapChain; }
		Device* GetDevice() { return RenderDevice; }

		RenderTexture* CreateRenderTexture(int width, int height);
		RenderTexture* CreateRenderTexture(int width, int height, VkFormat format);
		DepthBuffer* CreateDepth(RenderTarget target);
		DepthBuffer* CreateDepth(VkExtent2D extent);

		bool VALIDATION_LAYERS_ENABLED = true;

#ifdef USEIMGUI
		// Returns all needed information to be able to use Imgui
		ImGui_ImplVulkan_InitInfo GetImGUIinfo() {
			ImGui_ImplVulkan_InitInfo info = {};

			// Create separate pool for ImGui
			VkDescriptorPoolSize pool_sizes[] = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};

			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.maxSets = 1000;
			pool_info.poolSizeCount = std::size(pool_sizes);
			pool_info.pPoolSizes = pool_sizes;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

			vkCreateDescriptorPool(RenderDevice->getLogicalDevice(), &pool_info, nullptr, &imguiPool);

			info.Instance = instance;
			info.PhysicalDevice = RenderDevice->getPhysicalDevice();
			info.Device = RenderDevice->getLogicalDevice();
			info.QueueFamily = RenderDevice->findQueueFamilies().graphicsFamily.value();
			info.Queue = RenderDevice->getVKgraphicsQueue();
			info.DescriptorPool = imguiPool;
			info.MinImageCount = swapChain->getImageViews().size();
			info.ImageCount = info.MinImageCount;
			info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
			info.RenderPass = renderPass;

			info.PipelineRenderingCreateInfo = {};
			info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
			info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
			info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChain->GetSwapChainImageFormat();

			return info;
		}

		bool NewImguiframe() {
			if (isMinimised())
				return false;


			// Begin new frame
			ImGui_ImplVulkan_NewFrame();

			// Platform newFrame
#ifdef SDL_WINDOW
			ImGui_ImplSDL2_NewFrame();
#else
			ImGui_ImplGlfw_NewFrame();
#endif // SDL_WINDOW

			ImGui::NewFrame();

			return true;
		}



		void drawImgui(VkCommandBuffer cmd, uint32_t imageIndex) {
			if (!usingImgui)
				return;

			// 1. Early validation
			if (cmd == VK_NULL_HANDLE) {
				// Log error: Invalid command buffer
				return;
			}

			if (ImGui::GetCurrentContext() == nullptr) {
				// Log error: Context not initialized
				return;
			}

			ImDrawData* draw_data = ImGui::GetDrawData();
			if (draw_data == nullptr) {
				// Log warning: No draw data to render
				return;
			}

			try {
				// 7. Render ImGui
				ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
			}
			catch (...) {
				return;
			}
		}

		void initImgui(std::function<void(ImGuiStyle&)> styleConfig = nullptr) {
			// 1. Create ImGui context
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			// 2. Configure ImGui IO settings
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

			// 3. Initialize platform backend
#ifdef SDL_WINDOW
			ImGui_ImplSDL2_InitForVulkan(window);
#else
			ImGui_ImplGlfw_InitForVulkan(window);
#endif

			// 4. Initialize Vulkan backend
			ImGui_ImplVulkan_InitInfo init_info = GetImGUIinfo();
			init_info.PipelineCache = nullptr;
			init_info.Allocator = nullptr;
			init_info.MinImageCount = swapChain->getImageViews().size();
			init_info.ImageCount = init_info.MinImageCount;
			init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

			// Apply custom style configuration if provided
			if (styleConfig != nullptr) {
				styleConfig(ImGui::GetStyle());
			}
			else {
				// Default style configuration
				ImGui::StyleColorsDark();
			}

			// Initialize Vulkan backend
			ImGui_ImplVulkan_Init(&init_info);

			usingImgui = true;
		}


		bool usingImgui = false;

	private:
		// DescriptorPool for Imgui
		VkDescriptorPool imguiPool;
	public:

#endif // USEIMGUI

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

		// Render passes
		VkRenderPass renderPass;
		VkRenderPass ClearRenderPass;

		// Framebuffer
		std::vector<VkFramebuffer> swapChainFramebuffers;

		// Device and swapchain(ik useful comment)
		Device* RenderDevice;
		SwapChain* swapChain;




		// Information gathered from windows
		const char** WindowExtensions;
		uint32_t WindowExtensionCount = 0;

		// Store the current frame we are on
		uint32_t currentFrame = 0;

		// Models and lights
		std::vector<RenderGraph*> RenderGraphs;
		std::vector<RenderTexture*> RenderTextures;
		std::vector<ShaderSet*> shaders;

		// Create instance
		void CreateInstance(std::string EngineName, std::string ApplicationName);

		// Create command pool and command buffer
		void createCommandPool();
		void createCommandBuffer();

		// Command buffer recording -- In header due to references to preprossessor
		void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, RenderData& data);

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

		int lastWidth = 0;
		int lastHeight = 0;

		bool WindowResized() {
			int currentWidth = 0, currentHeight = 0;

#ifdef SDL_WINDOW
			SDL_GetWindowSize(window, &currentWidth, &currentHeight);
#else
			glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
#endif

			// Check if the size actually changed
			if (currentWidth != lastWidth || currentHeight != lastHeight) {
				lastWidth = currentWidth;
				lastHeight = currentHeight;
				return true;
			}

			return false;
		}
	};
}