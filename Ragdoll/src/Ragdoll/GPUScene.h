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
		nvrhi::BufferHandle DepthSliceBoundsViewspaceBufferHandle;;
		nvrhi::TextureHandle LightGridTextureHandle{};
		uint32_t LightGridCount{ 0 };
		nvrhi::BufferHandle LightGridBoundingBoxBufferHandle{};
		uint32_t FieldsNeeded;
		nvrhi::BufferHandle LightBitFieldsBufferHandle{};

		void Update(Scene* Scene);
		//will sort the proxies before making a instance buffer copy and uploading to gpu
		void UpdateBuffers(Scene* Scene);
		//updates the bounding box buffer, will not open or close the command list
		void UpdateLightGrid(Scene* Scene, nvrhi::CommandListHandle CommandList);
		//Gets the min max depth value for each tile
		void GetLightGridMinMax(nvrhi::CommandListHandle CommandList, ragdoll::SceneRenderTargets* RenderTargets);
		//culls the light grid, will not open or close the command list
		void CullLightGrid(const SceneInformation& SceneInfo, nvrhi::CommandListHandle CommandList, ragdoll::SceneRenderTargets* RenderTargets);
		//returns the count buffer for the draw indirect function
		nvrhi::BufferHandle FrustumCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled);
		//returns the count buffer for the draw indirect function, culls the instances in the instance id buffer
		void OcclusionCullPhase1(
			nvrhi::CommandListHandle CommandList,
			SceneRenderTargets* Targets,
			Matrix ViewMatrix,
			Matrix ProjectionMatrix,
			nvrhi::BufferHandle FrustumVisibleCountBuffer,
			nvrhi::BufferHandle& PassedOcclusionCountOutput,
			nvrhi::BufferHandle& FailedOcclusionCountOutput,
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