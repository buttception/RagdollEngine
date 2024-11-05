#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
}
class ShadowPass {
	struct ConstantBuffer {
		Matrix LightViewProj;
		uint32_t InstanceOffset;
		uint32_t CascadeIndex;
	}CBuffer[4];

	nvrhi::CommandListHandle CommandListRef[4]{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList0, nvrhi::CommandListHandle cmdList1, nvrhi::CommandListHandle cmdList2, nvrhi::CommandListHandle cmdList3);

	void DrawAllInstances(nvrhi::BufferHandle instanceBuffer, std::vector<ragdoll::InstanceGroupInfo>& infos, const ragdoll::SceneInformation& sceneInfo, uint32_t cascadeIndex, ragdoll::SceneRenderTargets* targets);
};