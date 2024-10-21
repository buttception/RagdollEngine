#include "ragdollpch.h"
#include "DeferredRenderer.h"

#include <nvrhi/utils.h>
#include <microprofile.h>
#include <nvrhi/d3d12-backend.h>
#include "DirectXDevice.h"

#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"
#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Scene.h"
#include "GeometryBuilder.h"

void Renderer::Init(std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene)
{
	PrimaryWindowRef = win;
	//get the textures needed
	SceneColor = scene->SceneColor;
	AlbedoHandle = scene->GBufferAlbedo;
	NormalHandle = scene->GBufferNormal;
	RoughnessMetallicHandle = scene->GBufferRM;
	AOHandle = scene->AONormalized;
	VelocityBuffer = scene->VelocityBuffer;
	DepthHandle = scene->SceneDepthZ;
	ShadowMask = scene->ShadowMask;
	SkyTexture = scene->SkyTexture;
	SkyThetaGammaTable = scene->SkyThetaGammaTable;
	DeinterleavedDepth = scene->DeinterleavedDepth;
	DeinterleavedNormals = scene->DeinterleavedNormals;
	SSAOPong = scene->SSAOBufferPong;
	SSAOPing = scene->SSAOBufferPing;
	ImportanceMap = scene->ImportanceMap;
	ImportanceMapPong = scene->ImportanceMapPong;
	LoadCounter = scene->LoadCounter;
	DepthMips = scene->DepthMips;
	AOTerm = scene->AOTerm;
	FinalAOTermA = scene->FinalAOTerm;
	AOTermAccumulation = scene->AOTermAccumulation;
	Edges = scene->Edges;
	for (int i = 0; i < 4; ++i)
	{
		ShadowMap[i] = scene->ShadowMap[i];
	}
	Mips = &scene->DownsampledImages;
	CreateResource();
}

void Renderer::Shutdown()
{
	//release nvrhi stuff
	CommandList = nullptr;
	DepthHandle = nullptr;
}

void Renderer::BeginFrame()
{
	MICROPROFILE_SCOPEI("Render", "Begin Frame", MP_BLUE);
	DirectXDevice::GetInstance()->BeginFrame();
	DirectXDevice::GetNativeDevice()->runGarbageCollection();
	{
		MICROPROFILE_SCOPEGPUI("Clear targets", MP_YELLOW);
		auto bgCol = PrimaryWindowRef->GetBackgroundColor();
		nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
		CommandList->beginMarker("ClearGBuffer");
		CommandList->clearDepthStencilTexture(DepthHandle, nvrhi::AllSubresources, true, 0.f, false, 0);
		for (int i = 0; i < 4; ++i)
		{
			CommandList->clearDepthStencilTexture(ShadowMap[i], nvrhi::AllSubresources, true, 0.f, false, 0);
		}
		col = 0.f;
		CommandList->clearTextureFloat(DirectXDevice::GetInstance()->GetCurrentBackbuffer(), nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(SceneColor, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AlbedoHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(NormalHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(RoughnessMetallicHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AOHandle, nvrhi::AllSubresources, 1.f);
		CommandList->clearTextureFloat(VelocityBuffer, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(ShadowMask, nvrhi::AllSubresources, col);
		
		CommandList->endMarker();
	}
}

void Renderer::Render(ragdoll::Scene* scene, float _dt)
{
	CommandList->open();

	MICROPROFILE_GPU_SET_CONTEXT(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer, MicroProfileGetGlobalGpuThreadLog());
	{
		MICROPROFILE_SCOPEGPUI("Full Frame GPU", MP_YELLOW);
		MICROPROFILE_SCOPEI("Render", "Full Frame CPU", MP_CYAN);
		BeginFrame();

		SkyGeneratePass->GenerateSky(scene->SceneInfo);

		//gbuffer
		GBufferPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
		//ao
		if(scene->SceneInfo.UseCACAO)
			CACAOPass->GenerateAO(scene->SceneInfo);
		if (scene->SceneInfo.UseXeGTAO)
			XeGTAOPass->GenerateAO(scene->SceneInfo);
		//directional light shadow
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles, scene->StaticCascadeInstanceInfos, scene->SceneInfo);
		//shadow mask pass
		ShadowMaskPass->DrawShadowMask(scene->SceneInfo);
		//light scene color
		DeferredLightPass->LightPass(scene->SceneInfo);
		//sky
		SkyPass->DrawSky(scene->SceneInfo);
		//bloom
		BloomPass->Bloom(scene->SceneInfo);
		//get the exposure needed
		nvrhi::BufferHandle exposure = AutomaticExposurePass->GetAdaptedLuminance(_dt);

		nvrhi::FramebufferHandle fb;
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
		fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//tone map and gamma correct
		ToneMapPass->SetRenderTarget(fb);
		ToneMapPass->ToneMap(scene->SceneInfo, exposure);

		fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer())
			.setDepthAttachment(DepthHandle);
		fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);

		//after tone map for now since need its own tonemap and gamma correct
		//draw debug items
		DebugPass->SetRenderTarget(fb);
		DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);

		if (scene->DebugInfo.DbgTarget)
		{
			FramebufferViewer->DrawTarget(scene->DebugInfo.DbgTarget, scene->DebugInfo.Add, scene->DebugInfo.Mul, scene->DebugInfo.CompCount);
		}
	}

	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	MicroProfileFlip(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer);

	//readback all the debug infos needed
	float* valPtr;
	if (valPtr = (float*)DirectXDevice::GetNativeDevice()->mapBuffer(AutomaticExposurePass->ReadbackBuffer, nvrhi::CpuAccessMode::Read))
	{
		AdaptedLuminance = *valPtr;
	}
	DirectXDevice::GetNativeDevice()->unmapBuffer(AutomaticExposurePass->ReadbackBuffer);
}

