#include "ragdollpch.h"
#include "GbufferPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"

void GBufferPass::Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
	NvrhiDeviceRef = nvrhiDevice;

	//create the pipeline
	//load outputted shaders objects
	VertexShader = AssetManager::GetInstance()->GetShader("GBufferShader.vs.cso");
	PixelShader = AssetManager::GetInstance()->GetShader("GBufferShader.ps.cso");
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
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "GBufferPass CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	const auto& attribs = AssetManager::GetInstance()->VertexAttributes;
	nvrhi::InputLayoutHandle inputLayoutHandle = NvrhiDeviceRef->createInputLayout(attribs.data(), attribs.size(), VertexShader);

	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	PipelineDesc.addBindingLayout(AssetManager::GetInstance()->BindlessLayoutHandle);
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setFragmentShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	PipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Back;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	PipelineDesc.inputLayout = inputLayoutHandle;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
}

void GBufferPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget)
{
	RenderTarget = renderTarget;
}

void GBufferPass::DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, const ragdoll::SceneInformation& sceneInfo)
{
	if (infos.empty())
		return;
	MICROPROFILE_SCOPEI("Render", "Draw All Instances", MP_BLUEVIOLET);
	//create and set the state
	nvrhi::FramebufferHandle pipelineFb = RenderTarget;
	CBuffer.ViewProj = sceneInfo.MainCameraViewProj;

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
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO }
	};
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(AssetManager::GetInstance()->DescriptorTable);

	CommandListRef->beginMarker("Instance Draws");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);

	uint32_t instanceCount = 0;
	for (const ragdoll::InstanceGroupInfo& info : infos)
	{
		MICROPROFILE_SCOPEI("Render", "Each instance", MP_CADETBLUE);
		const VertexBufferInfo& buffer = AssetManager::GetInstance()->VertexBufferInfos[info.VertexBufferIndex];
		nvrhi::DrawArguments args;
		args.vertexCount = buffer.IndicesCount;
		args.startVertexLocation = buffer.VBOffset;
		args.startIndexLocation = buffer.IBOffset;
		args.instanceCount = info.InstanceCount;
		CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
		CommandListRef->drawIndexed(args);
		CBuffer.InstanceOffset += info.InstanceCount;
	}
	CBuffer.InstanceOffset = 0;

	CommandListRef->endMarker();
}
