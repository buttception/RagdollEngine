#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
	class FGPUScene;
}
class ShadowPass {
	struct ConstantBuffer {
		Matrix LightViewProj;
		uint32_t InstanceOffset;
		uint32_t CascadeIndex;
		uint32_t MeshIndex;
	}CBuffer[4];

	nvrhi::CommandListHandle CommandListRef[4]{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList0, nvrhi::CommandListHandle cmdList1, nvrhi::CommandListHandle cmdList2, nvrhi::CommandListHandle cmdList3);

	void DrawAllInstances(
		uint32_t CascadeIndex,
		ragdoll::FGPUScene* GPUScene,
		uint32_t ProxyCount,
		const ragdoll::SceneInformation& sceneInfo,
		ragdoll::SceneRenderTargets* targets);
};