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

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget[4]);

	void DrawAllInstances(nvrhi::BufferHandle instanceBuffer[4], std::vector<ragdoll::InstanceGroupInfo>* infos, const ragdoll::SceneInformation& sceneInfo);
};