#pragma once
#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
}
class DebugPass {
	struct ConstantBuffer {
		//constant buffer struct specific to the forward pass
		Matrix ViewProj;
		uint32_t InstanceOffset;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawDebug(
		nvrhi::BufferHandle instanceBuffer,
		size_t instanceCount,
		nvrhi::BufferHandle LineBuffer,
		size_t LineCount,
		const ragdoll::SceneInformation& sceneInfo,
		ragdoll::SceneRenderTargets* targets
	);

	void DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, size_t instanceCount, const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
	void DrawLines(nvrhi::BufferHandle LineBuffer, size_t LineCount, const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
};