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


	struct RenderResource {
		RenderTarget* target = nullptr;

		std::string name;
		bool external = false;
	};


	struct RenderData {
		std::vector<Model*> models;
		std::unordered_map<Model*, std::vector<UniformBufferObject>> instancedModels;
		std::vector<Light> lights;


		std::unordered_map<std::string, void*> FrameResouces;
	};



	class RenderPass {
	public:
		RenderPass() {}

		PassType type = None;
		std::string name;
		RenderInfo renderInfo;
		VkCommandPool CommandPool;

		// Resources this pass depends on
		std::vector<std::string> inputResources;
		std::vector<std::string> outputResources;
		std::unordered_map<std::string, VkDescriptorSet> BoundResources;


		void GenerateRenderPass();
		void CreatePipelines(std::vector<ShaderSet*> shaders);

		virtual void SetUpPass() = 0;
		virtual void Execute(VkCommandBuffer cmd, const RenderData& data, uint32_t currentFrame, uint32_t syncIndex) = 0;

		virtual ~RenderPass() = default;


		bool isOverlay = false;
		VkRenderPass renderPass = VK_NULL_HANDLE;
		Device* device;
		VkCommandPool* commandPool;
		PipelineConfig PassConfig;
	};
}