#include "ragdollpch.h"
#include "ShadowMaskPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void ShadowMaskPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;


}

void ShadowMaskPass::DrawShadowMask(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, ShadowMask);
	RD_GPU_SCOPE("ShadowMask", CommandListRef);

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(targets->ShadowMask);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowMask CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	for (int i = 0; i < 4; ++i) {
		CBuffer.LightViewProj[i] = sceneInfo.CascadeInfos[i].viewProj;
	}
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.View = sceneInfo.MainCameraView;
	CBuffer.EnableCascadeDebug = sceneInfo.EnableCascadeDebug;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->ShadowMap[0]),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->ShadowMap[1]),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->ShadowMap[2]),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->ShadowMap[3]),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[5]),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->ShadowSampler)
		
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

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
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, pipelineFb);
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
