#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
}
class SkyPass {
	struct ConstantBuffer {
		Matrix InvViewProj;
		Vector3 CameraPosition{};
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);
	void DrawSky(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
};