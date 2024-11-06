#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll
{
	struct SceneRenderTargets;
}
class IntelTAAPass {
    __declspec(align(16)) struct FTAAResolve
    {
        Vector4         Resolution;//width, height, 1/width, 1/height
        float           JitterX;
        float           JitterY;
        uint32_t        FrameNumber;
        uint32_t        DebugFlags;
    } ConstantBuffer;
	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void TemporalAA(ragdoll::SceneRenderTargets* targets, Vector2 jitter);
};