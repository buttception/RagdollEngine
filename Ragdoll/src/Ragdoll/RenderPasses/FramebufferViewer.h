#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
}
class FramebufferViewer {
	struct ConstantBuffer{
		float Add;
		float Mul;
		uint32_t ComponentCount;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawTarget(nvrhi::TextureHandle texture, float add, float mul, uint32_t numComp);

};