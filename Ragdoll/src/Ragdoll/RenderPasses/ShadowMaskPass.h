#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
}
class ShadowMaskPass {
	struct ConstantBuffer {
		Matrix LightViewProj[4];
		Matrix InvViewProj;
		Matrix View;
		int EnableCascadeDebug;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawShadowMask(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
};