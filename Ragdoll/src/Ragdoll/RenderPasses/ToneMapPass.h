#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
	struct SceneRenderTargets;
}
class ToneMapPass {
	struct ConstantBuffer {
		float Gamma;
		float Exposure;
		int32_t UseFixedExposure{ 0 };
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void ToneMap(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle exposureHandle, ragdoll::SceneRenderTargets* targets);
};