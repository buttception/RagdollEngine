 #include "ragdollpch.h"
#include "GbufferPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/GPUScene.h"
#include "Ragdoll/DirectXDevice.h"

#define INFINITE_Z_ENABLED 1
#define PREVIOUS_FRAME_ENABLED 1 << 1
#define IS_PHASE_1 1 << 2
#define ALPHA_TEST_ENABLED 1 << 3
#define CULL_ALL 1 << 4
#define ENABLE_INSTANCE_FRUSTUM_CULL 1 << 5
#define ENABLE_INSTANCE_OCCLUSION_CULL 1 << 6
#define ENABLE_AS_FRUSTUM_CULL 1 << 7
#define ENABLE_AS_CONE_CULL 1 << 8
#define ENABLE_AS_OCCLUSION_CULL 1 << 9
#define ENABLE_MESHLET_COLOR 1 << 10
#define ENABLE_INSTANCE_COLOR 1 << 11

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

	CountBufferDesc.debugName = "MeshletFrustumCulledCountBuffer";
	MeshletFrustumCulledCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "MeshletDegenerateConeCulledCountbuffer";
	MeshletDegenerateConeCulledCountbuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "MeshletOcclusionCulledPhase1CountBuffer";
	MeshletOcclusionCulledPhase1CountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "MeshletOcclusionCulledPhase2CountBuffer";
	MeshletOcclusionCulledPhase2CountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
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

