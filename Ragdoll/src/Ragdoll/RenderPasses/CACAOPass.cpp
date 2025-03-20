#include "ragdollpch.h"
#include "CACAOPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void CACAOPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void CACAOPass::GenerateAO(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, CACAO);
	RD_GPU_SCOPE("CACAO", CommandListRef);
	CommandListRef->beginMarker("CACAO");
	//cbuffer shared amongst all
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "CACAO CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	
	CBuffer.DepthBufferDimensions = Vector2((float)targets->CurrDepthBuffer->getDesc().width, (float)targets->CurrDepthBuffer->getDesc().height);
	CBuffer.DepthBufferInverseDimensions = Vector2(1.f, 1.f) / CBuffer.DepthBufferDimensions;
	CBuffer.DeinterleavedDepthBufferDimensions = Vector2((float)targets->DeinterleavedDepth->getDesc().width, (float)targets->DeinterleavedDepth->getDesc().height);
	CBuffer.DeinterleavedDepthBufferInverseDimensions = Vector2(1.f, 1.f) / CBuffer.DeinterleavedDepthBufferDimensions;
	CBuffer.EffectRadius = 1.2f;
	const int spmap[5]{ 0, 1, 4, 3, 2 };
	const float PI = 3.1415926535897932384626433832795f;
