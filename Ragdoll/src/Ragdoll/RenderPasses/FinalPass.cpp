#include "ragdollpch.h"
#include "FinalPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void FinalPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void FinalPass::DrawQuad(ragdoll::SceneRenderTargets* targets, bool upscaled)
{
	RD_SCOPE(Render, Final);
	RD_GPU_SCOPE("FinalPass", CommandListRef);

	struct CBuffer {
		Vector2 TexcoordAdd, TexcoordMul;
	} cbuffer;
	//cbuffer.TexcoordAdd = Vector2(1.f, 0.f);
	//cbuffer.TexcoordMul = Vector2(-1.f, 1.f);
	cbuffer.TexcoordAdd = Vector2(0.f, 0.f);
	cbuffer.TexcoordMul = Vector2(1.f, 1.f);
	nvrhi::BufferHandle constbuf = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(CBuffer), "Mix Pass CBuffer", 1));
	CommandListRef->writeBuffer(constbuf, &cbuffer, sizeof(CBuffer));

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, constbuf),
		nvrhi::BindingSetItem::Texture_SRV(0, upscaled ? targets->PresentationBuffer : targets->FinalColor),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("Fullscreen.ps.cso");
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

	CommandListRef->beginMarker("Final Pass");
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}

void FinalPass::MeshletPass(ragdoll::SceneRenderTargets* targets, bool upscaled)
{
	RD_SCOPE(Render, Final);
	RD_GPU_SCOPE("FinalPass", CommandListRef);

	struct CBuffer {
		Vector2 TexcoordAdd, TexcoordMul;
	} cbuffer;
	//cbuffer.TexcoordAdd = Vector2(1.f, 0.f);
	//cbuffer.TexcoordMul = Vector2(-1.f, 1.f);
	cbuffer.TexcoordAdd = Vector2(0.f, 0.f);
	cbuffer.TexcoordMul = Vector2(1.f, 1.f);
	nvrhi::BufferHandle constbuf = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(CBuffer), "Mix Pass CBuffer", 1));
	CommandListRef->writeBuffer(constbuf, &cbuffer, sizeof(CBuffer));

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, constbuf),
		nvrhi::BindingSetItem::Texture_SRV(0, upscaled ? targets->PresentationBuffer : targets->FinalColor),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::MeshletPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	nvrhi::ShaderHandle MeshShader = AssetManager::GetInstance()->GetShader("Fullscreen.ms.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("Fullscreen.ps.cso");
	PipelineDesc.setMeshShader(MeshShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::MeshletState state;
	state.pipeline = AssetManager::GetInstance()->GetMeshletPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->beginMarker("Final Pass");
	CommandListRef->setMeshletState(state);

	CommandListRef->dispatchMesh(1);

	CommandListRef->endMarker();
}
