#include "ragdollpch.h"
#include "ShadowMaskPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"
#include "Ragdoll/GPUScene.h"

void ShadowMaskPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void ShadowMaskPass::DrawShadowMask(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, ShadowMask);
	RD_GPU_SCOPE("ShadowMask", CommandListRef);

	nvrhi::FramebufferDesc desc = nvrhi::FramebufferDesc()
		.addColorAttachment(targets->ShadowMask);
	nvrhi::FramebufferHandle pipelineFb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
	//create a constant buffer here
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowMask CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	for (int i = 0; i < 4; ++i) {
		CBuffer.LightViewProj[i] = sceneInfo.CascadeInfos[i].viewProj;
	}
	CBuffer.InvViewProj = sceneInfo.MainCameraViewProj.Invert();
	CBuffer.View = sceneInfo.MainCameraView.Invert();
	CBuffer.EnableCascadeDebug = sceneInfo.EnableCascadeDebug;

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->ShadowMap[0]),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->ShadowMap[1]),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->ShadowMap[2]),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->ShadowMap[3]),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[5]),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->ShadowSampler)
		
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(bindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsPipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle VertexShader = AssetManager::GetInstance()->GetShader("Fullscreen.vs.cso");
	nvrhi::ShaderHandle PixelShader = AssetManager::GetInstance()->GetShader("ShadowMask.ps.cso");
	PipelineDesc.setVertexShader(VertexShader);
	PipelineDesc.setPixelShader(PixelShader);

	PipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	PipelineDesc.renderState.depthStencilState.stencilEnable = false;
	PipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	PipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	PipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;

	nvrhi::GraphicsState state;
	state.pipeline = AssetManager::GetInstance()->GetGraphicsPipeline(PipelineDesc, pipelineFb);
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.addBindingSet(BindingSetHandle);

	CommandListRef->beginMarker("Shadow Mask Pass");
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	CommandListRef->draw(args);

	CommandListRef->endMarker();
}

void ShadowMaskPass::RaytraceShadowMask(const ragdoll::SceneInformation& sceneInfo, const ragdoll::FGPUScene* GPUScene, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, ShadowMaskRT);
	RD_GPU_SCOPE("ShadowMaskRT", CommandListRef);
	CommandListRef->beginMarker("Raytrace Shadow Mask");

	struct ConstantBuffer {
		Matrix InvViewProjMatrixWithJitter;
		Vector3 LightDirection;
		float RenderWidth;
		float RenderHeight;
	}ConstantBuffer;
	ConstantBuffer.InvViewProjMatrixWithJitter = sceneInfo.MainCameraViewProjWithJitter.Invert();
	ConstantBuffer.LightDirection = sceneInfo.LightDirection;
	ConstantBuffer.RenderWidth = sceneInfo.RenderWidth;
	ConstantBuffer.RenderHeight = sceneInfo.RenderHeight;
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowMaskRT CBuffer", 1));
	CommandListRef->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(ConstantBuffer));

	//raytrace pipeline, directly draw onto shadow mask
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::RayTracingAccelStruct(0, GPUScene->TopLevelAS),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->ShadowMask),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	nvrhi::ShaderLibraryHandle ShaderLibrary = AssetManager::GetInstance()->GetShaderLibrary("RaytraceShadow.lib.cso");
	nvrhi::rt::PipelineDesc PipelineDesc;
	PipelineDesc.globalBindingLayouts = { BindingLayoutHandle };
	PipelineDesc.shaders = {
		{ "", ShaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration), nullptr },
		{ "", ShaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss), nullptr }
	};
	PipelineDesc.hitGroups = { {
		"HitGroup",
		nullptr, // closestHitShader
		nullptr, // anyHitShader
		nullptr, // intersectionShader
		nullptr, // bindingLayout
		false  // isProceduralPrimitive
	} };
	PipelineDesc.maxPayloadSize = sizeof(Vector4);
	nvrhi::rt::PipelineHandle PipelineHandle = AssetManager::GetInstance()->GetRaytracePipeline(PipelineDesc);

	ShaderTableHandle = PipelineHandle->createShaderTable();
	ShaderTableHandle->setRayGenerationShader("RayGen");
	ShaderTableHandle->addHitGroup("HitGroup");
	ShaderTableHandle->addMissShader("Miss");

	nvrhi::rt::State State;
	State.shaderTable = ShaderTableHandle;
	State.bindings = { BindingSetHandle };
	CommandListRef->setRayTracingState(State);

	nvrhi::rt::DispatchRaysArguments args;
	args.width = sceneInfo.RenderWidth;
	args.height = sceneInfo.RenderHeight;
	CommandListRef->dispatchRays(args);

	CommandListRef->endMarker();
}
