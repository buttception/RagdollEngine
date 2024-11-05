#include "ragdollpch.h"
#include "DeferredLightPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void DeferredLightPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
}

void DeferredLightPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void DeferredLightPass::SetDependencies(nvrhi::TextureHandle albedo, nvrhi::TextureHandle normal, nvrhi::TextureHandle rm, nvrhi::TextureHandle ao, nvrhi::TextureHandle depth, nvrhi::TextureHandle shadowMask)
{
	AlbedoHandle = albedo;
	NormalHandle = normal;
	RoughnessMetallicHandle = rm;
	AONormalized = ao;
	SceneDepthZ = depth;
	ShadowMask = shadowMask;
}

void DeferredLightPass::LightPass(const ragdoll::SceneInformation& sceneInfo)
{
	RD_SCOPE(Render, LightPass);
	RD_GPU_SCOPE("LightPass", CommandListRef);
	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DeferredLight CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.LightDiffuseColor = sceneInfo.LightDiffuseColor;
	CBuffer.SceneAmbientColor = sceneInfo.SceneAmbientColor;
	CBuffer.LightDirection = sceneInfo.LightDirection;
	CBuffer.CameraPosition = sceneInfo.MainCameraPosition;
	CBuffer.LightIntensity = sceneInfo.LightIntensity;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, AlbedoHandle),
		nvrhi::BindingSetItem::Texture_SRV(1, NormalHandle),
		nvrhi::BindingSetItem::Texture_SRV(2, RoughnessMetallicHandle),
		nvrhi::BindingSetItem::Texture_SRV(3, AONormalized),
		nvrhi::BindingSetItem::Texture_SRV(4, SceneDepthZ),
		nvrhi::BindingSetItem::Texture_SRV(5, ShadowMask)
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
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("DeferredLight.ps.cso");
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
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->beginMarker("Light Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);
	
	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
