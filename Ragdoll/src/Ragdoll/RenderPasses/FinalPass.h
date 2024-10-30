#pragma once
#include <nvrhi/nvrhi.h>

class FinalPass {
	nvrhi::FramebufferHandle RenderTarget;
	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::TextureHandle FinalColor;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetRenderTarget(nvrhi::FramebufferHandle renderTarget);
	void SetDependencies(nvrhi::TextureHandle final);
	void DrawQuad();
};