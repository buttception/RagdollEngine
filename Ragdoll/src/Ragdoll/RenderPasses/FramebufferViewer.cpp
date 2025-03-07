#include "ragdollpch.h"
#include "FramebufferViewer.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"
#include "Ragdoll/GPUScene.h"

void FramebufferViewer::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void FramebufferViewer::DrawTarget(ragdoll::FGPUScene* GPUScene, nvrhi::TextureHandle texture, Vector4 add, Vector4 mul, uint32_t numComp, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, FBView);
	RD_GPU_SCOPE("FBView", CommandListRef);
	//create cbuffer
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "FB View CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Trilinear_Clamp]),
		nvrhi::BindingSetItem::Texture_SRV(0, texture),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("FramebufferView.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::GraphicsState state;
	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	nvrhi::FramebufferHandle RenderTarget = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget);
	state.framebuffer = RenderTarget;
	state.viewport.addViewportAndScissorRect(RenderTarget->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Framebuffer View");
	CBuffer.Add = add;
	CBuffer.Mul = mul;
	CBuffer.ComponentCount = numComp;
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}

void FramebufferViewer::DrawLightGridHitMap(const ragdoll::FGPUScene* GPUScene, const ragdoll::SceneInformation& SceneInfo, const ragdoll::DebugInfo& DebugInfo, ragdoll::SceneRenderTargets* Targets)
{
	RD_SCOPE(Render, FBView);
	RD_GPU_SCOPE("FBView", CommandListRef);
	struct Constants
	{
		float RenderWidth;
		float RenderHeight;
		uint32_t FieldsNeeded;
		uint32_t TileX;
		uint32_t TileY;
		uint32_t TileZ;
		uint32_t LightCount;
		float LerpFactor;
		int32_t SliceStart;
		int32_t SliceEnd;
	} c;
	c.RenderWidth = SceneInfo.RenderWidth;
	c.RenderHeight = SceneInfo.RenderHeight;
	c.FieldsNeeded = GPUScene->FieldsNeeded;
	c.TileX = GPUScene->TileCountX;
	c.TileY = GPUScene->TileCountY;
	c.TileZ = GPUScene->TileCountZ;
	c.LightCount = GPUScene->PointLightCount;
	c.LerpFactor = 0.1f;
	c.SliceStart = DebugInfo.LightGridSliceStartEnd[0];
	c.SliceEnd = DebugInfo.LightGridSliceStartEnd[1];

	//create cbuffer
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Constants), "FB View CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &c, sizeof(Constants));

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, GPUScene->LightBitFieldsBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(1, Targets->FinalColor),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("LightGridView.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::GraphicsState state;
	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	nvrhi::FramebufferHandle RenderTarget = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget);
	state.framebuffer = RenderTarget;
	state.viewport.addViewportAndScissorRect(RenderTarget->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Framebuffer View");
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
