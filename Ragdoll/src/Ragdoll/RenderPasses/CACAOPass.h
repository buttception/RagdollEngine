#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct SceneInformation;
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

	struct Textures//exist for me to faster set dependencies
	{
		nvrhi::TextureHandle DepthBuffer;
		nvrhi::TextureHandle GBufferNormals;

		nvrhi::TextureHandle LoadCounter;	//1x1x1 r32uint
		nvrhi::TextureHandle DeinterleavedDepthMips;
		nvrhi::TextureHandle DeinterleavedNormals;
		nvrhi::TextureHandle SSAOPong; //r8g8unorm
		nvrhi::TextureHandle SSAOPing; //r8g8unorm
		nvrhi::TextureHandle ImportanceMap;	//r8unorm
		nvrhi::TextureHandle ImportanceMapPong;	//r8unorm half size
		nvrhi::TextureHandle GBufferAO;
	};

	//inputs
	nvrhi::TextureHandle DepthBuffer;
	nvrhi::TextureHandle GBufferNormals;

	//outputs
	nvrhi::TextureHandle LoadCounter;
	nvrhi::TextureHandle DeinterleavedDepthMips;
	nvrhi::TextureHandle DeinterleavedNormals;
	nvrhi::TextureHandle SSAOPong; //r8g8unorm
	nvrhi::TextureHandle SSAOPing; //r8g8unorm
	nvrhi::TextureHandle ImportanceMap;	//r8unorm
	nvrhi::TextureHandle ImportanceMapPong;	//r8unorm half size
	nvrhi::TextureHandle GBufferAO;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void SetDependencies(Textures dependencies);

	void GenerateAO(const ragdoll::SceneInformation& sceneInfo);
	void PrepareDepth(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer);
	void PrepareNormal(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer);
	void GenerateObscurance(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer);
};