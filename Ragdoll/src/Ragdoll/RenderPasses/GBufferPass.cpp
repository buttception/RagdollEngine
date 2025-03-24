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

	nvrhi::BufferDesc CountBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t), "CountBuffer");
	CountBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
	CountBufferDesc.keepInitialState = true;
	CountBufferDesc.cpuAccess = nvrhi::CpuAccessMode::Read;
	PassedFrustumTestCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "Phase1NonOccludedCountBuffer";
	Phase1NonOccludedCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "Phase2NonOccludedCountBuffer";
	Phase2NonOccludedCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
}

void GBufferPass::Draw(ragdoll::FGPUScene* GPUScene, uint32_t ProxyCount, const ragdoll::SceneInformation& sceneInfo, const ragdoll::DebugInfo& debugInfo, ragdoll::SceneRenderTargets* targets, bool isOcclusionCullingEnabled)
{
	RD_SCOPE(Render, GBufferPass);
	RD_GPU_SCOPE("GBufferPass", CommandListRef);
	CommandListRef->beginMarker("GBufferPass");

	nvrhi::BufferHandle CountBuffer;
	nvrhi::BufferHandle NotOccludedCountBuffer;
	nvrhi::BufferHandle OccludedCountBuffer;
	Matrix ViewMatrix;
	Matrix ProjectionMatrix;
	Matrix ViewProjectionMatrix;
	Matrix PrevViewMatrix;
	Matrix PrevProjectionMatrix;
	if (debugInfo.bFreezeFrustumCulling)
	{
		PrevViewMatrix = ViewMatrix = debugInfo.FrozenView;
		PrevProjectionMatrix = ProjectionMatrix = debugInfo.FrozenProjection;
		ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
	}
	else
	{
		ViewMatrix = sceneInfo.MainCameraView;
		ProjectionMatrix = sceneInfo.MainCameraProjWithJitter;
		ViewProjectionMatrix = sceneInfo.MainCameraViewProjWithJitter;
		PrevViewMatrix = sceneInfo.PrevMainCameraView;
		PrevProjectionMatrix = sceneInfo.PrevMainCameraProjWithJitter;
	}
	if (isOcclusionCullingEnabled)
	{
		//cull only opaques to draw opaque objects
		CountBuffer = GPUScene->FrustumCull(CommandListRef, ProjectionMatrix, ViewMatrix, ProxyCount, true, 1);
		CommandListRef->copyBuffer(PassedFrustumTestCountBuffer, 0, CountBuffer, 0, sizeof(uint32_t));
		//occlusion cull phase 1
		GPUScene->OcclusionCullPhase1(CommandListRef, targets, PrevViewMatrix, PrevProjectionMatrix, CountBuffer, NotOccludedCountBuffer, OccludedCountBuffer, ProxyCount);
		CommandListRef->copyBuffer(Phase1NonOccludedCountBuffer, 0, NotOccludedCountBuffer, 0, sizeof(uint32_t));
		//draw phase 1 onto gbuffer that was not occluded
		DrawAllInstances(GPUScene, NotOccludedCountBuffer, ProxyCount, sceneInfo, debugInfo, targets);
		//build hzb if not frozen
		if(!debugInfo.bFreezeFrustumCulling)
			GPUScene->BuildHZB(CommandListRef, targets);
		//occlusion cull phase 2, cull occluded objexcts
		CountBuffer = GPUScene->OcclusionCullPhase2(CommandListRef, targets, ViewMatrix, ProjectionMatrix, OccludedCountBuffer, ProxyCount);
		CommandListRef->copyBuffer(Phase2NonOccludedCountBuffer, 0, CountBuffer, 0, sizeof(uint32_t));
		//draw phase 2 onto gbuffer
		DrawAllInstances(GPUScene, CountBuffer, ProxyCount, sceneInfo, debugInfo, targets);
		//build hzb for next frame
		if (!debugInfo.bFreezeFrustumCulling)
			GPUScene->BuildHZB(CommandListRef, targets);
		//cull all the alpha objects
		CountBuffer = GPUScene->FrustumCull(CommandListRef, ProjectionMatrix, ViewMatrix, ProxyCount, true, 2);
		CommandListRef->copyBuffer(PassedFrustumTestCountBuffer, 0, CountBuffer, 0, sizeof(uint32_t));
		//phase 1 cull and can draw
		GPUScene->OcclusionCullPhase1(CommandListRef, targets, PrevViewMatrix, PrevProjectionMatrix, CountBuffer, NotOccludedCountBuffer, OccludedCountBuffer, ProxyCount);
		CommandListRef->copyBuffer(Phase1NonOccludedCountBuffer, 0, NotOccludedCountBuffer, 0, sizeof(uint32_t));
		//draw phase 1 onto gbuffer that was not occluded
		DrawAllInstances(GPUScene, NotOccludedCountBuffer, ProxyCount, sceneInfo, debugInfo, targets, false);
		//no hzb rebuild as next frame should not cull against transparent/transulucent objects
	}
	else
	{
		//draw all opaque
		CountBuffer = GPUScene->FrustumCull(CommandListRef, ProjectionMatrix, ViewMatrix, ProxyCount, true, 1);
		DrawAllInstances(GPUScene, CountBuffer, ProxyCount, sceneInfo, debugInfo, targets);
		//cull all alpha objects
		CountBuffer = GPUScene->FrustumCull(CommandListRef, ProjectionMatrix, ViewMatrix, ProxyCount, true, 2);
		DrawAllInstances(GPUScene, CountBuffer, ProxyCount, sceneInfo, debugInfo, targets, false);
	}
	CommandListRef->endMarker();
}

