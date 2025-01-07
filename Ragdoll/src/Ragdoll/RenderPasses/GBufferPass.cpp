#include "ragdollpch.h"
#include "GbufferPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/GPUScene.h"
#include "Ragdoll/DirectXDevice.h"

void GBufferPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void GBufferPass::DrawAllInstances(
	ragdoll::FGPUScene* GPUScene,
	uint32_t ProxyCount,
	const ragdoll::SceneInformation& sceneInfo,
	const ragdoll::DebugInfo& debugInfo,
	ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, GBufferPass);
	RD_GPU_SCOPE("GBufferPass", CommandListRef);
	CommandListRef->beginMarker("Instance Draws");
	//new gpu scene stuff
	nvrhi::BufferHandle CountBuffer;
	if(debugInfo.bFreezeFrustumCulling)
		CountBuffer = GPUScene->FrustumCull(CommandListRef, debugInfo.FrozenProjection, debugInfo.FrozenView, ProxyCount, true);
	else
		CountBuffer = GPUScene->FrustumCull(CommandListRef, sceneInfo.InfiniteReverseZProj, sceneInfo.MainCameraView, ProxyCount, true);
	//occlusion cull phase 1

	//draw phase 1 onto gbuffer
	
	//build hzb
	GPUScene->BuildHZB(CommandListRef, targets);
	//occlusion cull phase 2
	
	//draw phase 2 onto gbuffer
	
	MICROPROFILE_SCOPEI("Render", "Draw All Instances", MP_BLUEVIOLET);
	//create a constant buffer here
	nvrhi::BufferDesc ConstantBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "GBufferPass CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(ConstantBufferDesc);

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, GPUScene->InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(1, GPUScene->MaterialBuffer),
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	//get the binding layout
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	//get the pso
	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("GBufferShader.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("GBufferShader.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	PipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	PipelineDesc.inputLayout = AssetManager::GetInstance()->InstancedInputLayoutHandle;

	//create and set the state
	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(targets->GBufferAlbedo)
		.addColorAttachment(targets->GBufferNormal)
		.addColorAttachment(targets->GBufferRM)
		.addColorAttachment(targets->VelocityBuffer)
		.setDepthAttachment(targets->CurrDepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO, 0 },
		{ GPUScene->InstanceIdBuffer, 1 }
	};
	state.indirectParams = GPUScene->IndirectDrawArgsBuffer;
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;
	CBuffer.ViewProjWithAA = sceneInfo.MainCameraViewProjWithAA;
	CBuffer.PrevViewProj = sceneInfo.PrevMainCameraViewProj;
	CBuffer.RenderResolution = Vector2((float)sceneInfo.RenderWidth, (float)sceneInfo.RenderHeight);
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));

	CommandListRef->setGraphicsState(state);
	CommandListRef->drawIndexedIndirect(0, CountBuffer, AssetManager::GetInstance()->VertexBufferInfos.size());

	//build hzb for next frame
	GPUScene->BuildHZB(CommandListRef, targets);

	CommandListRef->endMarker();
}
