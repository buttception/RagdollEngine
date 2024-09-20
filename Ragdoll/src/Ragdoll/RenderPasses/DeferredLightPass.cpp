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
	VertexShader = AssetManager::GetInstance()->GetShader("DeferredLight.vs.cso");
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
		nvrhi::BindingLayoutItem::Texture_SRV(0),
		nvrhi::BindingLayoutItem::Texture_SRV(1),
		nvrhi::BindingLayoutItem::Texture_SRV(2),
	};
	BindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DeferredLight CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	nvrhi::VertexAttributeDesc trianglePos;
	trianglePos.name = "POSITION";
	trianglePos.elementStride = sizeof(float) * 5;
	trianglePos.format = nvrhi::Format::RGB32_FLOAT;
	nvrhi::VertexAttributeDesc triangleTexcoord;
	triangleTexcoord.name = "TEXCOORD";
	triangleTexcoord.offset = sizeof(float) * 3;
	triangleTexcoord.elementStride = sizeof(float) * 5;
	triangleTexcoord.format = nvrhi::Format::RG32_FLOAT;
	nvrhi::VertexAttributeDesc attribs[] = { trianglePos, triangleTexcoord };
	nvrhi::InputLayoutHandle inputLayoutHandle = NvrhiDeviceRef->createInputLayout(attribs, 2, VertexShader);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	const static int32_t seed = 42;
	std::srand(seed);

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(VertexShader);
	pipelineDesc.setFragmentShader(PixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Front;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	pipelineDesc.inputLayout = inputLayoutHandle;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
	GraphicsPipeline = NvrhiDeviceRef->createGraphicsPipeline(pipelineDesc, RenderTarget);
}

void DeferredLightPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void DeferredLightPass::LightPass(const ragdoll::SceneInformation& sceneInfo)
{
	MICROPROFILE_SCOPEI("Render", "Light Pass", MP_BLUEVIOLET);
	//create and set the state
	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;
	CBuffer.LightDiffuseColor = sceneInfo.LightDiffuseColor;
	CBuffer.SceneAmbientColor = sceneInfo.SceneAmbientColor;
	CBuffer.LightDirection = sceneInfo.LightDirection;
	CBuffer.CameraPosition = sceneInfo.MainCameraPosition;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle)
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, AssetManager::GetInstance()->RenderTargetTextures.at("GBufferAlbedo")));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, AssetManager::GetInstance()->RenderTargetTextures.at("GBufferNormal")));
	bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, AssetManager::GetInstance()->RenderTargetTextures.at("GBufferRM")));
	BindingSetHandle = NvrhiDeviceRef->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	//TODO: make its own index buffer
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->FullscreenTriangleHandle }
	};
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Light Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);
	
	nvrhi::DrawArguments args;
	args.instanceCount = 1;
	args.vertexCount = 3;
	CommandListRef->drawIndexed(args);

	CommandListRef->endMarker();
}
