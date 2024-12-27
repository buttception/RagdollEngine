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
	struct FConstantBuffer
	{
		uint32_t Width, Height;
		float FilterRadius{ 0.05f };
		float BloomIntensity{ 0.f };
	} ConstantBuffer;
	struct ConstantBufferDS {
		uint32_t Width, Height;
	}CBufferDS;
	struct ConstantBufferUS {
		float FilterRadius{ 0.05f };
		float BloomIntensity{ 0.f };
		int32_t Final = 0;
	}CBufferUS;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

	nvrhi::BufferHandle ConstantBufferHandle;
	nvrhi::BindingSetHandle DownSampleSetHandles[5];
	nvrhi::BindingSetHandle UpSampleSetHandles[5];

public:
	void Init(nvrhi::CommandListHandle cmdList, ragdoll::SceneRenderTargets* targets);

	void Bloom(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
	void DownSample(ragdoll::SceneRenderTargets* targets);
	void UpSample(float filterRadius, float bloomIntensity, ragdoll::SceneRenderTargets* targets);
};