void GBufferPass::BuildMeshletParameters(ragdoll::FGPUScene* GPUScene, const ragdoll::SceneInformation& sceneInfo, const ragdoll::DebugInfo& debugInfo, uint32_t ProxyCount, nvrhi::BufferHandle InstanceIdBuffer, nvrhi::BufferHandle InstanceCountBuffer)
{
	RD_SCOPE(Render, BuildMeshletParameters);
	{
		CommandListRef->beginMarker("ResetMeshletArgs");
		//create the binding set
		nvrhi::BindingSetDesc BindingSetDesc;
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::StructuredBuffer_UAV(1, GPUScene->IndirectMeshletArgsBuffer),
		};
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

		//pipeline descs
		nvrhi::ComputePipelineDesc CullingPipelineDesc;
		CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
		nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("ResetMeshlet.cs.cso");
		CullingPipelineDesc.CS = CullingShader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
		state.bindings = { BindingSetHandle };
		CommandListRef->setComputeState(state);
		CommandListRef->dispatch(1, 1, 1);
		CommandListRef->endMarker();
	}
	{
		CommandListRef->beginMarker("BuildMeshletParameters");
		nvrhi::BindingSetDesc BindingSetDesc;
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, GPUScene->InstanceBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(6, GPUScene->MeshBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(9, InstanceCountBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(10, InstanceIdBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(0, GPUScene->AmplificationGroupInfoBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(1, GPUScene->IndirectMeshletArgsBuffer),
		};
		//get the binding layout
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);
		//get the pso
		nvrhi::ComputePipelineDesc PipelineDesc;
		PipelineDesc.addBindingLayout(BindingLayoutHandle);
		PipelineDesc.CS = AssetManager::GetInstance()->GetShader("MeshletBuildCommandParameters.cs.cso");
		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
		state.bindings = { BindingSetHandle };
		CommandListRef->setComputeState(state);
		CommandListRef->dispatch((ProxyCount + 63) / 64);
		CommandListRef->endMarker();
	}
}

void GBufferPass::DispatchMeshlets(
	ragdoll::FGPUScene* GPUScene,
	const ragdoll::SceneInformation& sceneInfo,
	const ragdoll::DebugInfo& debugInfo,
	ragdoll::SceneRenderTargets* targets,
	Matrix ViewMatrix,
	Matrix ProjectionMatrix,
	Matrix TestedViewMatrix,
	Matrix TestedProjectionMatrix,
	Vector3 CameraPosition,
	bool IsPhase1
)
{
	RD_SCOPE(Render, MeshletGBuffer);
	//constant buffer used by the AS
	struct FConstantBuffer
	{
		Matrix ViewMatrix{};
		Matrix ProjectionMatrix{};
		Vector4 FrustumPlanes[6]{};
		Vector3 CameraPosition;
		uint32_t MipBaseWidth;
		uint32_t MipBaseHeight;
		uint32_t MipLevels;
		uint32_t Flags{};
	} ConstantBuffer;
	GPUScene->ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, ProjectionMatrix, ViewMatrix);
	ConstantBuffer.ViewMatrix = TestedViewMatrix;
	ConstantBuffer.ProjectionMatrix = TestedProjectionMatrix;
	ConstantBuffer.Flags |= IS_PHASE_1 * IsPhase1;
	ConstantBuffer.Flags |= INFINITE_Z_ENABLED;
	ConstantBuffer.Flags |= ENABLE_INSTANCE_FRUSTUM_CULL * sceneInfo.bEnableInstanceFrustumCull;
	ConstantBuffer.Flags |= ENABLE_AS_FRUSTUM_CULL * sceneInfo.bEnableMeshletFrustumCulling;
	ConstantBuffer.Flags |= ENABLE_AS_CONE_CULL * sceneInfo.bEnableMeshletConeCulling;
	ConstantBuffer.Flags |= ENABLE_AS_OCCLUSION_CULL * sceneInfo.bEnableMeshletOcclusionCulling;
	ConstantBuffer.Flags |= ENABLE_MESHLET_COLOR * sceneInfo.bEnableMeshletColors;
	ConstantBuffer.Flags |= ENABLE_INSTANCE_COLOR * sceneInfo.bEnableInstanceColors;
	ConstantBuffer.MipBaseWidth = targets->HZBMips->getDesc().width;
	ConstantBuffer.MipBaseHeight = targets->HZBMips->getDesc().height;
	ConstantBuffer.MipLevels = targets->HZBMips->getDesc().mipLevels;
	ConstantBuffer.CameraPosition = CameraPosition;

	nvrhi::BufferHandle ConstantBufferHandle0 = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "InstanceCull ConstantBuffer", 1));
	CommandListRef->writeBuffer(ConstantBufferHandle0, &ConstantBuffer, sizeof(FConstantBuffer));

	//update values for the const buffer used by the MS
	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;
	CBuffer.ViewProjWithAA = sceneInfo.MainCameraViewProjWithJitter;
	CBuffer.PrevViewProj = sceneInfo.PrevMainCameraViewProj;
	CBuffer.RenderResolution = Vector2((float)sceneInfo.RenderWidth, (float)sceneInfo.RenderHeight);
	nvrhi::BufferDesc ConstantBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(struct ConstantBuffer), "GBufferPass CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle1 = DirectXDevice::GetNativeDevice()->createBuffer(ConstantBufferDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle1, &CBuffer, sizeof(struct ConstantBuffer));

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle0),
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle1),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, GPUScene->InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(1, GPUScene->MaterialBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(2, AssetManager::GetInstance()->VBO),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(3, AssetManager::GetInstance()->MeshletBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(4, AssetManager::GetInstance()->MeshletVertexBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(5, AssetManager::GetInstance()->MeshletPrimitiveBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(6, GPUScene->MeshBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(7, GPUScene->AmplificationGroupInfoBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(8, AssetManager::GetInstance()->MeshletBoundingSphereBuffer),
		nvrhi::BindingSetItem::Texture_SRV(11, targets->HZBMips, nvrhi::Format::D32, nvrhi::AllSubresources),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(6, GPUScene->MeshletOcclusionCulledPhase1CountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(7, GPUScene->MeshletOcclusionCulledPhase2CountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(8, GPUScene->MeshletFrustumCulledCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(9, GPUScene->MeshletDegenerateConeCountBuffer),

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
	nvrhi::ShaderHandle AmpShader = AssetManager::GetInstance()->GetShader("Meshlet.as.cso");
	nvrhi::ShaderHandle MeshShader = AssetManager::GetInstance()->GetShader("Meshlet.ms.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("Meshlet.ps.cso");
	PipelineDesc.setAmplificationShader(AmpShader);
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
	state.indirectParams = GPUScene->IndirectMeshletArgsBuffer;

	CommandListRef->beginMarker("Meshlet Pass");
	CommandListRef->setMeshletState(state);
	CommandListRef->dispatchMeshIndirect(0, nullptr, 1);
	CommandListRef->endMarker();
}

void GBufferPass::DrawMeshlets(
	ragdoll::FGPUScene* GPUScene, 
	uint32_t ProxyCount,
	const ragdoll::SceneInformation& sceneInfo,
	const ragdoll::DebugInfo& debugInfo,
	ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, MeshletGBuffer);
	RD_GPU_SCOPE("MeshletGBufferPass", CommandListRef);
	CommandListRef->beginMarker("MeshletGBufferPass");

	Matrix ViewMatrix;
	Matrix ProjectionMatrix;
	Matrix ViewProjectionMatrix;
	Matrix PrevViewMatrix;
	Matrix PrevProjectionMatrix;
	Vector3 CameraPosition;
	if (debugInfo.bFreezeFrustumCulling)
	{
		PrevViewMatrix = ViewMatrix = debugInfo.FrozenView;
		PrevProjectionMatrix = ProjectionMatrix = debugInfo.FrozenProjection;
		ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
		CameraPosition = debugInfo.FrozenCameraPosition;
	}
	else
	{
		ViewMatrix = sceneInfo.MainCameraView;
		ProjectionMatrix = sceneInfo.MainCameraProjWithJitter;
		ViewProjectionMatrix = sceneInfo.MainCameraViewProjWithJitter;
		PrevViewMatrix = sceneInfo.PrevMainCameraView;
		PrevProjectionMatrix = sceneInfo.PrevMainCameraProjWithJitter;
		CameraPosition = sceneInfo.MainCameraPosition;
	}
	//frustum cull the instances
	nvrhi::BufferHandle CountBuffer = GPUScene->FrustumCull(CommandListRef, ProjectionMatrix, ViewMatrix, ProxyCount, true);
	CommandListRef->copyBuffer(PassedFrustumTestCountBuffer, 0, CountBuffer, 0, sizeof(uint32_t));
	//occlusion cull phase 1 the instances
	nvrhi::BufferHandle NonOccludedCountBuffer;
	nvrhi::BufferHandle OccludedCountBuffer;
	GPUScene->OcclusionCullPhase1(CommandListRef, targets, PrevViewMatrix, PrevProjectionMatrix, CountBuffer, NonOccludedCountBuffer, OccludedCountBuffer, ProxyCount);
	CommandListRef->copyBuffer(Phase1NonOccludedCountBuffer, 0, NonOccludedCountBuffer, 0, sizeof(uint32_t));
	//build the indirect draw arg and buffers needed for the draw call
	BuildMeshletParameters(GPUScene, sceneInfo, debugInfo, ProxyCount, GPUScene->InstanceIdBuffer, NonOccludedCountBuffer);
	//draw the instances that passed the frustum and occlusion cull
	DispatchMeshlets(GPUScene, sceneInfo, debugInfo, targets, ViewMatrix, ProjectionMatrix, PrevViewMatrix, PrevProjectionMatrix, CameraPosition, true);
	//build the HZB
	if (!debugInfo.bFreezeFrustumCulling)
		GPUScene->BuildHZB(CommandListRef, targets);
	//occlusion cull phase 2 the instances
	CountBuffer = GPUScene->OcclusionCullPhase2(CommandListRef, targets, ViewMatrix, ProjectionMatrix, OccludedCountBuffer, ProxyCount);
	CommandListRef->copyBuffer(Phase2NonOccludedCountBuffer, 0, CountBuffer, 0, sizeof(uint32_t));
	//build the indirect draw for the meshlet
	BuildMeshletParameters(GPUScene, sceneInfo, debugInfo, ProxyCount, GPUScene->OccludedInstanceIdBuffer, CountBuffer);
	//draw the instances that passed the frustum but failed the phase 1 occlusion cull
	DispatchMeshlets(GPUScene, sceneInfo, debugInfo, targets, ViewMatrix, ProjectionMatrix, ViewMatrix, ProjectionMatrix, CameraPosition, false);
	//build hzb for the next frame
	if (!debugInfo.bFreezeFrustumCulling)
		GPUScene->BuildHZB(CommandListRef, targets);

	CommandListRef->copyBuffer(MeshletFrustumCulledCountBuffer, 0, GPUScene->MeshletFrustumCulledCountBuffer, 0, sizeof(uint32_t));
	CommandListRef->copyBuffer(MeshletDegenerateConeCulledCountbuffer, 0, GPUScene->MeshletDegenerateConeCountBuffer, 0, sizeof(uint32_t));
	CommandListRef->copyBuffer(MeshletOcclusionCulledPhase1CountBuffer, 0, GPUScene->MeshletOcclusionCulledPhase1CountBuffer, 0, sizeof(uint32_t));
	CommandListRef->copyBuffer(MeshletOcclusionCulledPhase2CountBuffer, 0, GPUScene->MeshletOcclusionCulledPhase2CountBuffer, 0, sizeof(uint32_t));
	//reset debug count buffers
	CommandListRef->clearBufferUInt(GPUScene->MeshletFrustumCulledCountBuffer, 0);
	CommandListRef->clearBufferUInt(GPUScene->MeshletDegenerateConeCountBuffer, 0);
	CommandListRef->clearBufferUInt(GPUScene->MeshletOcclusionCulledPhase1CountBuffer, 0);
	CommandListRef->clearBufferUInt(GPUScene->MeshletOcclusionCulledPhase2CountBuffer, 0);

	CommandListRef->endMarker();
}
