#include "ragdollpch.h"
#include "FramebufferViewer.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void FramebufferViewer::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void FramebufferViewer::DrawTarget(nvrhi::TextureHandle texture, Vector4 add, Vector4 mul, uint32_t numComp)
{
	MICROPROFILE_SCOPEI("Render", "Debug Framebuffer View", MP_BLUEVIOLET);
	MICROPROFILE_SCOPEGPUI("Framebuffer View", MP_LIGHTYELLOW1);
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
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetNativeDevice()->createBindingSet(bindingSetDesc, BindingLayoutHandle);

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
	nvrhi::FramebufferDesc desc;
	desc.colorAttachments = { { DirectXDevice::GetInstance()->GetCurrentBackbuffer() } };
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
