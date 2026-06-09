#pragma once
#include "SimpleCore.h"

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"
#include "Internal/RenderTexture.h"
#include "Internal/RenderGraph.h"
#include "Internal/Allocator.h"

// Ray tracing
#include "Internal/RTcontext.h"    // RayTracingContext, BLAS, TLAS, TLASInstance
#include "Internal/RT/RTModel.h"   // BuildBLAS, RefitBLAS, DestroyBLAS

// Components
#include "Component/Renderable/Model.h"
#include "Component/Tools/Camera.h"
#include "Component/Tools/Lights.h"
#include "Internal/Tex3D.h"
#include "Internal/JobSystem.h"

// RenderPasses
#include "Internal/RenderPasses/GeometryPass.h"

namespace Simple3D {

    class Renderer {
    public:

        // ── Platform-specific constructor (SDL) ──────────────────────────────────────
#ifdef SDL_WINDOW
        Renderer(SDL_Window* window,
            std::string EngineName,
            std::string ApplicationName,
            bool        useValidation = true)
            : window(window), VALIDATION_LAYERS_ENABLED(useValidation)
        {
            int width, height;
            SDL_GetWindowSize(window, &width, &height);

            if (!SDL_Vulkan_GetInstanceExtensions(window, &WindowExtensionCount, nullptr))
                throw std::runtime_error("Failed to get Vulkan extension count");

            WindowExtensions = new const char* [WindowExtensionCount];
            if (!SDL_Vulkan_GetInstanceExtensions(window, &WindowExtensionCount, WindowExtensions))
            {
                delete[] WindowExtensions;
                throw std::runtime_error("Failed to get Vulkan extensions");
            }

            CreateInstance(EngineName, ApplicationName);

            if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
                throw std::runtime_error("failed to create window surface!");

            RenderDevice = new Device(instance, surface);
            swapChain = new SwapChain(*RenderDevice, surface, width, height);

            Allocator::Init(instance,
                RenderDevice->getPhysicalDevice(),
                RenderDevice->getLogicalDevice());

            CreateRenderPass();
            createCommandPool();
            createFramebuffers();
            createCommandBuffer();
            createSyncObjects();

            lastWidth = width;
            lastHeight = height;
        }

        int  GetWindowWidth() { int w; SDL_GetWindowSize(window, &w, nullptr);  return w; }
        int  GetWindowHeight() { int h; SDL_GetWindowSize(window, nullptr, &h);  return h; }
        bool isMinimised() { return (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0; }

    private:
        SDL_Window* window;
    public:

        // ── Platform-specific constructor (GLFW) ─────────────────────────────────────
#else
        Renderer(GLFWwindow* window,
            std::string EngineName,
            std::string ApplicationName)
            : window(window)
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            WindowExtensions = glfwGetRequiredInstanceExtensions(&WindowExtensionCount);

            CreateInstance(EngineName, ApplicationName);

            if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
                throw std::runtime_error("Failed to create window surface!");

            RenderDevice = new Device(instance, surface);
            swapChain = new SwapChain(*RenderDevice, surface, width, height);

            Allocator::Init(instance,
                RenderDevice->getPhysicalDevice(),
                RenderDevice->getLogicalDevice());

            CreateRenderPass();
            createCommandPool();
            depthBuffer = new DepthBuffer(RenderDevice, swapChain, &commandPool);
            createFramebuffers();
            createCommandBuffer();
            createSyncObjects();

            lastWidth = width;
            lastHeight = height;
        }

        int  GetWindowWidth() { int w; glfwGetFramebufferSize(window, &w, nullptr); return w; }
        int  GetWindowHeight() { int h; glfwGetFramebufferSize(window, nullptr, &h); return h; }
        bool isMinimised() { return glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0; }

        bool WindowResized()
        {
            int cw, ch;
            glfwGetFramebufferSize(window, &cw, &ch);
            if (cw != lastWidth || ch != lastHeight) { lastWidth = cw; lastHeight = ch; return true; }
            return false;
        }

    private:
        GLFWwindow* window;
    public:
#endif

        ~Renderer();

        // ── Frame ─────────────────────────────────────────────────────────────────
        void Render(RenderData& data);
        void WaitToFinish();
        void WindoResize();
        void RecreateSwapChain();

        // ── Resources ─────────────────────────────────────────────────────────────
        Material* CreateMaterial(MaterialInfo info);
        ShaderSet* CreateShaderSet(std::string name);
        RenderGraph* CreateRenderGraph(std::string name);
        void           BuildRenderGraphs();
        void           BuildGraph(RenderGraph* graph);
        TextureBinding CreateTexture(std::string filepath);
        TextureCube* CreateTextureCube(std::vector<std::string> filepath);
        RenderTexture* CreateRenderTexture(int width, int height);
        RenderTexture* CreateRenderTexture(int width, int height, VkFormat format);
        DepthBuffer* CreateDepth(RenderTarget target);
        DepthBuffer* CreateDepth(VkExtent2D extent);

        void RegisterJobSystem(IJobSystem* js) { jobSystem = js; }

