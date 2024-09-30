#include "ragdollpch.h"
#include "ShadowMaskPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void ShadowMaskPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;


	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
}

void ShadowMaskPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void ShadowMaskPass::SetDependencies(nvrhi::TextureHandle shadow[4], nvrhi::TextureHandle depth)
{
	for (int i = 0; i < 4; ++i) {
		ShadowMaps[i] = shadow[i];
	}
	GBufferDepth = depth;
}

void ShadowMaskPass::DrawShadowMask(const ragdoll::SceneInformation& sceneInfo)
{
	MICROPROFILE_SCOPEI("Render", "Shadow Mask Pass", MP_BLUEVIOLET);

	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowMask CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	for (int i = 0; i < 4; ++i) {
		CBuffer.LightViewProj[i] = sceneInfo.CascadeInfo[i].viewProj;
	}
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.View = sceneInfo.MainCameraView;
	CBuffer.EnableCascadeDebug = sceneInfo.EnableCascadeDebug;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, ShadowMaps[0]),
		nvrhi::BindingSetItem::Texture_SRV(1, ShadowMaps[1]),
		nvrhi::BindingSetItem::Texture_SRV(2, ShadowMaps[2]),
		nvrhi::BindingSetItem::Texture_SRV(3, ShadowMaps[3]),
		nvrhi::BindingSetItem::Texture_SRV(4, GBufferDepth),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[5]),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->ShadowSampler)
		
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetNativeDevice()->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("ShadowMask.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setPixelShader(PixelShader);

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

	CommandListRef->beginMarker("Shadow Mask Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
