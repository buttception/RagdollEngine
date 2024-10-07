#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
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

	const std::vector<BloomMip>* Mips;
	nvrhi::TextureHandle SceneColor;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetDependencies(nvrhi::TextureHandle sceneColor, const std::vector<BloomMip>* mips);

	void Bloom(const ragdoll::SceneInformation& sceneInfo);
	void DownSample();
	void UpSample(float filterRadius, float bloomIntensity);
};