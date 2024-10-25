#include "ragdollpch.h"
#include "ShadowPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void ShadowPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;

	RD_ASSERT(RenderTarget == nullptr, "Render Target Framebuffer not set");
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
	RD_SCOPE(Render, ShadowPass);
	RD_GPU_SCOPE("ShadowPass", CommandListRef);

	for (int i = 0; i < 4; ++i) {
		//MICROPROFILE_SCOPEGPUI("Cascade Shadow", MP_LIGHTYELLOW1);
		if (infos[i].empty())
			continue;
		nvrhi::FramebufferHandle pipelineFb = RenderTarget[i];

		nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowPass CBuffer", 1);
		nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

		nvrhi::BindingSetDesc BindingSetDesc;
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer[i]),
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
		state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, RenderTarget[0]);
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
