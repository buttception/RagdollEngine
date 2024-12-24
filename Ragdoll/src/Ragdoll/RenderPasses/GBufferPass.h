#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct DebugInfo;
	struct SceneRenderTargets;
	class FGPUScene;
}
class GBufferPass {
	struct ConstantBuffer {
		//constant buffer struct specific to the forward pass
		Matrix ViewProj;
		Matrix ViewProjWithAA;
		Matrix PrevViewProj;
		Vector2 RenderResolution;
		uint32_t MeshIndex{ 0 };
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawAllInstances(
		ragdoll::FGPUScene* GPUScene,
		uint32_t ProxyCount,
		nvrhi::BufferHandle instanceBuffer,
		const std::vector<ragdoll::InstanceGroupInfo>& infos,
		const ragdoll::SceneInformation& sceneInfo,
		const ragdoll::DebugInfo& debugInfo,
		ragdoll::SceneRenderTargets* targets);
};