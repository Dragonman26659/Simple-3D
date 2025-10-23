#pragma once
#include "SimpleCore.h"

// Internal
#include "Internal/Device.h"
#include "Internal/SwapChain.h"
#include "Internal/Pipeline.h"
#include "Internal/DepthBuffer.h"
#include "Internal/RenderTexture.h"
#include "Internal/Material/ShaderSet.h"
#include "Internal/RenderTarget.h"


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
		PostProsess,
		None
	};


	


	struct RenderInfo {
		std::unordered_map<ShaderSet*, Pipeline*> Pipelines;

		RenderTexture* inputTexture = nullptr;

		RenderTarget target;

		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> framebuffers;


		VkFramebuffer GetFrameBuffer(uint32_t imageIndex) {
			if (target.swapchain) {
				return framebuffers[imageIndex];
			}
			return framebuffer;
		}
	};



	struct RenderData {
		std::vector<Model*> models;
		std::unordered_map<Model*, std::vector<UniformBufferObject>> instancedModels;
		std::vector<Light> lights;
	};



	class RenderPass {
	public:
		PassType type = None;
		std::string name;
		RenderInfo renderInfo;

		// Resources this pass depends on
		std::vector<std::string> inputResources;
		std::vector<std::string> outputResources;


		void GenerateRenderPass();
		void CreatePipelines(std::vector<ShaderSet*> shaders);
		virtual void Execute(VkCommandBuffer cmd, const RenderData& data, uint32_t currentFrame) = 0;

		virtual ~RenderPass() = default;

		VkRenderPass renderPass = VK_NULL_HANDLE;
		Device* device;
		VkCommandPool* commandPool;
	};
}