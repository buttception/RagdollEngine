#pragma once
#include <nvrhi/nvrhi.h>
#include "xegtao/XeGTAO.h"

namespace ragdoll {
	struct SceneInformation;
	struct SceneRenderTargets;
}
class XeGTAOPass {
	struct ConstantBuffer
	{
		Matrix viewMatrix;
		Matrix InvViewProjMatrix;
		Matrix prevViewProjMatrix;
		float modulationFactor;
	} OtherBuffer;
	XeGTAO::GTAOConstants CBuffer;
	XeGTAO::GTAOSettings Settings;
	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void UpdateConstants(const uint32_t width, const uint32_t height, const Matrix& projMatrix);

	void GenerateAO(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);

	void GenerateDepthMips(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets);
	void MainPass(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets);
	void Denoise(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets);
	void Compose(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets);
};