#include "ragdollpch.h"
#include "DebugPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"

void DebugPass::Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
	NvrhiDeviceRef = nvrhiDevice;

	//create the pipeline
	//load outputted shaders objects
	ForwardVertexShader = AssetManager::GetInstance()->GetShader("ForwardShader.vs.cso");
	ForwardPixelShader = AssetManager::GetInstance()->GetShader("ForwardShader.ps.cso");
	//TODO: move into draw call then create only if needed
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
		nvrhi::BindingLayoutItem::Sampler(0),
		nvrhi::BindingLayoutItem::Sampler(1),
		nvrhi::BindingLayoutItem::Sampler(2),
		nvrhi::BindingLayoutItem::Sampler(3),
		nvrhi::BindingLayoutItem::Sampler(4),
		nvrhi::BindingLayoutItem::Sampler(5),
		nvrhi::BindingLayoutItem::Sampler(6),
		nvrhi::BindingLayoutItem::Sampler(7),
		nvrhi::BindingLayoutItem::Sampler(8),
	};
	BindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ForwardPass CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	const auto& attribs = AssetManager::GetInstance()->VertexAttributes;
	nvrhi::InputLayoutHandle inputLayoutHandle = NvrhiDeviceRef->createInputLayout(attribs.data(), attribs.size(), ForwardVertexShader);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	const static int32_t seed = 42;
	std::srand(seed);

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	pipelineDesc.setVertexShader(ForwardVertexShader);
	pipelineDesc.setFragmentShader(ForwardPixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	pipelineDesc.renderState.rasterState.fillMode = nvrhi::RasterFillMode::Wireframe;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	pipelineDesc.inputLayout = inputLayoutHandle;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
	GraphicsPipeline = NvrhiDeviceRef->createGraphicsPipeline(pipelineDesc, RenderTarget);
}

void DebugPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void DebugPass::DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, uint32_t instanceCount, const ragdoll::SceneInformation& sceneInfo)
{
	MICROPROFILE_SCOPEI("Render", "Draw Bounding Box", MP_ALICEBLUE);
	if (instanceCount == 0)
		return;
	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.CameraPosition = sceneInfo.MainCameraPosition;
	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;
	CBuffer.LightDiffuseColor = sceneInfo.LightDiffuseColor;
	CBuffer.LightDirection = sceneInfo.LightDirection;
	CBuffer.SceneAmbientColor = sceneInfo.SceneAmbientColor;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer),
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	BindingSetHandle = NvrhiDeviceRef->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO }
	};
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->beginMarker("Debug Draws");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(CBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 36;
	args.startVertexLocation = 0;
	args.startIndexLocation = 0;
	args.instanceCount = instanceCount;
	CBuffer.InstanceOffset = 0;
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(CBuffer));
	CommandListRef->drawIndexed(args);

	CommandListRef->endMarker();
}
