#include "ragdollpch.h"
#include "XeGTAOPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

#define GET_BATCH_DIMENSION_DEPTH(x, y) (x + 16-1) / 16, (y + 16-1) / 16, 1
#define GET_BATCH_DIMENSION_COMPOSE(x, y) (x + 8-1) / 8, (y + 8-1) / 8, 1
#define GET_BATCH_DIMENSION_AO(x, y) (x + XE_GTAO_NUMTHREADS_X-1) / XE_GTAO_NUMTHREADS_X, (y + XE_GTAO_NUMTHREADS_Y-1) / XE_GTAO_NUMTHREADS_Y, 1
#define GET_BATCH_DIMENSION_NOISE(x,y) (x + (XE_GTAO_NUMTHREADS_X*2)-1) / (XE_GTAO_NUMTHREADS_X*2), (y + XE_GTAO_NUMTHREADS_Y-1) / XE_GTAO_NUMTHREADS_Y, 1

void XeGTAOPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void XeGTAOPass::UpdateConstants(const uint32_t width, const uint32_t height, const Matrix& projMatrix, const ragdoll::SceneInformation& sceneInfo)
{
	static uint32_t framecounter = 0;
	XeGTAO::GTAOUpdateConstants(CBuffer, width, height, Settings, &projMatrix._11, true, framecounter);
	if(sceneInfo.bEnableXeGTAONoise)
		framecounter++;
}

void XeGTAOPass::GenerateAO(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, XeGTAO);
	RD_GPU_SCOPE("XeGTAO", CommandListRef);

	CommandListRef->beginMarker("XeGTAO");
	UpdateConstants(targets->CurrDepthBuffer->getDesc().width, targets->CurrDepthBuffer->getDesc().height, sceneInfo.MainCameraProjWithJitter, sceneInfo);
	//cbuffer shared amongst all
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(XeGTAO::GTAOConstants), "XeGTAO CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(XeGTAO::GTAOConstants));

	CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ViewMatrix CBuffer", 1);
	OtherBuffer.viewMatrix = sceneInfo.MainCameraView * DirectX::XMMatrixScaling(1.f, 1.f, -1.f);
	OtherBuffer.prevViewProjMatrix = sceneInfo.PrevMainCameraViewProjWithJitter;
	OtherBuffer.InvViewProjMatrix = sceneInfo.MainCameraViewProjWithJitter.Invert();
	nvrhi::BufferHandle MatrixHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(MatrixHandle, &OtherBuffer, sizeof(ConstantBuffer));
	{
		//MICROPROFILE_SCOPEGPUI("Prepare Depth Mips", MP_LIGHTYELLOW1);
		GenerateDepthMips(sceneInfo, ConstantBufferHandle, MatrixHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Generate SSAO", MP_LIGHTYELLOW1);
		MainPass(sceneInfo, ConstantBufferHandle, MatrixHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Denoise SSAO", MP_LIGHTYELLOW1);
		Denoise(sceneInfo, ConstantBufferHandle, MatrixHandle, targets);
	}
	{
		//MICROPROFILE_SCOPEGPUI("Apply SSAO", MP_LIGHTYELLOW1);
		Compose(sceneInfo, ConstantBufferHandle, MatrixHandle, targets);
	}

	CommandListRef->endMarker();
}

void XeGTAOPass::GenerateDepthMips(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Depth Mip", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Depth Mip");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->DepthMip, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{0,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->DepthMip, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{1,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(2, targets->DepthMip, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{2,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(3, targets->DepthMip, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{3,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(4, targets->DepthMip, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{4,1,0,1}),
		nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CSPrefilterDepths16x16.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(GET_BATCH_DIMENSION_DEPTH(targets->DepthMip->getDesc().width, targets->DepthMip->getDesc().height));
	CommandListRef->endMarker();
}

void XeGTAOPass::MainPass(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Main Pass", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Main Pass");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->DepthMip),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->GBufferNormal),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->AOTerm),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->EdgeMap),
		nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CSGTAOMedium.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(GET_BATCH_DIMENSION_AO(targets->DepthMip->getDesc().width, targets->DepthMip->getDesc().height));
	CommandListRef->endMarker();
}

void XeGTAOPass::Denoise(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Denoise", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Denoise");

	const int passCount = std::max(1, Settings.DenoisePasses); // even without denoising we have to run a single last pass to output correct term into the external output texture
	nvrhi::TextureHandle AOPing{ targets->AOTerm }, AOPong{ targets->FinalAOTerm };
	for (int i = 0; i < passCount; i++)
	{
		const bool lastPass = i == passCount - 1;

		std::string shaderName = (lastPass) ? ("CSDenoiseLastPass.cs.cso") : ("CSDenoisePass.cs.cso");

		nvrhi::BindingSetDesc setDesc;
		setDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
			nvrhi::BindingSetItem::Texture_SRV(0, AOPing),
			nvrhi::BindingSetItem::Texture_SRV(1, targets->EdgeMap),
			nvrhi::BindingSetItem::Texture_UAV(0, AOPong),
			nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
		};
		nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
		nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

		std::swap(AOPing, AOPong);      // ping becomes pong, pong becomes ping.

		nvrhi::ComputePipelineDesc PipelineDesc;
		PipelineDesc.bindingLayouts = { layoutHandle };
		nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader(shaderName);
		PipelineDesc.CS = shader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
		state.bindings = { setHandle };
		CommandListRef->setComputeState(state);
		CommandListRef->dispatch(GET_BATCH_DIMENSION_NOISE(targets->DepthMip->getDesc().width, targets->DepthMip->getDesc().height));
	}
	CommandListRef->endMarker();
}

void XeGTAOPass::Compose(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix, ragdoll::SceneRenderTargets* targets)
{
	MICROPROFILE_SCOPEI("Render", "Compose", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Compose");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->FinalAOTerm ),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->VelocityBuffer),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->AOTermAccumulation),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->AONormalized),
		nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CSComposeAO.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(GET_BATCH_DIMENSION_COMPOSE(targets->DepthMip->getDesc().width, targets->DepthMip->getDesc().height));
	CommandListRef->copyTexture(targets->AOTermAccumulation, nvrhi::TextureSlice().resolve(targets->AOTermAccumulation->getDesc()), targets->AONormalized, nvrhi::TextureSlice().resolve(targets->AONormalized->getDesc()));

	CommandListRef->endMarker();
}
