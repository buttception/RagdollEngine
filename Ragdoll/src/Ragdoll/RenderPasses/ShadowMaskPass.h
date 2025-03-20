#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
	class FGPUScene;
}
class ShadowMaskPass {
	struct ConstantBuffer {
		Matrix LightViewProj[4];
		Matrix InvViewProj;
		Matrix View;
		int EnableCascadeDebug;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::rt::ShaderTableHandle ShaderTableHandle{};

	nvrhi::BufferHandle DenoiserTileBuffer;
	nvrhi::BufferHandle DenoiserTileMetaDataBuffer;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawShadowMask(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
	void RaytraceShadowMask(const ragdoll::SceneInformation& sceneInfo, const ragdoll::FGPUScene* GPUScene, ragdoll::SceneRenderTargets* targets);

private:
	void FFX_Denoise(const ragdoll::SceneInformation& SceneInfo, ragdoll::SceneRenderTargets* Targets);
};