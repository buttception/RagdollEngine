#include "ragdollpch.h"
#include "DebugPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void DebugPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
}

void DebugPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

bool DebugPass::DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, uint32_t instanceCount, const ragdoll::SceneInformation& sceneInfo)
{
	MICROPROFILE_SCOPEI("Render", "Draw Bounding Box", MP_ALICEBLUE);
	//MICROPROFILE_SCOPEGPUI("Debug Instance Draw", MP_LIGHTYELLOW1);
	CommandListRef->open();
	if (instanceCount == 0)
	{
		CommandListRef->close();
		return false;
	}
	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "DebugPass CBuffer", 1);
	nvrhi::BufferHandle	ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetNativeDevice()->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("DebugShader.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("DebugShader.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	PipelineDesc.primType = nvrhi::PrimitiveType::LineList;

	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget);
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
	CommandListRef->close();

	return true;
}
