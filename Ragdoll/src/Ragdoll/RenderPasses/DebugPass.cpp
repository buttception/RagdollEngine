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
	VertexShader = AssetManager::GetInstance()->GetShader("DebugShader.vs.cso");
	PixelShader = AssetManager::GetInstance()->GetShader("DebugShader.ps.cso");
	//TODO: move into draw call then create only if needed
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
	};
	BindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DebugPass CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(VertexShader);
	pipelineDesc.setFragmentShader(PixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	pipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	pipelineDesc.primType = nvrhi::PrimitiveType::LineList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
	GraphicsPipeline = AssetManager::GetInstance()->GetGraphicsPipeline(pipelineDesc, RenderTarget);
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
	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer),
	};
	BindingSetHandle = NvrhiDeviceRef->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Debug Draws");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(CBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 24;
	args.instanceCount = instanceCount;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}
