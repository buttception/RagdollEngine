#include "ragdollpch.h"
#include "BloomPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void BloomPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void BloomPass::Bloom(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, Bloom);
	RD_GPU_SCOPE("Bloom", CommandListRef);
	DownSample(targets);
	UpSample(sceneInfo.FilterRadius, sceneInfo.BloomIntensity, targets);
}

void BloomPass::DownSample(ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, DownSample);
	CommandListRef->beginMarker("Down Sample Pass");
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBufferDS), "DownSample CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	nvrhi::TextureHandle Source = targets->SceneColor;
	for (const BloomMip& mip : targets->DownsampledImages) {
		RD_SCOPE(Render, DownSampleMip);
		//create the target
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(mip.Image);
		nvrhi::FramebufferHandle target = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//create the binding set and layout
		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[3]),
			nvrhi::BindingSetItem::Texture_SRV(0, Source),
		};
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);
		{
			RD_SCOPE(Render, GetPipelineSetState);
			//create the pipeline
			nvrhi::GraphicsPipelineDesc PipelineDesc;
			PipelineDesc.addBindingLayout(BindingLayoutHandle);
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
			state.addBindingSet(BindingSetHandle);

			CBufferDS.Width = mip.Width; CBufferDS.Height = mip.Height;
			CommandListRef->writeBuffer(ConstantBufferHandle, &CBufferDS, sizeof(ConstantBufferDS));
			CommandListRef->setGraphicsState(state);
		}

		nvrhi::DrawArguments args;
		args.vertexCount = 3;
		CommandListRef->draw(args);

		//reset source to prev
		Source = mip.Image;
	}
	CommandListRef->endMarker();
}

void BloomPass::UpSample(float filterRadius, float bloomIntensity, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, UpSample);
	CommandListRef->beginMarker("Up Sample Pass");
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBufferUS), "UpSample CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);

	for (size_t i = targets->DownsampledImages.size() - 1; i > 0; --i) {
		RD_SCOPE(Render, UpSampleMip);
		//MICROPROFILE_SCOPEGPUI("Bloom Up Sample GPU", MP_LIGHTYELLOW1);
		nvrhi::TextureHandle Source = targets->DownsampledImages[i].Image;
		nvrhi::TextureHandle Target = targets->DownsampledImages[i - 1].Image;
		//create the target
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(Target);
		nvrhi::FramebufferHandle TargetFB = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//create the binding set and layout
		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[3]),
			nvrhi::BindingSetItem::Texture_SRV(0, Source),
			nvrhi::BindingSetItem::Texture_SRV(1, targets->SceneColor),
		};
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);
		//create the pipeline
		nvrhi::GraphicsPipelineDesc PipelineDesc;
		PipelineDesc.addBindingLayout(BindingLayoutHandle);
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
		state.addBindingSet(BindingSetHandle);

		CBufferUS.FilterRadius = filterRadius;
		CBufferUS.BloomIntensity = i - 1 == 0 ? bloomIntensity : 0.f;
		CommandListRef->writeBuffer(ConstantBufferHandle, &CBufferUS, sizeof(ConstantBufferUS));
		CommandListRef->setGraphicsState(state);

		nvrhi::DrawArguments args;
		args.vertexCount = 3;
		CommandListRef->draw(args);
	}
	CommandListRef->endMarker();
	{
		//MICROPROFILE_SCOPEGPUI("Apply Bloom", MP_LIGHTYELLOW1);
		CBufferUS.BloomIntensity = 0.f;
		//copy the first mip into scene color
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

		CommandListRef->beginMarker("Bloom Pass");
		CommandListRef->setGraphicsState(state);

		nvrhi::DrawArguments args;
		args.vertexCount = 3;
		CommandListRef->draw(args);
	}

	CommandListRef->endMarker();
}
