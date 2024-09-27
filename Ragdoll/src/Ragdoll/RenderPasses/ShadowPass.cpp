#include "ragdollpch.h"
#include "ShadowPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"

void ShadowPass::Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
	NvrhiDeviceRef = nvrhiDevice;

	//create the pipeline
	//load outputted shaders objects
	VertexShader = AssetManager::GetInstance()->GetShader("DirectionalShadow.vs.cso");
	//TODO: move into draw call then create only if needed
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
	};
	BindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowPass CBuffer", 1);
	ConstantBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	const auto& attribs = AssetManager::GetInstance()->VertexAttributes;
	nvrhi::InputLayoutHandle inputLayoutHandle = NvrhiDeviceRef->createInputLayout(attribs.data(), attribs.size(), VertexShader);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(VertexShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	pipelineDesc.inputLayout = inputLayoutHandle;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
	GraphicsPipeline = AssetManager::GetInstance()->GetGraphicsPipeline(pipelineDesc, RenderTarget[0]);
}

void ShadowPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget[4])
{
	for (int i = 0; i < 4; ++i)
	{
		RenderTarget[i] = renderTarget[i];
	}
}

void ShadowPass::DrawAllInstances(nvrhi::BufferHandle instanceBuffer[4], std::vector<ragdoll::InstanceGroupInfo>* infos, const ragdoll::SceneInformation& sceneInfo)
{
	MICROPROFILE_SCOPEI("Render", "Directional Light Shadow Pass", MP_BLUEVIOLET);
	for (int i = 0; i < 4; ++i) {
		if (infos[i].empty())
			continue;
		//create and set the state
		nvrhi::FramebufferHandle pipelineFb = RenderTarget[i];
		//create the light view proj

		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer[i]),
		};
		BindingSetHandle = NvrhiDeviceRef->createBindingSet(bindingSetDesc, BindingLayoutHandle);

		nvrhi::GraphicsState state;
		state.pipeline = GraphicsPipeline;
		state.framebuffer = pipelineFb;
		state.viewport.addViewportAndScissorRect({ 2000.f, 2000.f });
		state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
		state.vertexBuffers = {
			{ AssetManager::GetInstance()->VBO }
		};
		state.addBindingSet(BindingSetHandle);

		CommandListRef->beginMarker(("Directional Light Shadow Pass" + std::to_string(i)).c_str());
		CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
		CommandListRef->setGraphicsState(state);

		CBuffer.CascadeIndex = i;
		CBuffer.LightViewProj = sceneInfo.CascadeInfo[i].viewProj;

		uint32_t instanceCount = 0;
		for (const ragdoll::InstanceGroupInfo& info : infos[i])
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
}