#define FFX_CACAO_ARRAY_SIZE(xs)             (sizeof(xs) / sizeof(xs[0]))
	const int subPassCount = FFX_CACAO_ARRAY_SIZE(CBuffer.PatternRotScaleMatrices[0]);
	for (int passId = 0; passId < FFX_CACAO_ARRAY_SIZE(CBuffer.PatternRotScaleMatrices); ++passId)
	{
		for (int subPass = 0; subPass < subPassCount; subPass++)
		{
			const int a = passId;

			const int b = spmap[subPass];

			float ca, sa;
			const float angle0 = ((float)a + (float)b / (float)subPassCount) * (PI) * 0.5f;

			ca = cosf(angle0);
			sa = sinf(angle0);

			const float scale = 1.0f + (a - 1.5f + (b - (subPassCount - 1.0f) * 0.5f) / (float)subPassCount) * 0.07f;

			CBuffer.PatternRotScaleMatrices[passId][subPass].x = scale * ca;
			CBuffer.PatternRotScaleMatrices[passId][subPass].y = scale * -sa;
			CBuffer.PatternRotScaleMatrices[passId][subPass].z = -scale * sa;
			CBuffer.PatternRotScaleMatrices[passId][subPass].w = -scale * ca;
		}
	}
	CBuffer.NDCToViewMul.x = CBuffer.CameraTanHalfFOV.x * 2.0f;
	CBuffer.NDCToViewMul.y = CBuffer.CameraTanHalfFOV.y * -2.0f;
	CBuffer.NDCToViewAdd.x = CBuffer.CameraTanHalfFOV.x * -1.0f;
	CBuffer.NDCToViewAdd.y = CBuffer.CameraTanHalfFOV.y * 1.0f;
	const float ratio = ((float)targets->AONormalized->getDesc().width) / ((float)targets->CurrDepthBuffer->getDesc().width);
	const float border = (1.0f - ratio) / 2.0f;
	CBuffer.DepthBufferUVToViewMul.x = CBuffer.NDCToViewMul.x / ratio;
	CBuffer.DepthBufferUVToViewAdd.x = CBuffer.NDCToViewAdd.x - CBuffer.NDCToViewMul.x * border / ratio;
	CBuffer.DepthBufferUVToViewMul.y = CBuffer.NDCToViewMul.y / ratio;
	CBuffer.DepthBufferUVToViewAdd.y = CBuffer.NDCToViewAdd.y - CBuffer.NDCToViewMul.y * border / ratio;
	CBuffer.DepthUnpackConsts = Vector2(-0.1f, 0);
	float halfFovX = sceneInfo.CameraFov * 0.5f * DirectX::XM_PI / 180.f;
	CBuffer.CameraTanHalfFOV = Vector2(tanf(halfFovX), tanf(halfFovX / sceneInfo.CameraAspect));
	CBuffer.EffectRadius = 1.5;
	CBuffer.EffectShadowStrength = 2.f;
	CBuffer.EffectShadowPow = 1.5f;
	CBuffer.EffectShadowClamp = 0.98f;
	float fadeOutTo = 10000.f;
	float fadeOutFrom = 5000.f;
	CBuffer.EffectFadeOutMul = -1.0f / (fadeOutTo - fadeOutFrom);
	CBuffer.EffectFadeOutAdd = fadeOutFrom / (fadeOutTo - fadeOutFrom) + 1.0f;
	float effectSamplingRadiusNearLimit = (CBuffer.EffectRadius * 1.2f);
	effectSamplingRadiusNearLimit /= CBuffer.CameraTanHalfFOV.y;  // to keep the effect same regardless of FOV
	CBuffer.EffectSamplingRadiusNearLimitRec = 1.f / effectSamplingRadiusNearLimit;
	CBuffer.EffectHorizonAngleThreshold = 0.06f;
	CBuffer.DepthPrecisionOffsetMod = 0.9992f;
	CBuffer.NegRecEffectRadius = -1.f / CBuffer.EffectRadius;
	CBuffer.LoadCounterAvgDiv = 9.0f / (float)(targets->ImportanceMap->getDesc().width * targets->ImportanceMap->getDesc().height * 255.0);
	CBuffer.AdaptiveSampleCountLimit = 0.75f;
	CBuffer.NormalsUnpackMul = 1.f;
	CBuffer.NormalsUnpackAdd = 0.f;
	CBuffer.NormalsWorldToViewspaceMatrix = sceneInfo.MainCameraView * DirectX::XMMatrixScaling(1.f, 1.f, -1.f);
	CBuffer.ImportanceMapDimensions = { (float)targets->ImportanceMap->getDesc().width, (float)targets->ImportanceMap->getDesc().height };
	CBuffer.ImportanceMapInverseDimensions = Vector2{ 1.f, 1.f } / CBuffer.ImportanceMapDimensions;
	CBuffer.InvSharpness = 0.02f;
	CBuffer.BlurNumPasses = 4;
	CBuffer.BilateralSigmaSquared = 5.f;
	CBuffer.BilateralSimilarityDistanceSigma = 0.1f;
	CBuffer.DetailAOStrength = 0.5f;
	CBuffer.SSAOBufferDimensions = { (float)targets->SSAOBufferPong->getDesc().width, (float)targets->SSAOBufferPong->getDesc().height };
	CBuffer.SSAOBufferInverseDimensions = Vector2{ 1.f, 1.f } / CBuffer.SSAOBufferDimensions;
	CBuffer.OutputBufferDimensions = { (float)targets->AONormalized->getDesc().width, (float)targets->AONormalized->getDesc().height };
	CBuffer.OutputBufferInverseDimensions = Vector2{ 1.f, 1.f } / CBuffer.OutputBufferDimensions;

	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	//clear the load counter
	CommandListRef->clearTextureUInt(targets->LoadCounter, nvrhi::AllSubresources, 0);
    //prepare the depth deinterleaved and mips
	{
		//MICROPROFILE_SCOPEGPUI("Generate Depth Mips", MP_LIGHTYELLOW1);
		PrepareDepth(sceneInfo, ConstantBufferHandle, targets);
	}
	//prepare the normal deinterleaved
	{
		//MICROPROFILE_SCOPEGPUI("Generate Normal Mips", MP_LIGHTYELLOW1);
		PrepareNormal(sceneInfo, ConstantBufferHandle, targets);
	}
	//part 1 ssao generate
	{
		//MICROPROFILE_SCOPEGPUI("Prepare Obscurance Map", MP_LIGHTYELLOW1);
		PrepareObscurance(sceneInfo, ConstantBufferHandle, targets);
	}
	//importance map
	{
		//MICROPROFILE_SCOPEGPUI("Prepare Importance Map", MP_LIGHTYELLOW1);
		PrepareImportance(sceneInfo, ConstantBufferHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Importance Ping Pong", MP_LIGHTYELLOW1);
		PrepareImportanceA(sceneInfo, ConstantBufferHandle, targets);
		PrepareImportanceB(sceneInfo, ConstantBufferHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Generate SSAO", MP_LIGHTYELLOW1);
		SSAOPass(sceneInfo, ConstantBufferHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Blur SSAO", MP_LIGHTYELLOW1);
		BlurSSAO(sceneInfo, ConstantBufferHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Apply SSAO", MP_LIGHTYELLOW1);
		ApplySSAO(sceneInfo, ConstantBufferHandle, targets);
	}

	CommandListRef->endMarker();
}

void CACAOPass::PrepareDepth(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Depth Mip", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Depth Mip");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->CurrDepthBuffer),
		//nvrhi::BindingSetItem::Texture_SRV(10, LoadCounter),
		//nvrhi::BindingSetItem::Texture_SRV(3, DeinterleavedDepth),
		//nvrhi::BindingSetItem::Texture_SRV(4, DeinterleavedNormals),
		//nvrhi::BindingSetItem::Texture_SRV(5, SSAOPing),
		//nvrhi::BindingSetItem::Texture_SRV(6, SSAOPong),
		//nvrhi::BindingSetItem::Texture_SRV(7, ImportanceMap),
		//nvrhi::BindingSetItem::Texture_SRV(8, ImportanceMapPong),
		nvrhi::BindingSetItem::Texture_UAV(12, targets->DeinterleavedDepth, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{0,1,0,4}),
		nvrhi::BindingSetItem::Texture_UAV(13, targets->DeinterleavedDepth, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{1,1,0,4}),
		nvrhi::BindingSetItem::Texture_UAV(14, targets->DeinterleavedDepth, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{2,1,0,4}),
		nvrhi::BindingSetItem::Texture_UAV(15, targets->DeinterleavedDepth, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{3,1,0,4}),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareDepth.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->DeinterleavedDepth->getDesc().width + 8 - 1) / 8, (targets->DeinterleavedDepth->getDesc().height + 8 - 1) / 8, 1);
	CommandListRef->endMarker();
}

