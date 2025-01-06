#pragma once
#include <nvrhi/nvrhi.h>
#include "Scene.h"

namespace ragdoll
{
	class FGPUScene {
	public:
		//TODO: upload mesh data as well, so can derive bounding box with the instance buffer
		//InstanceBuffer (only transforms)
		//MaterialBuffer (only material data)
		//MeshBuffer (only mesh data)
		nvrhi::BufferHandle MeshBuffer{};
		nvrhi::BufferHandle IndirectDrawArgsBuffer{};
		nvrhi::BufferHandle InstanceBuffer{}; //all the material data can be separated and duplicates can be removed, so it can contain only transformation data
		nvrhi::BufferHandle InstanceIdBuffer{};
		//buffer of all the instances bounding boxes in world
		nvrhi::BufferHandle InstanceBoundingBoxBuffer{};	//in world space, TODO: remove wen i derive the boxes in gpu scene instead

		void Update(Scene* Scene);
		//will sort the proxies before making a instance buffer copy and uploading to gpu
		void UpdateInstanceBuffer(std::vector<Proxy>& Proxies);
		//returns the count buffer for the draw indirect function
		nvrhi::BufferHandle InstanceCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled);

		//helper
		void ExtractFrustumPlanes(Vector4 OutPlanes[6], const Matrix& Projection, const Matrix& View);

	private:
		void CreateBuffers(const std::vector<Proxy>& Proxies);
		//cull the scene then update the instance id buffer and indirect draw args buffer
		void FrustumCullScene(nvrhi::CommandListHandle Commandlist, nvrhi::BufferHandle ConstantBufferHandle, nvrhi::BindingSetHandle BindingSetHandle, uint32_t ProxyCount);
	};
}