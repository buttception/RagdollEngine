#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
}
class ToneMapPass {
	struct ConstantBuffer {
		float Gamma;
		float Exposure;
		int32_t UseFixedExposure{ 0 };
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget{ nullptr };
	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::DeviceHandle NvrhiDeviceRef{ nullptr };

	nvrhi::TextureHandle SceneColor;

	nvrhi::ShaderHandle VertexShader;
	nvrhi::ShaderHandle PixelShader;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;
	nvrhi::BufferHandle ConstantBufferHandle;

public:
	void Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);
	void SetDependencies(nvrhi::TextureHandle sceneColor);

	void ToneMap(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle exposureHandle);
};