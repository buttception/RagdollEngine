#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class SkyPass {
	struct ConstantBuffer {
		Matrix InvViewProj;
		Vector3 CameraPosition;
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget;
	nvrhi::CommandListHandle CommandListRef{ nullptr };

	nvrhi::TextureHandle SkyTexture;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);
	void SetDependencies(nvrhi::TextureHandle sky);
	void DrawSky(const ragdoll::SceneInformation& sceneInfo);
};