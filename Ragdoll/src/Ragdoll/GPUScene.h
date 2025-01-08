#pragma once
#include <nvrhi/nvrhi.h>
#include "Scene.h"

namespace ragdoll
{
	class FGPUScene {
		nvrhi::BufferHandle DebugBuffer{};
	public:
		//TODO: upload mesh data as well, so can derive bounding box with the instance buffer
		//InstanceBuffer (only transforms)
		nvrhi::BufferHandle InstanceBuffer{};
		//MaterialBuffer (only material data)
		nvrhi::BufferHandle MaterialBuffer{};
		//MeshBuffer (only mesh data)
		nvrhi::BufferHandle MeshBuffer{};
		nvrhi::BufferHandle IndirectDrawArgsBuffer{};
		nvrhi::BufferHandle InstanceIdBuffer{};
		nvrhi::BufferHandle OccludedInstanceIdBuffer{};

		void Update(Scene* Scene);
		//will sort the proxies before making a instance buffer copy and uploading to gpu
		void UpdateInstanceBuffer(std::vector<Proxy>& Proxies);
		//returns the count buffer for the draw indirect function
		nvrhi::BufferHandle FrustumCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled);
		//returns the count buffer for the draw indirect function, culls the instances in the instance id buffer
		void OcclusionCullPhase1(
			nvrhi::CommandListHandle CommandList,
			const SceneInformation& SceneInfo,
			SceneRenderTargets* Targets,
			nvrhi::BufferHandle FrustumVisibleCountBuffer,
			nvrhi::BufferHandle& PassedOcclusionCountOutput,
			nvrhi::BufferHandle& FailedOcclusionCountOutput,
			uint32_t ProxyCount
		);
		nvrhi::BufferHandle OcclusionCullPhase2(
			nvrhi::CommandListHandle CommandList,
			const SceneInformation& SceneInfo,
			SceneRenderTargets* Targets,
			nvrhi::BufferHandle FrustumVisibleCountBuffer,
			uint32_t ProxyCount
		);
		void BuildHZB(nvrhi::CommandListHandle CommandList, SceneRenderTargets* Targets);

		//helper
		void ExtractFrustumPlanes(Vector4 OutPlanes[6], const Matrix& Projection, const Matrix& View);

	private:
		void CreateBuffers(const std::vector<Proxy>& Proxies);
	};
}