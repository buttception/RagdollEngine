#include "ragdollpch.h"
#include "ShadowMaskPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"

void ShadowMaskPass::Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
	NvrhiDeviceRef = nvrhiDevice;

	//create the pipeline
	//load outputted shaders objects
	VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	PixelShader = AssetManager::GetInstance()->GetShader("ShadowMask.ps.cso");
	//TODO: move into draw call then create only if needed
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::Texture_SRV(0),	//shadow maps
		nvrhi::BindingLayoutItem::Texture_SRV(1),
		nvrhi::BindingLayoutItem::Texture_SRV(2),
		nvrhi::BindingLayoutItem::Texture_SRV(3),
		nvrhi::BindingLayoutItem::Sampler(0),	//samplers
		nvrhi::BindingLayoutItem::Sampler(1),
		nvrhi::BindingLayoutItem::Sampler(2),
		nvrhi::BindingLayoutItem::Sampler(3),
		nvrhi::BindingLayoutItem::Sampler(4),
		nvrhi::BindingLayoutItem::Sampler(5),
		nvrhi::BindingLayoutItem::Sampler(6),
		nvrhi::BindingLayoutItem::Sampler(7),
		nvrhi::BindingLayoutItem::Sampler(8),
		nvrhi::BindingLayoutItem::Sampler(9),	//comparison sampler
		nvrhi::BindingLayoutItem::Texture_SRV(4),	//depth
	};
	BindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowMask CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, ShadowMaps[0]),
		nvrhi::BindingSetItem::Texture_SRV(1, ShadowMaps[1]),
		nvrhi::BindingSetItem::Texture_SRV(2, ShadowMaps[2]),
		nvrhi::BindingSetItem::Texture_SRV(3, ShadowMaps[3]),
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler((int)SamplerTypes::COUNT, AssetManager::GetInstance()->ShadowSampler));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, GBufferDepth));
	BindingSetHandle = NvrhiDeviceRef->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(VertexShader);
	pipelineDesc.setPixelShader(PixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
	GraphicsPipeline = AssetManager::GetInstance()->GetGraphicsPipeline(pipelineDesc, RenderTarget);
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
	//create and set the state
	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	for (int i = 0; i < 4; ++i) {
		CBuffer.LightViewProj[i] = sceneInfo.CascadeInfo[i].viewProj;
	}
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.View = sceneInfo.MainCameraView;
	CBuffer.EnableCascadeDebug = sceneInfo.EnableCascadeDebug;

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
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
