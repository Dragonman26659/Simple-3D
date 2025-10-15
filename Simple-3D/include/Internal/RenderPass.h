#pragma once
#include "SimpleCore.h"

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"
#include "Internal/RenderTexture.h"


// Component
#include "Component/Renderable/Model.h" 
#include "Component/Tools/Material.h"
#include "Component/Tools/Lights.h"
#include "Component/Tools/Camera.h"



namespace Simple3D {
	enum PassType {
		Render,
		Raytrace,
		Enviroment,
		UI,
		PostProsess
	};


	// Target of a renderpass
	struct RenderTarget {
		SwapChain* swapchain = nullptr;
		RenderTexture* texture = nullptr;

		bool IsSwapchain() const { return swapchain != nullptr; }
		bool IsTexture()   const { return texture != nullptr; }

		bool IsValid() const {
			return (swapchain != nullptr) ^ (texture != nullptr);
		}

		VkImage GetVkImage(int currentFrame) const {
			if (texture) return texture->GetImage();
			if (swapchain) return swapchain->getImages()[currentFrame];
			return VK_NULL_HANDLE;
		}
	};


	struct RenderInfo {
		std::unordered_map<Material*, Pipeline*> materialPipelines;

		RenderTexture* inputTexture = nullptr;

		RenderTarget target;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> framebuffers;

		VkRenderPass renderPass = VK_NULL_HANDLE;
	};



	struct RenderData {
		std::vector<Model*> models;
		std::unordered_map<Model*, std::vector<UniformBufferObject>> instancedModels;
		std::vector<Light> lights;
	};



	class RenderPass {
	public:
		PassType type;
		std::string name;
		RenderInfo renderInfo;

		// Resources this pass depends on
		std::vector<std::string> inputResources;
		std::vector<std::string> outputResources;

		virtual void Execute(VkCommandBuffer cmd, RenderData data, uint32_t currentFrame) = 0;

		virtual ~RenderPass() = default;
	};
}