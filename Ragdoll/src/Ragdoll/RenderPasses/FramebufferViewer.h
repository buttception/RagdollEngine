#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
	struct DebugInfo;
	struct SceneRenderTargets;
	class FGPUScene;
}
class FramebufferViewer {
	struct ConstantBuffer{
		Vector4 Add;
		Vector4 Mul;
		Vector2 TexcoordAdd = Vector2(0.f, 0.f);
		Vector2 TexcoordMul = Vector2(1.f, 1.f);
		uint32_t ComponentCount;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void DrawTarget(ragdoll::FGPUScene* GPUScene, nvrhi::TextureHandle texture, Vector4 add, Vector4 mul, uint32_t numComp, ragdoll::SceneRenderTargets* targets);
	void DrawLightGridHitMap(const ragdoll::FGPUScene* GPUScene, const ragdoll::SceneInformation& SceneInfo, const ragdoll::DebugInfo& DebugInfo, ragdoll::SceneRenderTargets* Targets);
};