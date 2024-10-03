#include "ragdollpch.h"
#include "ToneMapPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void ToneMapPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
}

void ToneMapPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void ToneMapPass::SetDependencies(nvrhi::TextureHandle SceneColor)
{
	Target = SceneColor;
}

void ToneMapPass::ToneMap(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle exposureHandle)
{
	MICROPROFILE_SCOPEI("Render", "Tone Map Pass", MP_BLUEVIOLET);
	//create cbuffer
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ToneMap CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.Exposure = sceneInfo.Exposure;
	CBuffer.Gamma = sceneInfo.Gamma;
	CBuffer.UseFixedExposure = sceneInfo.UseFixedExposure;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[5]),
		nvrhi::BindingSetItem::Texture_SRV(0, Target),
		nvrhi::BindingSetItem::TypedBuffer_UAV(0, exposureHandle)
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetNativeDevice()->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("ToneMap.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Tone Map Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
