#pragma once
#include <nvrhi/nvrhi.h>
#include "Scene.h"

namespace ragdoll
{
	class FGPUScene {
		nvrhi::BufferHandle IndirectDrawArgsBuffer{};
		nvrhi::BufferHandle InstanceBuffer{};
		nvrhi::BufferHandle InstanceIdBuffer{};
		//buffer of offsets to each mesh in the instance buffer
		nvrhi::BufferHandle InstanceOffsetBuffer{};
	public:
		void Update(Scene* Scene);
		//will sort the proxies before making a instance buffer copy and uploading to gpu
		void UpdateInstanceBuffer(std::vector<Proxy>& Proxies);
		void InstanceCull(nvrhi::CommandListHandle CommandList, const Matrix& ViewProjection, const Vector3& CameraPosition);

	private:
		void CreateBuffers(const std::vector<Proxy>& Proxies);
		void ResetBuffers(nvrhi::CommandListHandle CommandList);
		//cull the scene then update the instance id buffer and indirect draw args buffer
		void FrustumCullScene(nvrhi::CommandListHandle Commandlist, const Matrix& ViewProjection, const Vector3& CameraPosition);
	};
}