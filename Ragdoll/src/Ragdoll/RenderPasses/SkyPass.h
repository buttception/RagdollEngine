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
		Matrix ViewProj;
		Matrix PrevViewProj;
		Vector3 CameraPosition{};
		float pad1;
		Vector3 PrevCameraPosition{};
		float pad2;
		Vector2 RenderResolution;
		int32_t DrawVelocityOnSky;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);
	void DrawSky(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
};