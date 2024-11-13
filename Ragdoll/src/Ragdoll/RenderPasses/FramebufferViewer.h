#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
	struct SceneRenderTargets;
}
class FramebufferViewer {
	struct ConstantBuffer{
		Vector4 Add;
		Vector4 Mul;
		uint32_t ComponentCount;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawTarget(nvrhi::TextureHandle texture, Vector4 add, Vector4 mul, uint32_t numComp, ragdoll::SceneRenderTargets* targets);

};