#pragma once
#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class DebugPass {
	struct ConstantBuffer {
		//constant buffer struct specific to the forward pass
		Matrix ViewProj;
		Vector4 LightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
		Vector4 SceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
		Vector3 LightDirection = { 1.f, -1.f, 1.f };
		int InstanceOffset{ 0 };
		Vector3 CameraPosition;
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget{ nullptr };
	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::DeviceHandle NvrhiDeviceRef{ nullptr };

	nvrhi::ShaderHandle ForwardVertexShader;
	nvrhi::ShaderHandle ForwardPixelShader;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;
	nvrhi::BufferHandle ConstantBufferHandle;

public:
	void Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);

	void DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, uint32_t instanceCount, const ragdoll::SceneInformation& sceneInfo);
};