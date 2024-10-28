#include "ragdollpch.h"
#include "ShadowPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void ShadowPass::Init(nvrhi::CommandListHandle cmdList0, nvrhi::CommandListHandle cmdList1, nvrhi::CommandListHandle cmdList2, nvrhi::CommandListHandle cmdList3)
{
	CommandListRef[0] = cmdList0;
	CommandListRef[1] = cmdList1;
	CommandListRef[2] = cmdList2;
	CommandListRef[3] = cmdList3;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
}

void ShadowPass::SetRenderTarget(nvrhi::FramebufferHandle renderTarget[4])
{
	for (int i = 0; i < 4; ++i)
	{
		RenderTarget[i] = renderTarget[i];
	}
}

void ShadowPass::DrawAllInstances(nvrhi::BufferHandle instanceBuffer, std::vector<ragdoll::InstanceGroupInfo>& infos, const ragdoll::SceneInformation& sceneInfo, uint32_t cascadeIndex)
{
	RD_SCOPE(Render, ShadowPass);
	RD_GPU_SCOPE("ShadowPass", CommandListRef[cascadeIndex]);
	if (infos.empty())
		return;
	nvrhi::FramebufferHandle pipelineFb = RenderTarget[cascadeIndex];

	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowPass CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("DirectionalShadow.vs.cso");
	PipelineDesc.setVertexShader(VertexShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	PipelineDesc.renderState.depthStencilState.depthFunc = nvrhi::ComparisonFunc::Greater;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	PipelineDesc.inputLayout = AssetManager::GetInstance()->InstancedInputLayoutHandle;

	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget[cascadeIndex]);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect({ 2000.f, 2000.f });
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO }
	};
	state.addBindingSet(BindingSetHandle);

	CommandListRef[cascadeIndex]->beginMarker(("Directional Light Shadow Pass" + std::to_string(cascadeIndex)).c_str());
	CommandListRef[cascadeIndex]->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef[cascadeIndex]->setGraphicsState(state);

	CBuffer[cascadeIndex].CascadeIndex = cascadeIndex;
	CBuffer[cascadeIndex].LightViewProj = sceneInfo.CascadeInfo[cascadeIndex].viewProj;

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
		CommandListRef[cascadeIndex]->writeBuffer(ConstantBufferHandle, &CBuffer[cascadeIndex], sizeof(ConstantBuffer));
		CommandListRef[cascadeIndex]->drawIndexed(args);
		CBuffer[cascadeIndex].InstanceOffset += info.InstanceCount;
	}
	CBuffer[cascadeIndex].InstanceOffset = 0;
	CommandListRef[cascadeIndex]->endMarker();
}