        // ── Accessors ─────────────────────────────────────────────────────────────
        SwapChain* GetSwapChain() { return swapChain; }
        Device* GetDevice() { return RenderDevice; }
        VkCommandPool* GetCommandPool() { return &commandPool; }

        // Returns the raw VmaAllocator so cross-DLL callers can pass it to
        // Allocator::SetInstance() and share the same allocator instance.
        VmaAllocator   GetVmaAllocator() { return Allocator::GetRawAllocator(); }

        // ── Ray Tracing ───────────────────────────────────────────────────────────

        // Call once after construction if you want RT support.
        // Returns true if the hardware and driver support VK_KHR_acceleration_structure
        // + VK_KHR_ray_query.  If false, all RT calls are safely no-ops.
        bool EnableRayTracing();

        // Returns false if EnableRayTracing() was never called or hardware lacks support.
        bool IsRayTracingAvailable() const { return m_RTCtx.IsAvailable(); }

        // Access the context for advanced use (e.g. querying feature flags).
        const RayTracingContext& GetRTContext() const { return m_RTCtx; }
        RayTracingContext& GetRTContext() { return m_RTCtx; }

        // Build a BLAS for a model.  The model's buffers must already be created.
        // allowUpdate=true adds ALLOW_UPDATE so the BLAS can be refitted later.
        // Returns an invalid (default) BLAS if RT is unavailable.
        BLAS BuildModelBLAS(Model& model, bool allowUpdate = false);

        // Update a BLAS in-place after vertex positions change (dynamic meshes).
        // The BLAS must have been built with allowUpdate=true.
        void RefitModelBLAS(BLAS& blas, Model& model);

        // Destroy a BLAS and free its backing buffer.
        void DestroyModelBLAS(BLAS& blas);

        // Build / rebuild a TLAS from a list of instances.
        // If tlas.IsValid() the previous TLAS is destroyed first.
        void BuildSceneTLAS(TLAS& tlas, const std::vector<TLASInstance>& instances);

        // Destroy a TLAS.
        void DestroySceneTLAS(TLAS& tlas);

        // ── Validation ────────────────────────────────────────────────────────────
        bool VALIDATION_LAYERS_ENABLED = true;

        // ── ImGui (optional) ─────────────────────────────────────────────────────────
#ifdef USEIMGUI
        ImGui_ImplVulkan_InitInfo GetImGUIinfo();
        bool          NewImguiframe();
        void          drawImgui(VkCommandBuffer cmd, uint32_t imageIndex);
        void          initImgui(std::function<void(ImGuiStyle&)> styleConfig = nullptr);
        bool          usingImgui = false;
    private:
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    public:
#endif

    private:
        // ── Vulkan core ───────────────────────────────────────────────────────────
        VkInstance               instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR             surface = VK_NULL_HANDLE;

        VkCommandPool                commandPool;
        std::vector<VkCommandBuffer> commandBuffers;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence>     inFlightFences;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPass ClearRenderPass = VK_NULL_HANDLE;

        std::vector<VkFramebuffer> swapChainFramebuffers;

        Device* RenderDevice = nullptr;
        SwapChain* swapChain = nullptr;
        DepthBuffer* depthBuffer = nullptr;   // GLFW path only

        IJobSystem* jobSystem = nullptr;

        const char** WindowExtensions = nullptr;
        uint32_t     WindowExtensionCount = 0;
        uint32_t     currentFrame = 0;
        int          lastWidth = 0;
        int          lastHeight = 0;

        std::vector<RenderGraph*>    RenderGraphs;
        std::vector<RenderTexture*>  RenderTextures;
        std::vector<ShaderSet*>      shaders;

        // ── Ray Tracing ───────────────────────────────────────────────────────────
        RayTracingContext m_RTCtx;
        bool              m_RTEnabled = false;

        // ── Private helpers ───────────────────────────────────────────────────────
        void CreateInstance(std::string EngineName, std::string ApplicationName);
        void createCommandPool();
        void createCommandBuffer();
        void recordCommandBuffer(VkCommandBuffer cmd,
            uint32_t        syncIndex,
            uint32_t        imageIndex,
            RenderData& data);
        void createSyncObjects();
        void setupDebugMessenger();
        void DestroyDebugUtilsMessengerEXT(VkInstance                   instance,
            VkDebugUtilsMessengerEXT     debugMessenger,
            const VkAllocationCallbacks* pAllocator);
        std::vector<const char*> getRequiredExtensions();
        bool checkValidationLayerSupport();
        void CreateRenderPass();
        void createFramebuffers();
        void cleanupSwapChain();

        bool WindowResized()
        {
            int cw = 0, ch = 0;
#ifdef SDL_WINDOW
            SDL_GetWindowSize(window, &cw, &ch);
#else
            glfwGetFramebufferSize(window, &cw, &ch);
#endif
            if (cw != lastWidth || ch != lastHeight) { lastWidth = cw; lastHeight = ch; return true; }
            return false;
        }
    };

} // namespace Simple3D