#include "ragdollpch.h"
#include "IntelTAAPass.h"

void IntelTAAPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void IntelTAAPass::TemporalAA(ragdoll::SceneRenderTargets* targets, Vector2 jitter)
{
	static uint32_t FrameIndex{};
	uint32_t FrameIndexMod2 = FrameIndex % 2;
	

	FrameIndex++;
}
