#pragma once
#include <nvrhi/nvrhi.h>
#include "xegtao/XeGTAO.h"

namespace ragdoll {
	struct SceneInformation;
}
class XeGTAOPass {
	XeGTAO::GTAOConstants CBuffer;
	XeGTAO::GTAOSettings Settings;
	nvrhi::CommandListHandle CommandListRef{ nullptr };

	struct Textures//exist for me to faster set dependencies
	{
		//
		// inputs
		nvrhi::TextureHandle DepthBuffer;
		nvrhi::TextureHandle NormalMap;
		nvrhi::TextureHandle ORM;

		//outputs
		nvrhi::TextureHandle DepthMips;
		nvrhi::TextureHandle AOTerm;
		nvrhi::TextureHandle EdgeMap;
		nvrhi::TextureHandle FinalAOTerm;
	};

	//inputs
	nvrhi::TextureHandle DepthBuffer;
	nvrhi::TextureHandle NormalMap;
	nvrhi::TextureHandle ORM;

	//outputs
	nvrhi::TextureHandle DepthMips;
	nvrhi::TextureHandle AOTerm;
	nvrhi::TextureHandle EdgeMap;
	nvrhi::TextureHandle FinalAOTerm;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetDependencies(Textures dependencies);
	void UpdateConstants(const uint32_t width, const uint32_t height, const Matrix& projMatrix);

	void GenerateAO(const ragdoll::SceneInformation& sceneInfo);

	void GenerateDepthMips(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix);
	void MainPass(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix);
	void Denoise(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix);
};