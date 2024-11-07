#include "ragdollpch.h"
#include "IntelTAAPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void IntelTAAPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void IntelTAAPass::TemporalAA(ragdoll::SceneRenderTargets* targets, Vector2 jitter)
{
	RD_SCOPE(Render, TemporalAA);
	RD_GPU_SCOPE("TemporalAA", CommandListRef);
	static uint32_t FrameIndex{};
	uint32_t FrameIndexMod2 = FrameIndex % 2;
	Vector2 resolution = Vector2((float)targets->SceneColor->getDesc().width, (float)targets->SceneColor->getDesc().height);

	nvrhi::TextureHandle SrcTemporal, DesTemporal;
	SrcTemporal = !FrameIndexMod2 ? targets->TemporalColor0 : targets->TemporalColor1;
	DesTemporal = FrameIndexMod2 ? targets->TemporalColor0 : targets->TemporalColor1;
	CommandListRef->beginMarker("TemporalAA");
	{
		CommandListRef->beginMarker("Resolve");
		ConstantBuffer.Resolution.x = resolution.x;
		ConstantBuffer.Resolution.y = resolution.y;
		ConstantBuffer.Resolution.z = 1.f / resolution.x;
		ConstantBuffer.Resolution.w = 1.f / resolution.y;
		ConstantBuffer.JitterX = jitter.x;
		ConstantBuffer.JitterY = jitter.y;

		nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FTAAResolve), "Temporal AA CBuffer", 1);
		nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
		CommandListRef->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FTAAResolve));

		nvrhi::BindingSetDesc setDesc;
		setDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
			nvrhi::BindingSetItem::Texture_SRV(0, targets->VelocityBuffer),
			nvrhi::BindingSetItem::Texture_SRV(1, targets->FinalColor),
			nvrhi::BindingSetItem::Texture_SRV(2, SrcTemporal),
			nvrhi::BindingSetItem::Texture_SRV(3, targets->CurrDepthBuffer),
			nvrhi::BindingSetItem::Texture_SRV(4, targets->PrevDepthBuffer),
			nvrhi::BindingSetItem::Texture_UAV(0, DesTemporal),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp]),
			nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp]),
		};
		nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
		nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

		nvrhi::ComputePipelineDesc PipelineDesc;
		PipelineDesc.bindingLayouts = { layoutHandle };
		nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("TAAResolve.cs.cso");
		PipelineDesc.CS = shader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
		state.bindings = { setHandle };
		CommandListRef->setComputeState(state);
		CommandListRef->dispatch((targets->SceneColor->getDesc().width + 8 - 1) / 8, (targets->SceneColor->getDesc().height + 8 - 1) / 8, 1);
		CommandListRef->endMarker();
	}

	{
		CommandListRef->beginMarker("Sharpen");
		float sharpness = 0.5f;
		InlineConstants.x = 1.0f + sharpness;
		InlineConstants.y = 0.25f * sharpness;
		bool ITAA_Enable = true;
		InlineConstants.z = ITAA_Enable ? 0.f : 1.f;
		nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Vector3), "Temporal Sharpen CBuffer", 1);
		nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
		CommandListRef->writeBuffer(ConstantBufferHandle, &InlineConstants, sizeof(Vector3));

		nvrhi::BindingSetDesc setDesc;
		setDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::Texture_SRV(0, DesTemporal),
			nvrhi::BindingSetItem::Texture_UAV(0, targets->FinalColor),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp]),
			nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp]),
		};
		nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
		nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle); 
		
		nvrhi::ComputePipelineDesc PipelineDesc;
		PipelineDesc.bindingLayouts = { layoutHandle };
		nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("TAASharpen.cs.cso");
		PipelineDesc.CS = shader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
		state.bindings = { setHandle };
		CommandListRef->setComputeState(state);
		CommandListRef->dispatch((targets->SceneColor->getDesc().width + 8 - 1) / 8, (targets->SceneColor->getDesc().height + 8 - 1) / 8, 1);
		CommandListRef->endMarker();
	}
	CommandListRef->endMarker();

	FrameIndex++;
}
