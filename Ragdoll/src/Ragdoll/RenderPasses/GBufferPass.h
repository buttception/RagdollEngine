#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class GBufferPass {
	struct ConstantBuffer {
		//constant buffer struct specific to the forward pass
		Matrix ViewProj;
		int InstanceOffset{ 0 };
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget{ nullptr };
	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::DeviceHandle NvrhiDeviceRef{ nullptr };

	nvrhi::ShaderHandle VertexShader;
	nvrhi::ShaderHandle PixelShader;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::BufferHandle ConstantBufferHandle;
	nvrhi::GraphicsPipelineDesc PipelineDesc;

public:
	void Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);

	void DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, const ragdoll::SceneInformation& sceneInfo);
};