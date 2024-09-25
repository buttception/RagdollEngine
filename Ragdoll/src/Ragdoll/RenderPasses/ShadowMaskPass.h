#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class ShadowMaskPass {
	struct ConstantBuffer {
		Matrix LightViewProj[4];
		Matrix InvViewProj;
		Matrix View;
		int EnableCascadeDebug;
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget;
	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::DeviceHandle NvrhiDeviceRef{ nullptr };

	nvrhi::TextureHandle ShadowMaps[4];
	nvrhi::TextureHandle GBufferDepth;

	nvrhi::ShaderHandle VertexShader;
	nvrhi::ShaderHandle PixelShader;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;
	nvrhi::BufferHandle ConstantBufferHandle;

public:
	void Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);
	void SetDependencies(nvrhi::TextureHandle shadow[4], nvrhi::TextureHandle depth);
	void DrawShadowMask(const ragdoll::SceneInformation& sceneInfo);
};