#include "ragdollpch.h"
#include "ShadowPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/GPUScene.h"
#include "Ragdoll/DirectXDevice.h"

void ShadowPass::Init(nvrhi::CommandListHandle cmdList0, nvrhi::CommandListHandle cmdList1, nvrhi::CommandListHandle cmdList2, nvrhi::CommandListHandle cmdList3)
{
	CommandListRef[0] = cmdList0;
	CommandListRef[1] = cmdList1;
	CommandListRef[2] = cmdList2;
	CommandListRef[3] = cmdList3;
}

void ShadowPass::DrawAllInstances(
	uint32_t CascadeIndex,
	ragdoll::FGPUScene* GPUScene,
	uint32_t ProxyCount,
	const ragdoll::SceneInformation& sceneInfo,
	ragdoll::SceneRenderTargets* targets
)
{
	RD_SCOPE(Render, ShadowPass);
	RD_GPU_SCOPE("ShadowPass", CommandListRef[CascadeIndex]);
	ragdoll::CascadeInfo CascadeInfo = sceneInfo.CascadeInfos[CascadeIndex];
	GPUScene->InstanceCull(CommandListRef[CascadeIndex], CascadeInfo.proj, CascadeInfo.view, ProxyCount, false);

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.setDepthAttachment(targets->ShadowMap[CascadeIndex]);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);

	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowPass CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, GPUScene->InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(1, GPUScene->InstanceOffsetBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(2, GPUScene->InstanceIdBuffer),
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
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect({ 2000.f, 2000.f });
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO }
	};
	state.indirectParams = GPUScene->IndirectDrawArgsBuffer;
	state.addBindingSet(BindingSetHandle);

	CommandListRef[CascadeIndex]->beginMarker(("Directional Light Shadow Pass" + std::to_string(CascadeIndex)).c_str());
	CommandListRef[CascadeIndex]->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef[CascadeIndex]->setGraphicsState(state);

	CBuffer[CascadeIndex].CascadeIndex = CascadeIndex;
	CBuffer[CascadeIndex].LightViewProj = sceneInfo.CascadeInfos[CascadeIndex].viewProj;
	CBuffer[CascadeIndex].MeshIndex = 0;

	for (int MeshIndex = 0; MeshIndex < AssetManager::GetInstance()->VertexBufferInfos.size(); ++MeshIndex)
	{
		MICROPROFILE_SCOPEI("Render", "Each instance", MP_CADETBLUE);
		CBuffer[CascadeIndex].MeshIndex = MeshIndex;
		CommandListRef[CascadeIndex]->writeBuffer(ConstantBufferHandle, &CBuffer[CascadeIndex], sizeof(ConstantBuffer));
		CommandListRef[CascadeIndex]->drawIndexedIndirect(MeshIndex * sizeof(nvrhi::DrawIndexedIndirectArguments));
	}
	CBuffer[CascadeIndex].MeshIndex = 0;
	CommandListRef[CascadeIndex]->endMarker();
}