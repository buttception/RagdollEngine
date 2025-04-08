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

	//one day should move it to a single buffer with macros for which index is which debug val
	nvrhi::BufferHandle PassedFrustumTestCountBuffer;
	nvrhi::BufferHandle Phase1NonOccludedCountBuffer;
	nvrhi::BufferHandle Phase2NonOccludedCountBuffer;
	nvrhi::BufferHandle Phase1OccludedCountBuffer;
	nvrhi::BufferHandle MeshletFrustumCulledCountBuffer;
	nvrhi::BufferHandle MeshletDegenerateConeCulledCountbuffer;
	nvrhi::BufferHandle MeshletOcclusionCulledPhase1CountBuffer;
	nvrhi::BufferHandle MeshletOcclusionCulledPhase2CountBuffer;

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

	//helper to build the command parameters for the meshlet pass
	void BuildMeshletParameters(
		ragdoll::FGPUScene* GPUScene,
		const ragdoll::SceneInformation& sceneInfo,
		const ragdoll::DebugInfo& debugInfo,
		uint32_t ProxyCount,
		nvrhi::BufferHandle InstanceIdBuffer,
		nvrhi::BufferHandle InstanceCountBuffer
	);

	//helper for dispatching the meshlets
	void DispatchMeshlets(
		ragdoll::FGPUScene* GPUScene,
		const ragdoll::SceneInformation& sceneInfo,
		const ragdoll::DebugInfo& debugInfo,
		ragdoll::SceneRenderTargets* targets,
		Matrix ViewMatrix,
		Matrix ProjectionMatrix,
		Matrix TestedViewMatrix,
		Matrix TestedProjectionMatrix,
		Vector3 CameraPosition,
		bool IsPhase1 = false
	);
};