void CACAOPass::PrepareNormal(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Depth Normal", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Normal");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->GBufferNormal),
		nvrhi::BindingSetItem::Texture_UAV(4, targets->DeinterleavedNormals),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareNormal.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->DeinterleavedNormals->getDesc().width + 8 - 1) / 8, (targets->DeinterleavedNormals->getDesc().height + 8 - 1) / 8, 1);
	CommandListRef->endMarker();
}

void CACAOPass::PrepareObscurance(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Obscurance", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Generate Obscurance");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->DeinterleavedDepth),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->DeinterleavedNormals),
		nvrhi::BindingSetItem::Texture_UAV(6, targets->SSAOBufferPong),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Mirror]),
		nvrhi::BindingSetItem::Sampler(3, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareBaseQ3.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->SSAOBufferPong->getDesc().width + 8 - 1) / 8, (targets->SSAOBufferPong->getDesc().height + 8 - 1) / 8, 4);
	CommandListRef->endMarker();
}

void CACAOPass::PrepareImportance(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Importance", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Generate Importance");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(6, targets->SSAOBufferPong),
		nvrhi::BindingSetItem::Texture_UAV(7, targets->ImportanceMap),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareImportance.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->ImportanceMap->getDesc().width + 8 - 1) / 8, (targets->ImportanceMap->getDesc().height + 8 - 1) / 8, 4);
	CommandListRef->endMarker();
}

void CACAOPass::PrepareImportanceA(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "ImportanceA", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Generate ImportanceA");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(7, targets->ImportanceMap),
		nvrhi::BindingSetItem::Texture_UAV(8, targets->ImportanceMapPong),
		nvrhi::BindingSetItem::Sampler(2, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareImportanceA.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->ImportanceMap->getDesc().width + 8 - 1) / 8, (targets->ImportanceMap->getDesc().height + 8 - 1) / 8, 4);
	CommandListRef->endMarker();
}

void CACAOPass::PrepareImportanceB(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "ImportanceB", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Generate ImportanceB");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(8, targets->ImportanceMapPong),
		nvrhi::BindingSetItem::Texture_UAV(10, targets->LoadCounter),
		nvrhi::BindingSetItem::Texture_UAV(7, targets->ImportanceMap),
		nvrhi::BindingSetItem::Sampler(2, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareImportanceB.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->ImportanceMap->getDesc().width + 8 - 1) / 8, (targets->ImportanceMap->getDesc().height + 8 - 1) / 8, 4);
	CommandListRef->endMarker();
}

void CACAOPass::SSAOPass(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "SSAO", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Generate SSAO");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(10, targets->LoadCounter),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->DeinterleavedDepth),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->DeinterleavedNormals),
		nvrhi::BindingSetItem::Texture_SRV(6, targets->SSAOBufferPong),
		nvrhi::BindingSetItem::Texture_SRV(7, targets->ImportanceMap),
		nvrhi::BindingSetItem::Texture_UAV(6, targets->SSAOBufferPing),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Mirror]),
		nvrhi::BindingSetItem::Sampler(2, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp]),
		nvrhi::BindingSetItem::Sampler(3, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareQ3.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((targets->SSAOBufferPong->getDesc().width + 8 - 1) / 8, (targets->SSAOBufferPong->getDesc().height + 8 - 1) / 8, 4);
	CommandListRef->endMarker();
}

void CACAOPass::BlurSSAO(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Blur SSAO", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Blur SSAO");
	int blurPassCount = 4;
	const uint32_t w = 4 * 16 - 2 * blurPassCount;
	const uint32_t h = 3 * 16 - 2 * blurPassCount;
	const uint32_t dispatchWidth = (targets->SSAOBufferPong->getDesc().width + w - 1) / w;
	const uint32_t dispatchHeight = (targets->SSAOBufferPong->getDesc().height + h - 1) / h;
	const uint32_t dispatchDepth = 4;

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(6, targets->SSAOBufferPing),
		nvrhi::BindingSetItem::Texture_UAV(6, targets->SSAOBufferPong),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Mirror]),
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOBlur.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(dispatchWidth, dispatchHeight, dispatchDepth);
	CommandListRef->endMarker();
}

void CACAOPass::ApplySSAO(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Apply SSAO", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Apply SSAO");
	int blurPassCount = 4;
	const uint32_t w = 8;
	const uint32_t h = 8;
	const uint32_t dispatchWidth = (targets->AONormalized->getDesc().width + w - 1) / w;
	const uint32_t dispatchHeight = (targets->AONormalized->getDesc().height + h - 1) / h;
	const uint32_t dispatchDepth = 1;

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(6, targets->SSAOBufferPong),
		nvrhi::BindingSetItem::Texture_UAV(11, targets->AONormalized),
		nvrhi::BindingSetItem::Sampler(2, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp]),
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOApply.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(dispatchWidth, dispatchHeight, dispatchDepth);
	CommandListRef->endMarker();
}