void Renderer::CreateResource()
{
	CommandList = DirectXDevice::GetNativeDevice()->createCommandList();

	SkyGeneratePass = std::make_shared<class SkyGeneratePass>();
	SkyGeneratePass->SetDependencies(SkyTexture, SkyThetaGammaTable);
	SkyGeneratePass->Init(CommandList);

	nvrhi::FramebufferHandle fbArr[4];
	for (int i = 0; i < 4; ++i)
	{
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.setDepthAttachment(ShadowMap[i]);
		fbArr[i] = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	}
	ShadowPass = std::make_shared<class ShadowPass>();
	ShadowPass->SetRenderTarget(fbArr);
	ShadowPass->Init(CommandList);

	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(ShadowMask);
	nvrhi::FramebufferHandle fb;
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ShadowMaskPass = std::make_shared<class ShadowMaskPass>();
	ShadowMaskPass->SetRenderTarget(fb);
	ShadowMaskPass->SetDependencies(ShadowMap, DepthHandle);
	ShadowMaskPass->Init(CommandList);

	//create the fb for the graphics pipeline to draw on
	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(AlbedoHandle)
		.addColorAttachment(NormalHandle)
		.addColorAttachment(RoughnessMetallicHandle)
		.addColorAttachment(VelocityBuffer)
		.setDepthAttachment(DepthHandle);
	GBuffer = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);

	GBufferPass = std::make_shared<class GBufferPass>();
	GBufferPass->SetRenderTarget(GBuffer);
	GBufferPass->Init(CommandList);

	CACAOPass = std::make_shared<class CACAOPass>();
	CACAOPass->SetDependencies({ DepthHandle, NormalHandle, LoadCounter, DeinterleavedDepth, DeinterleavedNormals, SSAOPong, SSAOPing, ImportanceMap, ImportanceMapPong, AOHandle });
	CACAOPass->Init(CommandList);

	XeGTAOPass = std::make_shared<class XeGTAOPass>();
	XeGTAOPass->SetDependencies({ DepthHandle, NormalHandle, AOHandle, DepthMips, AOTerm, Edges, FinalAOTermA, AOTermAccumulation, VelocityBuffer });
	XeGTAOPass->Init(CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DeferredLightPass = std::make_shared<class DeferredLightPass>();
	DeferredLightPass->SetRenderTarget(fb);
	DeferredLightPass->SetDependencies(AlbedoHandle, NormalHandle, RoughnessMetallicHandle, AOHandle, DepthHandle, ShadowMask);
	DeferredLightPass->Init(CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor)
		.setDepthAttachment(DepthHandle);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	SkyPass = std::make_shared<class SkyPass>();
	SkyPass->SetRenderTarget(fb);
	SkyPass->SetDependencies(SkyTexture);
	SkyPass->Init(CommandList);
	
	BloomPass = std::make_shared<class BloomPass>();
	BloomPass->SetDependencies(SceneColor, Mips);
	BloomPass->Init(CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	AutomaticExposurePass = std::make_shared<class AutomaticExposurePass>();
	AutomaticExposurePass->SetDependencies(SceneColor);
	AutomaticExposurePass->Init(CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ToneMapPass = std::make_shared<class ToneMapPass>();
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->SetDependencies(SceneColor);
	ToneMapPass->Init(CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);;
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(CommandList);

	FramebufferViewer = std::make_shared<class FramebufferViewer>();
	FramebufferViewer->Init(CommandList);
}
