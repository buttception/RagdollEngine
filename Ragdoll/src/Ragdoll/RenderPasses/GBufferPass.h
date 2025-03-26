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
		uint32_t InstanceId{ 0 };
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);
	//determines whether to cull objects
	void Draw(ragdoll::FGPUScene* GPUScene,
		uint32_t ProxyCount,
		const ragdoll::SceneInformation& sceneInfo,
		const ragdoll::DebugInfo& debugInfo,
		ragdoll::SceneRenderTargets* targets,
		bool isOcclusionCullingEnabled);

	void DrawMeshlets(
		ragdoll::FGPUScene* GPUScene,
		uint32_t ProxyCount,
		const ragdoll::SceneInformation& sceneInfo,
		const ragdoll::DebugInfo& debugInfo,
		ragdoll::SceneRenderTargets* targets
	);

	nvrhi::BufferHandle PassedFrustumTestCountBuffer;
	nvrhi::BufferHandle Phase1NonOccludedCountBuffer;
	nvrhi::BufferHandle Phase2NonOccludedCountBuffer;

private:
	//only draw instances
	void DrawAllInstances(
		ragdoll::FGPUScene* GPUScene,
		nvrhi::BufferHandle CountBuffer,
		uint32_t ProxyCount,
		const ragdoll::SceneInformation& sceneInfo,
		const ragdoll::DebugInfo& debugInfo,
		ragdoll::SceneRenderTargets* targets,
		bool opaquePass = true);
};