void GBufferPass::DrawAllInstances(
	ragdoll::FGPUScene* GPUScene,
	nvrhi::BufferHandle CountBuffer,
	uint32_t ProxyCount,
	const ragdoll::SceneInformation& sceneInfo,
	const ragdoll::DebugInfo& debugInfo,
	ragdoll::SceneRenderTargets* targets,
	bool opaquePass)
{
	RD_SCOPE(Render, Draw All Instances)
	CommandListRef->beginMarker("Instance Draws");
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
	nvrhi::ShaderHandle PixelShader;
	if (opaquePass)
	{
		PixelShader = AssetManager::GetInstance()->GetShader("GBufferShaderOpaque.ps.cso");
		PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
	}
	else
	{
		PixelShader = AssetManager::GetInstance()->GetShader("GBufferShaderAlpha.ps.cso");
		PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
		nvrhi::BlendState::RenderTarget& BlendStateTarget = PipelineDesc.renderState.blendState.targets[0];
		BlendStateTarget.blendEnable = true;
		BlendStateTarget.srcBlend = nvrhi::BlendFactor::SrcAlpha;
		BlendStateTarget.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
	}
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	PipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	PipelineDesc.renderState.rasterState.frontCounterClockwise = true;
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
	CBuffer.ViewProjWithAA = sceneInfo.MainCameraViewProjWithJitter;
	CBuffer.PrevViewProj = sceneInfo.PrevMainCameraViewProj;
	CBuffer.RenderResolution = Vector2((float)sceneInfo.RenderWidth, (float)sceneInfo.RenderHeight);
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));

	CommandListRef->setGraphicsState(state);
	CommandListRef->drawIndexedIndirect(0, CountBuffer, ProxyCount);

	CommandListRef->endMarker();
}

void GBufferPass::DrawMeshlets(
	ragdoll::FGPUScene* GPUScene, 
	const ragdoll::SceneInformation& sceneInfo,
	const ragdoll::DebugInfo& debugInfo,
	ragdoll::SceneRenderTargets* targets)
{
	//hardcoded to draw only the first mesh
	RD_SCOPE(Render, MeshletGBuffer);
	RD_GPU_SCOPE("MeshletGBufferPass", CommandListRef);

	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;
	CBuffer.ViewProjWithAA = sceneInfo.MainCameraViewProjWithJitter;
	CBuffer.PrevViewProj = sceneInfo.PrevMainCameraViewProj;
	CBuffer.RenderResolution = Vector2((float)sceneInfo.RenderWidth, (float)sceneInfo.RenderHeight);
	nvrhi::BufferDesc ConstantBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "GBufferPass CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(ConstantBufferDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, GPUScene->InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(1, GPUScene->MaterialBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(2, AssetManager::GetInstance()->VBO),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(3, AssetManager::GetInstance()->Meshes[0].Submeshes[0].MeshletBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(4, AssetManager::GetInstance()->Meshes[0].Submeshes[0].MeshletVertexBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(5, AssetManager::GetInstance()->Meshes[0].Submeshes[0].MeshletPrimitiveBuffer),

	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	//get the binding layout
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);//create and set the state

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(targets->GBufferAlbedo)
		.addColorAttachment(targets->GBufferNormal)
		.addColorAttachment(targets->GBufferRM)
		.addColorAttachment(targets->VelocityBuffer)
		.setDepthAttachment(targets->CurrDepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);

	nvrhi::MeshletPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	nvrhi::ShaderHandle MeshShader = AssetManager::GetInstance()->GetShader("GBufferShader.ms.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("GBufferShaderOpaque.ps.cso");
	PipelineDesc.setMeshShader(MeshShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	PipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	PipelineDesc.renderState.rasterState.frontCounterClockwise = true;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::MeshletState state;
	state.pipeline = AssetManager::GetInstance()->GetMeshletPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->beginMarker("Meshlet GBuffer Pass");
	CommandListRef->setMeshletState(state);

	CommandListRef->dispatchMesh(AssetManager::GetInstance()->Meshes[0].Submeshes[0].MeshletCount);

	CommandListRef->endMarker();
}
