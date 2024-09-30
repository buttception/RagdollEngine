#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
}
class ToneMapPass {
	struct ConstantBuffer {
		float Gamma;
		float Exposure;
		int32_t UseFixedExposure{ 0 };
	}CBuffer;

	nvrhi::FramebufferHandle RenderTarget{ nullptr };
	nvrhi::CommandListHandle CommandListRef{ nullptr };

	nvrhi::TextureHandle Target;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);
	void SetDependencies(nvrhi::TextureHandle SceneColor);

	void ToneMap(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle exposureHandle);
};