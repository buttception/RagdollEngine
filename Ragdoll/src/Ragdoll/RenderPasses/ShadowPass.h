#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class ShadowPass {
	struct ConstantBuffer {
		Matrix LightViewProj;
		uint32_t InstanceOffset;
		uint32_t CascadeIndex;
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget[4]{ nullptr, };
	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::DeviceHandle NvrhiDeviceRef{ nullptr };

	nvrhi::ShaderHandle VertexShader;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;
	nvrhi::BufferHandle ConstantBufferHandle;

public:
	void Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget[4]);

	void DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, const ragdoll::SceneInformation& sceneInfo);
};