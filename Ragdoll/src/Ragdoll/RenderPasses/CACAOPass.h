#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
	struct SceneRenderTargets;
}
class CACAOPass {
	struct ConstantBuffer {
		Vector2                 DepthUnpackConsts;
		Vector2                 CameraTanHalfFOV;

		Vector2                 NDCToViewMul;
		Vector2                 NDCToViewAdd;

		Vector2                 DepthBufferUVToViewMul;
		Vector2                 DepthBufferUVToViewAdd;

		float                   EffectRadius;                           // world (viewspace) maximum size of the shadow
		float                   EffectShadowStrength;                   // global strength of the effect (0 - 5)
		float                   EffectShadowPow;
		float                   EffectShadowClamp;

		float                   EffectFadeOutMul;                       // effect fade out from distance (ex. 25)
		float                   EffectFadeOutAdd;                       // effect fade out to distance   (ex. 100)
		float                   EffectHorizonAngleThreshold;            // limit errors on slopes and caused by insufficient geometry tessellation (0.05 to 0.5)
		float                   EffectSamplingRadiusNearLimitRec;          // if viewspace pixel closer than this, don't enlarge shadow sampling radius anymore (makes no sense to grow beyond some distance, not enough samples to cover everything, so just limit the shadow growth; could be SSAOSettingsFadeOutFrom * 0.1 or less)

		float                   DepthPrecisionOffsetMod;
		float                   NegRecEffectRadius;                     // -1.0 / EffectRadius
		float                   LoadCounterAvgDiv;                      // 1.0 / ( halfDepthMip[SSAO_DEPTH_MIP_LEVELS-1].sizeX * halfDepthMip[SSAO_DEPTH_MIP_LEVELS-1].sizeY )
		float                   AdaptiveSampleCountLimit;

		float                   InvSharpness;
		int32_t                 BlurNumPasses;
		float                   BilateralSigmaSquared;
		float                   BilateralSimilarityDistanceSigma;

		Vector4                 PatternRotScaleMatrices[4][5];

		float                   NormalsUnpackMul;
		float                   NormalsUnpackAdd;
		float                   DetailAOStrength;
		float                   Dummy0;

		Vector2                 SSAOBufferDimensions;
		Vector2                 SSAOBufferInverseDimensions;

		Vector2                 DepthBufferDimensions;
		Vector2                 DepthBufferInverseDimensions;

		int32_t                 DepthBufferOffset[2];
		int32_t					Dummy1[2];

		Vector4                 PerPassFullResUVOffset[4];

		Vector2                 OutputBufferDimensions;
		Vector2                 OutputBufferInverseDimensions;

		Vector2                 ImportanceMapDimensions;
		Vector2                 ImportanceMapInverseDimensions;

		Vector2                 DeinterleavedDepthBufferDimensions;
		Vector2                 DeinterleavedDepthBufferInverseDimensions;

		Vector2                 DeinterleavedDepthBufferOffset;
		Vector2                 DeinterleavedDepthBufferNormalisedOffset;

		Matrix                  NormalsWorldToViewspaceMatrix;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void GenerateAO(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
	void PrepareDepth(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void PrepareNormal(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void PrepareObscurance(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void PrepareImportance(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void PrepareImportanceA(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void PrepareImportanceB(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void SSAOPass(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void BlurSSAO(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
	void ApplySSAO(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets);
};