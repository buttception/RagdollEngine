#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll
{
	struct SceneRenderTargets;
}
class FinalPass {
	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawQuad(ragdoll::SceneRenderTargets* targets, bool upscaled);
	void MeshletPass(ragdoll::SceneRenderTargets* targets, bool upscaled);
};