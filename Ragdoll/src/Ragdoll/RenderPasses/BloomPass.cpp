#include "ragdollpch.h"
#include "BloomPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void BloomPass::CreateBindingSet(ragdoll::SceneRenderTargets* targets)
{
	for (int i = 0; i < 5; ++i)
	{
		nvrhi::BindingSetDesc BindingSetDesc;
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[3]),
			nvrhi::BindingSetItem::Texture_SRV(0, i == 0 ? targets->SceneColor : targets->DownsampledImages[i - 1].Image),
		};
		DownSampleSetHandles[i] = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc));
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[3]),
			nvrhi::BindingSetItem::Texture_SRV(0, targets->DownsampledImages[4 - i].Image),
			nvrhi::BindingSetItem::Texture_SRV(1, targets->SceneColor),
		};
		UpSampleSetHandles[i] = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc));
	}
}

void BloomPass::Init(nvrhi::CommandListHandle cmdList, ragdoll::SceneRenderTargets* targets)
{
	CommandListRef = cmdList;
}

void BloomPass::Bloom(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, Bloom);
	RD_GPU_SCOPE("Bloom", CommandListRef);
	if (sceneInfo.BloomIntensity == 0.f)
		return;
	if (sceneInfo.FilterRadius == 0.f)
		return;
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "Bloom CBuffer", 1);
	ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CreateBindingSet(targets);

	DownSample(targets);
	UpSample(sceneInfo.FilterRadius, sceneInfo.BloomIntensity, targets);
}

void BloomPass::DownSample(ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, DownSample);
	CommandListRef->beginMarker("Down Sample Pass");

	for (int i = 0; i < 5; ++i) {
		RD_SCOPE(Render, DownSampleMip);
		//create the target
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(targets->DownsampledImages[i].Image);
		nvrhi::FramebufferHandle target = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		{
			RD_SCOPE(Render, GetPipelineSetState);
			//create the pipeline
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.addBindingLayout(DownSampleSetHandles[i]->getLayout());
			nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
			nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("DownSample.ps.cso");
			PipelineDesc.setVertexShader(VertexShader);
			PipelineDesc.setFragmentShader(PixelShader);

			PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
			PipelineDesc.renderState.depthStencilState.stencilEnable = false;
			PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
			PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
			PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
			//create the state
			nvrhi::GraphicsState state;
			state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, target);
			state.framebuffer = target;
			state.viewport.addViewportAndScissorRect(target->getFramebufferInfo().getViewport());
			state.addBindingSet(DownSampleSetHandles[i]);

			ConstantBuffer.Width = targets->DownsampledImages[i].Width;
			ConstantBuffer.Height = targets->DownsampledImages[i].Height;
			CommandListRef->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));
			CommandListRef->setGraphicsState(state);
		}

		nvrhi::DrawArguments args;
		args.vertexCount = 3;
		CommandListRef->draw(args);
	}
	CommandListRef->endMarker();
}

void BloomPass::UpSample(float filterRadius, float bloomIntensity, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, UpSample);
	CommandListRef->beginMarker("Up Sample Pass");

	//the final result is the mix pass, where the 1 mip down will be mixed with the native resolution result
	//4 -> 3 -> 2 -> 1 -> 0
	for (size_t i = targets->DownsampledImages.size() - 1; i > 0; --i) {
		RD_SCOPE(Render, UpSampleMip);
		//3 -> 2 -> 1 -> 0
		nvrhi::TextureHandle Target = targets->DownsampledImages[i - 1].Image;
		//create the target
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(Target);
		nvrhi::FramebufferHandle TargetFB = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//create the binding set and layout
		//create the pipeline
		nvrhi::GraphicsPipelineDesc PipelineDesc;
		PipelineDesc.addBindingLayout(UpSampleSetHandles[4 - i]->getLayout());
		nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
		nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("UpSample.ps.cso");
		PipelineDesc.setVertexShader(VertexShader);
		PipelineDesc.setFragmentShader(PixelShader);

		PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
		PipelineDesc.renderState.depthStencilState.stencilEnable = false;
		PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
		PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
		PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
		//create the state
		nvrhi::GraphicsState state;
		state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, TargetFB);
		state.framebuffer = TargetFB;
		state.viewport.addViewportAndScissorRect(TargetFB->getFramebufferInfo().getViewport());
		//images used is 4 -> 3 -> 2 -> 1
		state.addBindingSet(UpSampleSetHandles[4 - i]);

		ConstantBuffer.FilterRadius = filterRadius;
		ConstantBuffer.BloomIntensity = (i - 1 == 0) ? bloomIntensity : 0.f;
		//if bloom intensity is 0, the shader will pressume is last pass and will bloom instead of upsample
		CommandListRef->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));
		CommandListRef->setGraphicsState(state);

		nvrhi::DrawArguments args;
		args.vertexCount = 3;
		CommandListRef->draw(args);
	}
	CommandListRef->endMarker();
	{
		//copy the result into scene color
		//create the target
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(targets->SceneColor);
		nvrhi::FramebufferHandle TargetFB = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//create the binding set and layout
		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.bindings = {
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[5]),
			nvrhi::BindingSetItem::Texture_SRV(0, targets->DownsampledImages[0].Image),
		};
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);
		//create the pipeline
		nvrhi::GraphicsPipelineDesc PipelineDesc;
		PipelineDesc.addBindingLayout(BindingLayoutHandle);
		nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
		nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("Fullscreen.ps.cso");
		PipelineDesc.setVertexShader(VertexShader);
		PipelineDesc.setFragmentShader(PixelShader);

		PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
		PipelineDesc.renderState.depthStencilState.stencilEnable = false;
		PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
		PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
		PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
		//create the state
		nvrhi::GraphicsState state;
		state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, TargetFB);
		state.framebuffer = TargetFB;
		state.viewport.addViewportAndScissorRect(TargetFB->getFramebufferInfo().getViewport());
		state.addBindingSet(BindingSetHandle);

		CommandListRef->beginMarker("Copy Result");
		CommandListRef->setGraphicsState(state);

		nvrhi::DrawArguments args;
		args.vertexCount = 3;
		CommandListRef->draw(args);
	}

	CommandListRef->endMarker();
}
