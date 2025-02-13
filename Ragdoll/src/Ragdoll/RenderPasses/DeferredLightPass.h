#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
	class FGPUScene;
}
class DeferredLightPass {
	struct ConstantBuffer {
		//constant buffer struct specific to the forward pass
		Matrix InvViewProj;
		Matrix InvProjWithJitter;
		Vector4 LightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
		Vector4 SceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
		Vector3 LightDirection = { 1.f, -1.f, 1.f };
		float LightIntensity = 1.f;
		Vector3 CameraPosition;
		uint32_t PointLightCount;
		Vector2 ScreenSize;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void LightPass(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, ragdoll::FGPUScene* GPUScene);
	void LightGridPass(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, ragdoll::FGPUScene* GPUScene);
};