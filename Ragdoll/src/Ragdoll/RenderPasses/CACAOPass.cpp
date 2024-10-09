#include "ragdollpch.h"
#include "CACAOPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void CACAOPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void CACAOPass::SetDependencies(Textures dependencies)
{
    // Inputs
    DepthBuffer = dependencies.DepthBuffer;
    GBufferNormals = dependencies.GBufferNormals;

    // Outputs
    LoadCounter = dependencies.LoadCounter;
	DeinterleavedDepthMips = dependencies.DeinterleavedDepthMips;
	DeinterleavedNormals = dependencies.DeinterleavedNormals;
    SSAOPong = dependencies.SSAOPong;
    SSAOPing = dependencies.SSAOPing;
    ImportanceMap = dependencies.ImportanceMap;
    ImportanceMapPong = dependencies.ImportanceMapPong;
    GBufferAO = dependencies.GBufferAO;
}

void CACAOPass::GenerateAO(const ragdoll::SceneInformation& sceneInfo)
{
	CommandListRef->beginMarker("CACAO");
	//cbuffer shared amongst all
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DownSample CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	
	CBuffer.DepthBufferDimensions = Vector2(DepthBuffer->getDesc().width, DepthBuffer->getDesc().height);
	CBuffer.DepthBufferInverseDimensions = Vector2(1.f, 1.f) / CBuffer.DepthBufferDimensions;
	CBuffer.DeinterleavedDepthBufferDimensions = Vector2(DeinterleavedDepthMips->getDesc().width, DeinterleavedDepthMips->getDesc().height);
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
	CBuffer.DepthUnpackConsts = Vector2(-0.1, 0);
	CBuffer.CameraTanHalfFOV = Vector2(tanf(sceneInfo.CameraFov), tanf(sceneInfo.CameraFov / sceneInfo.CameraAspect));
	CBuffer.EffectRadius = 1.2;
	CBuffer.EffectShadowStrength = 4.3;
	CBuffer.EffectShadowPow = 1.5;
	CBuffer.EffectShadowClamp = 0.98;
	CBuffer.NormalsUnpackMul = 1.f;
	CBuffer.NormalsUnpackAdd = 0.f;
	CBuffer.NormalsWorldToViewspaceMatrix = Matrix::Identity;

	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	//clear the load counter
	
    //prepare the depth deinterleaved and mips
    PrepareDepth(sceneInfo, ConstantBufferHandle);
	//prepare the normal deinterleaved
	PrepareNormal(sceneInfo, ConstantBufferHandle);

	CommandListRef->endMarker();
}

void CACAOPass::PrepareDepth(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer)
{
	MICROPROFILE_SCOPEI("Render", "Depth Mip", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Depth Mip");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(1, DepthBuffer),
		//nvrhi::BindingSetItem::Texture_SRV(10, LoadCounter),
		//nvrhi::BindingSetItem::Texture_SRV(3, DeinterleavedDepthMips),
		//nvrhi::BindingSetItem::Texture_SRV(4, DeinterleavedNormals),
		//nvrhi::BindingSetItem::Texture_SRV(5, SSAOPing),
		//nvrhi::BindingSetItem::Texture_SRV(6, SSAOPong),
		//nvrhi::BindingSetItem::Texture_SRV(7, ImportanceMap),
		//nvrhi::BindingSetItem::Texture_SRV(8, ImportanceMapPong),
		nvrhi::BindingSetItem::Texture_UAV(12, DeinterleavedDepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{0,1,0,4}),
		nvrhi::BindingSetItem::Texture_UAV(13, DeinterleavedDepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{1,1,0,4}),
		nvrhi::BindingSetItem::Texture_UAV(14, DeinterleavedDepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{2,1,0,4}),
		nvrhi::BindingSetItem::Texture_UAV(15, DeinterleavedDepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{3,1,0,4}),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareDepth.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((DeinterleavedDepthMips->getDesc().width + 8 - 1) / 8, (DeinterleavedDepthMips->getDesc().height + 8 - 1) / 8, 1);
	CommandListRef->endMarker();
}

void CACAOPass::PrepareNormal(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle CBuffer)
{
	MICROPROFILE_SCOPEI("Render", "Depth Normal", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Normal");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, CBuffer),
		nvrhi::BindingSetItem::Texture_SRV(2, GBufferNormals),
		nvrhi::BindingSetItem::Texture_UAV(4, DeinterleavedNormals, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{0,1,0,4}),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CACAOPrepareNormal.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch((DeinterleavedNormals->getDesc().width + 8 - 1) / 8, (DeinterleavedNormals->getDesc().height + 8 - 1) / 8, 1);
	CommandListRef->endMarker();
}
