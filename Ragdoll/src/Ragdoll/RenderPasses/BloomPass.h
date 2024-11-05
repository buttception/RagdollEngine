#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
	struct SceneRenderTargets;
}
struct BloomMip {
	uint32_t Width, Height;
	nvrhi::TextureHandle Image;
};
class BloomPass {
	struct ConstantBufferDS {
		uint32_t Width, Height;
	}CBufferDS;
	struct ConstantBufferUS {
		float FilterRadius{ 0.05f };
		float BloomIntensity{ 0.f };
		int32_t Final = 0;
	}CBufferUS;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void Bloom(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
	void DownSample(ragdoll::SceneRenderTargets* targets);
	void UpSample(float filterRadius, float bloomIntensity, ragdoll::SceneRenderTargets* targets);
};