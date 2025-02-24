#include "ragdollpch.h"
#include "DeferredLightPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/GPUScene.h"
#include "Ragdoll/DirectXDevice.h"

void DeferredLightPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void DeferredLightPass::LightPass(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, ragdoll::FGPUScene* GPUScene)
{
	RD_SCOPE(Render, LightPass);
	RD_GPU_SCOPE("LightPass", CommandListRef);
	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DeferredLight CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(targets->SceneColor);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.LightDiffuseColor = sceneInfo.LightDiffuseColor;
	CBuffer.SceneAmbientColor = sceneInfo.SceneAmbientColor;
	CBuffer.LightDirection = sceneInfo.LightDirection;
	CBuffer.CameraPosition = sceneInfo.MainCameraPosition;
	CBuffer.LightIntensity = sceneInfo.LightIntensity;
	CBuffer.PointLightCount = GPUScene->PointLightCount;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->GBufferAlbedo),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->GBufferNormal),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->GBufferRM),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->AONormalized),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Texture_SRV(5, targets->ShadowMask),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(6, GPUScene->PointLightBufferHandle)
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("DeferredLight.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->beginMarker("Light Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);
	
	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}

void DeferredLightPass::LightGridPass(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, ragdoll::FGPUScene* GPUScene)
{
	RD_SCOPE(Render, LightPass);
	RD_GPU_SCOPE("LightPass", CommandListRef);
	CommandListRef->beginMarker("Light Pass");

	GPUScene->CullLightGrid(sceneInfo, CommandListRef, targets);

	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DeferredLight CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(targets->SceneColor);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.LightDiffuseColor = sceneInfo.LightDiffuseColor;
	CBuffer.SceneAmbientColor = sceneInfo.SceneAmbientColor;
	CBuffer.LightDirection = sceneInfo.LightDirection;
	CBuffer.CameraPosition = sceneInfo.MainCameraPosition;
	CBuffer.LightIntensity = sceneInfo.LightIntensity;
	CBuffer.PointLightCount = GPUScene->PointLightCount;
	CBuffer.View = sceneInfo.MainCameraView;
	CBuffer.ScreenSize = { (float)sceneInfo.RenderWidth, (float)sceneInfo.RenderHeight };
	CBuffer.GridSize = { (float)GPUScene->TileCountX, (float)GPUScene->TileCountY };
	CBuffer.FieldsNeeded = GPUScene->FieldsNeeded;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->GBufferAlbedo),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->GBufferNormal),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->GBufferRM),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->AONormalized),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Texture_SRV(5, targets->ShadowMask),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(6, GPUScene->PointLightBufferHandle),
		//nvrhi::BindingSetItem::StructuredBuffer_SRV(7, GPUScene->LightBitFieldsBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(8, GPUScene->DepthSliceBoundsViewspaceBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(9, GPUScene->LightGridBoundingBoxBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(0, GPUScene->LightBitFieldsBufferHandle)
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("DeferredLightGrid.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
