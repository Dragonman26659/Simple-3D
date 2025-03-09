#pragma once
#include "SimpleCore.h"

#include "Device.h"


namespace Simple3D {

	// Configures ALL pipelines together
	struct PipelineConfig {

	};


	class Pipeline {
	public:
		Pipeline(Device& s_Device, VkRenderPass& renderPass);
		~Pipeline();

	private:
		Device& s_Device;
		const std::string& fragment = "shaders/vert.spv";
		const std::string& vertex = "shaders/frag.spv";

		static std::vector<char> readFile(const std::string& filename);

		// Render Pass Reference
		VkRenderPass& renderPass;

		// Pipeline itself
		VkPipelineLayout pipelineLayout;
		VkPipeline graphicsPipeline; 

		void CreatePipeline();
		VkShaderModule createShaderModule(const std::vector<char>& code);
	};
}