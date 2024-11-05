#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
}
class DeferredLightPass {
	struct ConstantBuffer {
		//constant buffer struct specific to the forward pass
		Matrix InvViewProj;
		Vector4 LightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
		Vector4 SceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
		Vector3 LightDirection = { 1.f, -1.f, 1.f };
		float LightIntensity = 1.f;
		Vector3 CameraPosition;
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget{ nullptr };
	nvrhi::CommandListHandle CommandListRef{ nullptr };

	nvrhi::TextureHandle AlbedoHandle;
	nvrhi::TextureHandle NormalHandle;
	nvrhi::TextureHandle RoughnessMetallicHandle;
	nvrhi::TextureHandle AONormalized;
	nvrhi::TextureHandle SceneDepthZ;
	nvrhi::TextureHandle ShadowMask;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);
	void SetDependencies(nvrhi::TextureHandle albedo, nvrhi::TextureHandle normal, nvrhi::TextureHandle rm, nvrhi::TextureHandle ao, nvrhi::TextureHandle depth, nvrhi::TextureHandle shadowMask);

	void LightPass(const ragdoll::SceneInformation& sceneInfo);
};