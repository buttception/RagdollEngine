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
		uint32_t Scroll;
		float RenderWidth;
		float RenderHeight;
		float SunSize;
	}ConstantBuffer;
	static uint32_t Scroll = 0.f;
	ConstantBuffer.InvViewProjMatrixWithJitter = sceneInfo.MainCameraViewProjWithJitter.Invert();
	ConstantBuffer.LightDirection = sceneInfo.LightDirection;
	ConstantBuffer.RenderWidth = sceneInfo.RenderWidth;
	ConstantBuffer.RenderHeight = sceneInfo.RenderHeight;
	ConstantBuffer.Scroll = Scroll++;
	ConstantBuffer.SunSize = sceneInfo.SunSize;
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "ShadowMaskRT CBuffer", 1));
	CommandListRef->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(ConstantBuffer));

	//raytrace pipeline, directly draw onto shadow mask
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::RayTracingAccelStruct(0, GPUScene->TopLevelAS),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(2, GPUScene->InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(3, GPUScene->MaterialBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(4, GPUScene->MeshBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(5, AssetManager::GetInstance()->VBO),
		nvrhi::BindingSetItem::RawBuffer_SRV(6, AssetManager::GetInstance()->IBO),
		nvrhi::BindingSetItem::Texture_SRV(7, AssetManager::GetInstance()->BlueNoise2D, nvrhi::Format::RGBA8_UNORM, nvrhi::TextureSubresourceSet(0, 1, Scroll % 64, 1)),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->ShadowMask),
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		BindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	nvrhi::ShaderLibraryHandle ShaderLibrary = AssetManager::GetInstance()->GetShaderLibrary("RaytraceShadow.lib.cso");
	nvrhi::rt::PipelineDesc PipelineDesc;
	PipelineDesc.globalBindingLayouts = { BindingLayoutHandle, AssetManager::GetInstance()->BindlessLayoutHandle };
	PipelineDesc.shaders = {
		{ "", ShaderLibrary->getShader("RayGen", nvrhi::ShaderType::RayGeneration), nullptr },
		{ "", ShaderLibrary->getShader("Miss", nvrhi::ShaderType::Miss), nullptr },
	};
	PipelineDesc.hitGroups = { {
		"HitGroup",
		nullptr, // closestHitShader
		ShaderLibrary->getShader("AnyHit", nvrhi::ShaderType::AnyHit), // anyHitShader
		nullptr, // intersectionShader
		nullptr, // bindingLayout
		false  // isProceduralPrimitive
	} };
	PipelineDesc.maxPayloadSize = 2 * sizeof(Vector4);
	nvrhi::rt::PipelineHandle PipelineHandle = AssetManager::GetInstance()->GetRaytracePipeline(PipelineDesc);

	ShaderTableHandle = PipelineHandle->createShaderTable();
	ShaderTableHandle->setRayGenerationShader("RayGen");
	ShaderTableHandle->addHitGroup("HitGroup");
	ShaderTableHandle->addMissShader("Miss");

	nvrhi::rt::State State;
	State.shaderTable = ShaderTableHandle;
	State.bindings = { BindingSetHandle, AssetManager::GetInstance()->DescriptorTable };
	CommandListRef->setRayTracingState(State);

	nvrhi::rt::DispatchRaysArguments args;
	args.width = sceneInfo.RenderWidth;
	args.height = sceneInfo.RenderHeight;
	CommandListRef->dispatchRays(args);

	CommandListRef->endMarker();

	if(sceneInfo.bRaytraceShadowDenoiser)
		FFX_Denoise(sceneInfo, targets);
}

void ShadowMaskPass::FFX_Denoise(const ragdoll::SceneInformation& SceneInfo, ragdoll::SceneRenderTargets* Targets)
{
	CommandListRef->beginMarker("FFX Denoise");
	uint32_t TileCountX{ DIVIDE_ROUNDING_UP(SceneInfo.RenderWidth, Targets->k_tileSizeX) };
	uint32_t TileCountY{ DIVIDE_ROUNDING_UP(SceneInfo.RenderHeight, Targets->k_tileSizeY) };
	uint32_t TileCount = TileCountX * TileCountY;
	//need to compress the shadow mask into a texture with 8x4 tiles first
	{
		struct PackShadowMaskConstants
		{
			Vector2 ShadowMaskSize;
		} PackShadowMaskConstants;
		PackShadowMaskConstants.ShadowMaskSize = Vector2((float)SceneInfo.RenderWidth, (float)SceneInfo.RenderHeight);
		nvrhi::BufferHandle PackShadowMaskConstantsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(PackShadowMaskConstants), "PackShadowMaskConstants", 1));
		CommandListRef->writeBuffer(PackShadowMaskConstantsBuffer, &PackShadowMaskConstants, sizeof(PackShadowMaskConstants));
		nvrhi::BindingSetDesc PackShadowMaskBindingSetDesc;
		PackShadowMaskBindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, PackShadowMaskConstantsBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, Targets->ShadowMask),
			nvrhi::BindingSetItem::Texture_UAV(0, Targets->RayTraceResult),
		};
		nvrhi::BindingLayoutHandle PackShadowMaskBindingLayout = AssetManager::GetInstance()->GetBindingLayout(PackShadowMaskBindingSetDesc);
		nvrhi::BindingSetHandle PackShadowMaskBindingSet = DirectXDevice::GetInstance()->CreateBindingSet(PackShadowMaskBindingSetDesc, PackShadowMaskBindingLayout);

		nvrhi::ComputePipelineDesc PackShadowMaskPipelineDesc;
		PackShadowMaskPipelineDesc.bindingLayouts = { PackShadowMaskBindingLayout };
		nvrhi::ShaderHandle PackShadowMaskShader = AssetManager::GetInstance()->GetShader("CompressShadowMask.cs.cso");
		PackShadowMaskPipelineDesc.CS = PackShadowMaskShader;
		nvrhi::ComputeState PackShadowMaskState;
		PackShadowMaskState.pipeline = AssetManager::GetInstance()->GetComputePipeline(PackShadowMaskPipelineDesc);
		PackShadowMaskState.bindings = { PackShadowMaskBindingSet };
		CommandListRef->setComputeState(PackShadowMaskState);
		CommandListRef->dispatch(TileCountX, TileCountY, 1);
	}
	
	//prepare shadow mask
	{
		struct PrepareShadowMaskConstants
		{
			int iBufferDimensionX;
			int iBufferDimensionY;
		} PrepareShadowMaskConstants;
		PrepareShadowMaskConstants.iBufferDimensionX = SceneInfo.RenderWidth;
		PrepareShadowMaskConstants.iBufferDimensionY = SceneInfo.RenderHeight;
		nvrhi::BufferHandle PrepareShadowMaskConstantsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(PrepareShadowMaskConstants), "PrepareShadowMaskConstants", 1));
		CommandListRef->writeBuffer(PrepareShadowMaskConstantsBuffer, &PrepareShadowMaskConstants, sizeof(PrepareShadowMaskConstants));

		if (!DenoiserTileBuffer)
		{
			nvrhi::BufferDesc DenoiserTileBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t) * TileCount, "DenoiserTileBuffer");
			DenoiserTileBufferDesc.canHaveUAVs = true;
			DenoiserTileBufferDesc.structStride = sizeof(uint32_t);
			DenoiserTileBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			DenoiserTileBufferDesc.keepInitialState = true;
			DenoiserTileBuffer = DirectXDevice::GetNativeDevice()->createBuffer(DenoiserTileBufferDesc);
		}

		nvrhi::BindingSetDesc PrepareShadowMaskBindingSetDesc;
		PrepareShadowMaskBindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, PrepareShadowMaskConstantsBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, Targets->RayTraceResult),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(0, DenoiserTileBuffer),
		};
		nvrhi::BindingLayoutHandle PrepareShadowMaskBindingLayout = AssetManager::GetInstance()->GetBindingLayout(PrepareShadowMaskBindingSetDesc);
		nvrhi::BindingSetHandle PrepareShadowMaskBindingSet = DirectXDevice::GetInstance()->CreateBindingSet(PrepareShadowMaskBindingSetDesc, PrepareShadowMaskBindingLayout);

		nvrhi::ComputePipelineDesc PrepareShadowMaskPipelineDesc;
		PrepareShadowMaskPipelineDesc.bindingLayouts = { PrepareShadowMaskBindingLayout };
		nvrhi::ShaderHandle PrepareShadowMaskShader = AssetManager::GetInstance()->GetShader("ffxPrepareShadowMask.cs.cso");
		PrepareShadowMaskPipelineDesc.CS = PrepareShadowMaskShader;
		nvrhi::ComputeState PrepareShadowMaskState;
		PrepareShadowMaskState.pipeline = AssetManager::GetInstance()->GetComputePipeline(PrepareShadowMaskPipelineDesc);
		PrepareShadowMaskState.bindings = { PrepareShadowMaskBindingSet };
		CommandListRef->setComputeState(PrepareShadowMaskState);
		uint32_t ThreadCountX{ DIVIDE_ROUNDING_UP(SceneInfo.RenderWidth, (Targets->k_tileSizeX * 4)) }, ThreadCountY{ DIVIDE_ROUNDING_UP(SceneInfo.RenderHeight, (Targets->k_tileSizeY * 4)) };
		CommandListRef->dispatch(ThreadCountX, ThreadCountY, 1);
	}

	//tile classification
	{
		struct TileClassificationConstants {
			Vector3 fEye;
			int iFirstFrame;
			int iBufferDimensionX;
			int iBufferDimensionY;
			Vector2 fInvBufferDimensions;
			Vector2 fMotionVectorScale;
			Vector2 normalsUnpackMul_unpackAdd;
			Matrix fProjectionInverse; // col_major
			Matrix fReprojectionMatrix; // col_major
			Matrix fViewProjectionInverse; // col_major
		} TileClassificationConstants;
		TileClassificationConstants.fEye = SceneInfo.MainCameraPosition;
		static bool FirstFrame = true;
		TileClassificationConstants.iFirstFrame = FirstFrame;
		FirstFrame = false;
		TileClassificationConstants.iBufferDimensionX = SceneInfo.RenderWidth;
		TileClassificationConstants.iBufferDimensionY = SceneInfo.RenderHeight;
		TileClassificationConstants.fInvBufferDimensions = Vector2(1.f / SceneInfo.RenderWidth, 1.f / SceneInfo.RenderHeight);
		TileClassificationConstants.fMotionVectorScale = TileClassificationConstants.fInvBufferDimensions;
		TileClassificationConstants.normalsUnpackMul_unpackAdd = Vector2(1.f, 0.f);
		TileClassificationConstants.fProjectionInverse = SceneInfo.MainCameraProjWithJitter.Invert();
		TileClassificationConstants.fReprojectionMatrix = SceneInfo.PrevMainCameraViewProjWithJitter * SceneInfo.PrevMainCameraViewProjWithJitter.Invert();
		//TileClassificationConstants.fReprojectionMatrix = SceneInfo.PrevMainCameraViewProj.Invert() * SceneInfo.PrevMainCameraViewProj;
		TileClassificationConstants.fViewProjectionInverse = SceneInfo.MainCameraViewProjWithJitter.Invert();
		nvrhi::BufferHandle TileClassificationConstantsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(TileClassificationConstants), "TileClassificationConstants", 1));
		CommandListRef->writeBuffer(TileClassificationConstantsBuffer, &TileClassificationConstants, sizeof(TileClassificationConstants));

		if (!DenoiserTileMetaDataBuffer)
		{
			nvrhi::BufferDesc DenoiserTileMetaDataBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t) * TileCount, "DenoiserTileMetaDataBuffer");
			DenoiserTileMetaDataBufferDesc.canHaveUAVs = true;
			DenoiserTileMetaDataBufferDesc.structStride = sizeof(uint32_t);
			DenoiserTileMetaDataBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
			DenoiserTileMetaDataBufferDesc.keepInitialState = true;
			DenoiserTileMetaDataBuffer = DirectXDevice::GetNativeDevice()->createBuffer(DenoiserTileMetaDataBufferDesc);
		}

		nvrhi::BindingSetDesc TileClassificationBindingSetDesc;
		TileClassificationBindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, TileClassificationConstantsBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, Targets->CurrDepthBuffer),
			nvrhi::BindingSetItem::Texture_SRV(1, Targets->VelocityBuffer),
			nvrhi::BindingSetItem::Texture_SRV(2, Targets->GBufferNormal),
			nvrhi::BindingSetItem::Texture_SRV(3, Targets->PrevScratch),
			nvrhi::BindingSetItem::Texture_SRV(4, Targets->PrevDepthBuffer),
			nvrhi::BindingSetItem::Texture_SRV(5, Targets->PrevMoment),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(0, DenoiserTileMetaDataBuffer),
			nvrhi::BindingSetItem::Texture_UAV(1, Targets->CurrScratch),
			nvrhi::BindingSetItem::Texture_UAV(2, Targets->CurrMoment),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(3, DenoiserTileBuffer),
			nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Trilinear_Clamp]),
		};
		nvrhi::BindingLayoutHandle TileClassificationBindingLayout = AssetManager::GetInstance()->GetBindingLayout(TileClassificationBindingSetDesc);
		nvrhi::BindingSetHandle TileClassificationBindingSet = DirectXDevice::GetInstance()->CreateBindingSet(TileClassificationBindingSetDesc, TileClassificationBindingLayout);

		nvrhi::ComputePipelineDesc TileClassificationPipelineDesc;
		TileClassificationPipelineDesc.bindingLayouts = { TileClassificationBindingLayout };
		nvrhi::ShaderHandle TileClassificationShader = AssetManager::GetInstance()->GetShader("ffxShadowTileClassification.cs.cso");
		TileClassificationPipelineDesc.CS = TileClassificationShader;

		nvrhi::ComputeState TileClassificationState;
		TileClassificationState.pipeline = AssetManager::GetInstance()->GetComputePipeline(TileClassificationPipelineDesc);
		TileClassificationState.bindings = { TileClassificationBindingSet };
		CommandListRef->setComputeState(TileClassificationState);
		CommandListRef->dispatch(TileCountX, TileCountY, 1);
	}

	{
		struct FilterSoftShadowConstants {
			Matrix fProjectionInverse; // col_major
			Vector2 fInvBufferDimensions;
			Vector2 normalsUnpackMul_unpackAdd;
			int iBufferDimensionX;
			int iBufferDimensionY;
			float fDepthSimilaritySigma;
		} FilterSoftShadowConstants;
		FilterSoftShadowConstants.fProjectionInverse = SceneInfo.MainCameraProjWithJitter.Invert();
		FilterSoftShadowConstants.fInvBufferDimensions = Vector2(1.f / SceneInfo.RenderWidth, 1.f / SceneInfo.RenderHeight);
		FilterSoftShadowConstants.normalsUnpackMul_unpackAdd = Vector2(1.f, 0.f);
		FilterSoftShadowConstants.iBufferDimensionX = SceneInfo.RenderWidth;
		FilterSoftShadowConstants.iBufferDimensionY = SceneInfo.RenderHeight;
		FilterSoftShadowConstants.fDepthSimilaritySigma = 1.f;
		nvrhi::BufferHandle FilterSoftShadowConstantsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FilterSoftShadowConstants), "FilterSoftShadowConstants", 1));
		CommandListRef->writeBuffer(FilterSoftShadowConstantsBuffer, &FilterSoftShadowConstants, sizeof(FilterSoftShadowConstants));

		//filter soft shadow 0
		{
			nvrhi::BindingSetDesc FilterSoftShadowBindingSetDesc;
			FilterSoftShadowBindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(0, FilterSoftShadowConstantsBuffer),
				nvrhi::BindingSetItem::Texture_SRV(0, Targets->CurrDepthBuffer),
				nvrhi::BindingSetItem::Texture_SRV(1, Targets->GBufferNormal),
				nvrhi::BindingSetItem::Texture_SRV(2, Targets->CurrScratch),
				nvrhi::BindingSetItem::StructuredBuffer_UAV(0, DenoiserTileMetaDataBuffer),
				nvrhi::BindingSetItem::Texture_UAV(1, Targets->PrevScratch),
			};
			nvrhi::BindingLayoutHandle FilterSoftShadowBindingLayout = AssetManager::GetInstance()->GetBindingLayout(FilterSoftShadowBindingSetDesc);
			nvrhi::BindingSetHandle FilterSoftShadowBindingSet = DirectXDevice::GetInstance()->CreateBindingSet(FilterSoftShadowBindingSetDesc, FilterSoftShadowBindingLayout);

			nvrhi::ComputePipelineDesc FilterSoftShadowPipelineDesc;
			FilterSoftShadowPipelineDesc.bindingLayouts = { FilterSoftShadowBindingLayout };
			nvrhi::ShaderHandle FilterSoftShadowShader = AssetManager::GetInstance()->GetShader("ffxSoftShadow0.cs.cso");
			FilterSoftShadowPipelineDesc.CS = FilterSoftShadowShader;

			nvrhi::ComputeState FilterSoftShadowState;
			FilterSoftShadowState.pipeline = AssetManager::GetInstance()->GetComputePipeline(FilterSoftShadowPipelineDesc);
			FilterSoftShadowState.bindings = { FilterSoftShadowBindingSet };
			CommandListRef->setComputeState(FilterSoftShadowState);
			CommandListRef->dispatch(TileCountX, TileCountY, 1);
		}
		//filter soft shadow 1
		{
			nvrhi::BindingSetDesc FilterSoftShadowBindingSetDesc;
			FilterSoftShadowBindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(0, FilterSoftShadowConstantsBuffer),
				nvrhi::BindingSetItem::Texture_SRV(0, Targets->CurrDepthBuffer),
				nvrhi::BindingSetItem::Texture_SRV(1, Targets->GBufferNormal),
				nvrhi::BindingSetItem::Texture_SRV(2, Targets->PrevScratch),
				nvrhi::BindingSetItem::StructuredBuffer_UAV(0, DenoiserTileMetaDataBuffer),
				nvrhi::BindingSetItem::Texture_UAV(1, Targets->CurrScratch),
			};
			nvrhi::BindingLayoutHandle FilterSoftShadowBindingLayout = AssetManager::GetInstance()->GetBindingLayout(FilterSoftShadowBindingSetDesc);
			nvrhi::BindingSetHandle FilterSoftShadowBindingSet = DirectXDevice::GetInstance()->CreateBindingSet(FilterSoftShadowBindingSetDesc, FilterSoftShadowBindingLayout);

			nvrhi::ComputePipelineDesc FilterSoftShadowPipelineDesc;
			FilterSoftShadowPipelineDesc.bindingLayouts = { FilterSoftShadowBindingLayout };
			nvrhi::ShaderHandle FilterSoftShadowShader = AssetManager::GetInstance()->GetShader("ffxSoftShadow1.cs.cso");
			FilterSoftShadowPipelineDesc.CS = FilterSoftShadowShader;

			nvrhi::ComputeState FilterSoftShadowState;
			FilterSoftShadowState.pipeline = AssetManager::GetInstance()->GetComputePipeline(FilterSoftShadowPipelineDesc);
			FilterSoftShadowState.bindings = { FilterSoftShadowBindingSet };
			CommandListRef->setComputeState(FilterSoftShadowState);
			CommandListRef->dispatch(TileCountX, TileCountY, 1);
		}
		//filter soft shadow 2
		{
			nvrhi::BindingSetDesc FilterSoftShadowBindingSetDesc;
			FilterSoftShadowBindingSetDesc.bindings = {
				nvrhi::BindingSetItem::ConstantBuffer(0, FilterSoftShadowConstantsBuffer),
				nvrhi::BindingSetItem::Texture_SRV(0, Targets->CurrDepthBuffer),
				nvrhi::BindingSetItem::Texture_SRV(1, Targets->GBufferNormal),
				nvrhi::BindingSetItem::Texture_SRV(2, Targets->CurrScratch),
				nvrhi::BindingSetItem::StructuredBuffer_UAV(0, DenoiserTileMetaDataBuffer),
				nvrhi::BindingSetItem::Texture_UAV(1, Targets->ShadowMask),
			};
			nvrhi::BindingLayoutHandle FilterSoftShadowBindingLayout = AssetManager::GetInstance()->GetBindingLayout(FilterSoftShadowBindingSetDesc);
			nvrhi::BindingSetHandle FilterSoftShadowBindingSet = DirectXDevice::GetInstance()->CreateBindingSet(FilterSoftShadowBindingSetDesc, FilterSoftShadowBindingLayout);

			nvrhi::ComputePipelineDesc FilterSoftShadowPipelineDesc;
			FilterSoftShadowPipelineDesc.bindingLayouts = { FilterSoftShadowBindingLayout };
			nvrhi::ShaderHandle FilterSoftShadowShader = AssetManager::GetInstance()->GetShader("ffxSoftShadow2.cs.cso");
			FilterSoftShadowPipelineDesc.CS = FilterSoftShadowShader;

			nvrhi::ComputeState FilterSoftShadowState;
			FilterSoftShadowState.pipeline = AssetManager::GetInstance()->GetComputePipeline(FilterSoftShadowPipelineDesc);
			FilterSoftShadowState.bindings = { FilterSoftShadowBindingSet };
			CommandListRef->setComputeState(FilterSoftShadowState);
			CommandListRef->dispatch(TileCountX, TileCountY, 1);
		}
	}

	CommandListRef->endMarker();
}
