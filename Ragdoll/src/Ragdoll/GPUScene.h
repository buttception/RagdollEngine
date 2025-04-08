#pragma once
#include <nvrhi/nvrhi.h>
#include "Scene.h"

namespace ragdoll
{
	class FGPUScene {
		nvrhi::BufferHandle DebugBuffer{};
		//count buffers, static buffers need to be precreated because it takes a long time for it to be volatile
		nvrhi::BufferHandle PassedFrustumTestCountBuffer{};
		nvrhi::BufferHandle Phase1NonOccludedCountBuffer{};
		nvrhi::BufferHandle Phase1OccludedCountBuffer{};
		nvrhi::BufferHandle Phase2NonOccludedCountBuffer{};
		nvrhi::BufferHandle Phase2OccludedCountBuffer{};
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
		//Light buffers
		nvrhi::BufferHandle PointLightBufferHandle;
		uint32_t PointLightCount{ 0 };
		nvrhi::BufferHandle DepthSliceBoundsClipspaceBufferHandle;
		nvrhi::BufferHandle DepthSliceBoundsViewspaceBufferHandle;
		uint32_t TileCountX;
		uint32_t TileCountY;
		uint32_t TileCountZ;
		uint32_t LightGridCount{ 0 };
		nvrhi::BufferHandle LightGridBoundingBoxBufferHandle{};
		uint32_t FieldsNeeded;
		nvrhi::BufferHandle LightBitFieldsBufferHandle{};
		//raytracing stuff
		std::vector<nvrhi::rt::AccelStructHandle> BottomLevelASs;
		nvrhi::rt::AccelStructHandle TopLevelAS;
		//meshlet stuff
		nvrhi::BufferHandle IndirectMeshletArgsBuffer{};
		nvrhi::BufferHandle AmplificationGroupInfoBuffer{};
		nvrhi::BufferHandle MeshletFrustumCulledCountBuffer{};
		nvrhi::BufferHandle MeshletDegenerateConeCountBuffer{};
		nvrhi::BufferHandle MeshletOcclusionCulledPhase1CountBuffer{};
		nvrhi::BufferHandle MeshletOcclusionCulledPhase2CountBuffer{};

		//temp
		Scene* SceneRef;

		void Update(Scene* Scene);
		//will sort the proxies before making a instance buffer copy and uploading to gpu
		void UpdateBuffers(Scene* Scene);
		//updates the bounding box buffer, will not open or close the command list
		void UpdateLightGrid(Scene* Scene, nvrhi::CommandListHandle CommandList);
		//culls the light grid, will not open or close the command list
		void CullLightGrid(const SceneInformation& SceneInfo, nvrhi::CommandListHandle CommandList, ragdoll::SceneRenderTargets* RenderTargets);
		//returns the count buffer for the draw indirect function
		nvrhi::BufferHandle FrustumCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled, uint32_t AlphaTest = 0 /*cull all = 0, opaque = 1, alpha = 2*/);
		//returns the count buffer for the draw indirect function, culls the instances in the instance id buffer
		void OcclusionCullPhase1(
			nvrhi::CommandListHandle CommandList,
			SceneRenderTargets* Targets,
			Matrix ViewMatrix,
			Matrix ProjectionMatrix,
			nvrhi::BufferHandle FrustumVisibleCountBuffer,		//buffer storing the number of visible instances after frustum test
			nvrhi::BufferHandle& PassedOcclusionCountOutput,	//buffer storing the number of occluded instances
			nvrhi::BufferHandle& FailedOcclusionCountOutput,	//buffer storing the number of non occluded instances
			uint32_t ProxyCount
		);
		nvrhi::BufferHandle OcclusionCullPhase2(
			nvrhi::CommandListHandle CommandList,
			SceneRenderTargets* Targets,
			Matrix ViewMatrix,
			Matrix ProjectionMatrix,
			nvrhi::BufferHandle FrustumVisibleCountBuffer,
			uint32_t ProxyCount
		);
		void BuildHZB(nvrhi::CommandListHandle CommandList, SceneRenderTargets* Targets);

		//helper
		void ExtractFrustumPlanes(Vector4 OutPlanes[6], const Matrix& Projection, const Matrix& View);

		//gpu lights
		void CreateLightGrid(Scene* Scene);
	private:
		void CreateBuffers(const std::vector<Proxy>& Proxies);
	};
}