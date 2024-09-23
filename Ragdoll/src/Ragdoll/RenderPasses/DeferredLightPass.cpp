#include "ragdollpch.h"
#include "DeferredLightPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"

void DeferredLightPass::Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
	NvrhiDeviceRef = nvrhiDevice;

	//create the pipeline
	//load outputted shaders objects
	VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	PixelShader = AssetManager::GetInstance()->GetShader("DeferredLight.ps.cso");
	//TODO: move into draw call then create only if needed
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(1),
		nvrhi::BindingLayoutItem::Sampler(0),
		nvrhi::BindingLayoutItem::Sampler(1),
		nvrhi::BindingLayoutItem::Sampler(2),
		nvrhi::BindingLayoutItem::Sampler(3),
		nvrhi::BindingLayoutItem::Sampler(4),
		nvrhi::BindingLayoutItem::Sampler(5),
		nvrhi::BindingLayoutItem::Sampler(6),
		nvrhi::BindingLayoutItem::Sampler(7),
		nvrhi::BindingLayoutItem::Sampler(8),
		nvrhi::BindingLayoutItem::Sampler(9),
		nvrhi::BindingLayoutItem::Texture_SRV(0),
		nvrhi::BindingLayoutItem::Texture_SRV(1),
		nvrhi::BindingLayoutItem::Texture_SRV(2),
		nvrhi::BindingLayoutItem::Texture_SRV(3),
	};
	BindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DeferredLight CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);
	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(VertexShader);
	pipelineDesc.setFragmentShader(PixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
	GraphicsPipeline = NvrhiDeviceRef->createGraphicsPipeline(pipelineDesc, RenderTarget);
}

void DeferredLightPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void DeferredLightPass::SetDependencies(nvrhi::TextureHandle albedo, nvrhi::TextureHandle normal, nvrhi::TextureHandle orm, nvrhi::TextureHandle depth)
{
	AlbedoHandle = albedo;
	NormalHandle = normal;
	AORoughnessMetallicHandle = orm;
	DepthHandle = depth;
}

void DeferredLightPass::LightPass(const ragdoll::SceneInformation& sceneInfo)
{
	MICROPROFILE_SCOPEI("Render", "Light Pass", MP_BLUEVIOLET);
	//create and set the state
	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.LightDiffuseColor = sceneInfo.LightDiffuseColor;
	CBuffer.SceneAmbientColor = sceneInfo.SceneAmbientColor;
	CBuffer.LightDirection = sceneInfo.LightDirection;
	CBuffer.CameraPosition = sceneInfo.MainCameraPosition;
	CBuffer.LightIntensity = sceneInfo.LightIntensity;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle)
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler((int)SamplerTypes::COUNT, AssetManager::GetInstance()->ShadowSampler));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, AlbedoHandle));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, NormalHandle));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, AORoughnessMetallicHandle));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, DepthHandle));
	BindingSetHandle = NvrhiDeviceRef->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Light Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);
	
	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
