#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll
{
    struct SceneInformation;
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
    Vector3 InlineConstants;
    __declspec(align(16)) struct CBuffer
    {
        Matrix CurToPrevXForm;
        Matrix premult;
        Matrix postmult;
        Matrix prevViewProj;
        Matrix viewProjInverse;
        Vector2 res;
        float near_p;
        float JitterX;
        float JitterY;
    } MotionConstant;
	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void TemporalAA(ragdoll::SceneRenderTargets* targets, const ragdoll::SceneInformation& sceneInfo, Vector2 jitter);
};