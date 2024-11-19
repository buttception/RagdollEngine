#pragma once

#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
	struct SceneRenderTargets;
}

class FSRPass {
    struct Fsr3UpscalerConstants {

        int32_t                     renderSize[2];
        int32_t                     previousFrameRenderSize[2];

        int32_t                     upscaleSize[2];
        int32_t                     previousFrameUpscaleSize[2];

        int32_t                     maxRenderSize[2];
        int32_t                     maxUpscaleSize[2];

        float                       deviceToViewDepth[4];

        float                       jitterOffset[2];
        float                       previousFrameJitterOffset[2];

        float                       motionVectorScale[2];
        float                       downscaleFactor[2];

        float                       motionVectorJitterCancellation[2];
        float                       tanHalfFOV;
        float                       jitterPhaseCount;

        float                       deltaTime;
        float                       deltaPreExposure;
        float                       viewSpaceToMetersFactor;
        float                       frameIndex;

        float                       velocityFactor;
    } Constants;
    float previousFramePreExposure;
    float preExposure;
    struct Fsr3UpscalerSpdConstants {

        uint32_t                    mips;
        uint32_t                    numworkGroups;
        uint32_t                    workGroupOffset[2];
        uint32_t                    renderSize[2];
    } SpdConstants;
    struct Fsr3UpscalerRcasConstants {

        uint32_t                    rcasConfig[4];
    } RcasConstants;

    int32_t DispatchX;
    int32_t DispatchY;
    uint32_t DispatchThreadGroupCountXY[2];
	nvrhi::CommandListHandle CommandListRef;

public:
	void Init(nvrhi::CommandListHandle cmdList);
	
	void Upscale(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, float _dt);

private:
	void PrepareInputs(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer);
    void ComputeLuminancePyramid(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer);
    void ComputeShadingChangePyramid(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer);
    void ComputeShadingChange(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer);
    void ComputeReactivity(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer);

    void UpdateConstants(const ragdoll::SceneInformation& sceneInfo, float _dt);
};