#include "ragdollpch.h"
#include "DeferredRenderer.h"

#include <nvrhi/utils.h>
#include <nvrhi/d3d12-backend.h>
#include "DirectXDevice.h"

#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"
#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Scene.h"
#include "GeometryBuilder.h"

#include "Executor.h"
#include "Profiler.h"

#define MULTITHREAD_RENDER 1

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
	PROFILE_SCOPE(Load, Begin Frame);
	DirectXDevice::GetInstance()->BeginFrame();
	DirectXDevice::GetNativeDevice()->runGarbageCollection();
	{
		////MICROPROFILE_SCOPEGPUI("Clear targets", MP_YELLOW);
		CommandList->open();
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
		CommandList->close();
	}
}

void Renderer::Render(ragdoll::Scene* scene, float _dt)
{
#if MULTITHREAD_RENDER == 1
	PROFILE_SCOPE(Render, Full Frame CPU)
	tf::Taskflow Taskflow;
	std::vector<nvrhi::ICommandList*> activeList;
	Taskflow.emplace([this]() {
		BeginFrame();
		});

	Taskflow.emplace([this, &scene]() {
		SkyGeneratePass->GenerateSky(scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SKY_GENERATE]);

	Taskflow.emplace([this, &scene]() {
		GBufferPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::GBUFFER]);

	if (scene->SceneInfo.UseCACAO)
	{
		Taskflow.emplace([this, &scene]() {
			CACAOPass->GenerateAO(scene->SceneInfo);
			});
		activeList.emplace_back(CommandLists[(int)Pass::AO]);
	}
	if (scene->SceneInfo.UseXeGTAO)
	{
		Taskflow.emplace([this, &scene]() {
			XeGTAOPass->GenerateAO(scene->SceneInfo);
			});
		activeList.emplace_back(CommandLists[(int)Pass::AO]);
	}

	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles, scene->StaticCascadeInstanceInfos, scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH]);

	Taskflow.emplace([this, &scene]() {
		ShadowMaskPass->DrawShadowMask(scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_MASK]);

	Taskflow.emplace([this, &scene]() {
		DeferredLightPass->LightPass(scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::LIGHT]);

	Taskflow.emplace([this, &scene]() {
		SkyPass->DrawSky(scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SKY]);

	Taskflow.emplace([this, &scene]() {
		BloomPass->Bloom(scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::BLOOM]);

	Taskflow.emplace([this, _dt]() {
		AutomaticExposurePass->GetAdaptedLuminance(_dt);
		});
	activeList.emplace_back(CommandLists[(int)Pass::EXPOSURE]);

	nvrhi::BufferHandle exposure = AutomaticExposurePass->AdaptedLuminanceHandle;
	Taskflow.emplace([this, &scene, exposure]() {
		nvrhi::FramebufferHandle fb;
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
		fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//tone map and gamma correct
		ToneMapPass->SetRenderTarget(fb);
		ToneMapPass->ToneMap(scene->SceneInfo, exposure);
		});
	activeList.emplace_back(CommandLists[(int)Pass::TONEMAP]);

	Taskflow.emplace([this, &scene]() {
		nvrhi::FramebufferHandle fb;
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer())
			.setDepthAttachment(DepthHandle);
		fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);

		//after tone map for now since need its own tonemap and gamma correct
		//draw debug items
		DebugPass->SetRenderTarget(fb);
		DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);
		});
	activeList.emplace_back(CommandLists[(int)Pass::DEBUG]);

	if (scene->DebugInfo.DbgTarget)
	{
		Taskflow.emplace([this, &scene]() {
			FramebufferViewer->DrawTarget(scene->DebugInfo.DbgTarget, scene->DebugInfo.Add, scene->DebugInfo.Mul, scene->DebugInfo.CompCount);
			});
		activeList.emplace_back(CommandLists[(int)Pass::FB_VIEWER]);
	}

	SExecutor::Executor.run(Taskflow).wait();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	DirectXDevice::GetNativeDevice()->executeCommandLists(activeList.data(), activeList.size());
#else
	MICROPROFILE_GPU_SET_CONTEXT(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer, MicroProfileGetGlobalGpuThreadLog());
	std::vector<nvrhi::ICommandList*> activeList;
	{
		//MICROPROFILE_SCOPEGPUI("Full Frame GPU", MP_YELLOW);
		MICROPROFILE_SCOPEI("Render", "Full Frame CPU", MP_CYAN);
		BeginFrame();

		SkyGeneratePass->GenerateSky(scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::SKY_GENERATE]);

		//gbuffer
		GBufferPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::GBUFFER]);
		//ao
		if(scene->SceneInfo.UseCACAO)
		{
			CACAOPass->GenerateAO(scene->SceneInfo);
			activeList.emplace_back(CommandLists[(int)Pass::AO]);
		}
		if (scene->SceneInfo.UseXeGTAO)
		{
			XeGTAOPass->GenerateAO(scene->SceneInfo);
			activeList.emplace_back(CommandLists[(int)Pass::AO]);
		}
		//directional light shadow
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles, scene->StaticCascadeInstanceInfos, scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH]);
		//shadow mask pass
		ShadowMaskPass->DrawShadowMask(scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::SHADOW_MASK]);
		//light scene color
		DeferredLightPass->LightPass(scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::LIGHT]);
		//sky
		SkyPass->DrawSky(scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::SKY]);
		//bloom
		BloomPass->Bloom(scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::BLOOM]);
		//get the exposure needed
		nvrhi::BufferHandle exposure = AutomaticExposurePass->GetAdaptedLuminance(_dt);
		activeList.emplace_back(CommandLists[(int)Pass::EXPOSURE]);

		nvrhi::FramebufferHandle fb;
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
		fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
		//tone map and gamma correct
		ToneMapPass->SetRenderTarget(fb);
		ToneMapPass->ToneMap(scene->SceneInfo, exposure);
		activeList.emplace_back(CommandLists[(int)Pass::TONEMAP]);

		fbDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer())
			.setDepthAttachment(DepthHandle);
		fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);

		//after tone map for now since need its own tonemap and gamma correct
		//draw debug items
		DebugPass->SetRenderTarget(fb);
		DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);
		activeList.emplace_back(CommandLists[(int)Pass::DEBUG]);

		if (scene->DebugInfo.DbgTarget)
		{
			FramebufferViewer->DrawTarget(scene->DebugInfo.DbgTarget, scene->DebugInfo.Add, scene->DebugInfo.Mul, scene->DebugInfo.CompCount);
			activeList.emplace_back(CommandLists[(int)Pass::FB_VIEWER]);
		}
	}

	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList); //command list to clear screen
	DirectXDevice::GetNativeDevice()->executeCommandLists(activeList.data(), activeList.size());
	//MicroProfileFlip(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer);
#endif

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
	MICROPROFILE_SCOPEI("Render", "Create Passes", MP_YELLOW4);
	CommandList = DirectXDevice::GetNativeDevice()->createCommandList();

	CommandLists.resize((int)Pass::COUNT);
	for (nvrhi::CommandListHandle& cmdList : CommandLists) {
		cmdList = DirectXDevice::GetNativeDevice()->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
	}

	SkyGeneratePass = std::make_shared<class SkyGeneratePass>();
	SkyGeneratePass->SetDependencies(SkyTexture, SkyThetaGammaTable);
	SkyGeneratePass->Init(CommandLists[(int)Pass::SKY_GENERATE]);

	nvrhi::FramebufferHandle fbArr[4];
	for (int i = 0; i < 4; ++i)
	{
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.setDepthAttachment(ShadowMap[i]);
		fbArr[i] = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	}
	ShadowPass = std::make_shared<class ShadowPass>();
	ShadowPass->SetRenderTarget(fbArr);
	ShadowPass->Init(CommandLists[(int)Pass::SHADOW_DEPTH]);

	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(ShadowMask);
	nvrhi::FramebufferHandle fb;
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ShadowMaskPass = std::make_shared<class ShadowMaskPass>();
	ShadowMaskPass->SetRenderTarget(fb);
	ShadowMaskPass->SetDependencies(ShadowMap, DepthHandle);
	ShadowMaskPass->Init(CommandLists[(int)Pass::SHADOW_MASK]);

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
	GBufferPass->Init(CommandLists[(int)Pass::GBUFFER]);

	CACAOPass = std::make_shared<class CACAOPass>();
	CACAOPass->SetDependencies({ DepthHandle, NormalHandle, LoadCounter, DeinterleavedDepth, DeinterleavedNormals, SSAOPong, SSAOPing, ImportanceMap, ImportanceMapPong, AOHandle });
	CACAOPass->Init(CommandLists[(int)Pass::AO]);

	XeGTAOPass = std::make_shared<class XeGTAOPass>();
	XeGTAOPass->SetDependencies({ DepthHandle, NormalHandle, AOHandle, DepthMips, AOTerm, Edges, FinalAOTermA, AOTermAccumulation, VelocityBuffer });
	XeGTAOPass->Init(CommandLists[(int)Pass::AO]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DeferredLightPass = std::make_shared<class DeferredLightPass>();
	DeferredLightPass->SetRenderTarget(fb);
	DeferredLightPass->SetDependencies(AlbedoHandle, NormalHandle, RoughnessMetallicHandle, AOHandle, DepthHandle, ShadowMask);
	DeferredLightPass->Init(CommandLists[(int)Pass::LIGHT]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor)
		.setDepthAttachment(DepthHandle);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	SkyPass = std::make_shared<class SkyPass>();
	SkyPass->SetRenderTarget(fb);
	SkyPass->SetDependencies(SkyTexture);
	SkyPass->Init(CommandLists[(int)Pass::SKY]);
	
	BloomPass = std::make_shared<class BloomPass>();
	BloomPass->SetDependencies(SceneColor, Mips);
	BloomPass->Init(CommandLists[(int)Pass::BLOOM]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	AutomaticExposurePass = std::make_shared<class AutomaticExposurePass>();
	AutomaticExposurePass->SetDependencies(SceneColor);
	AutomaticExposurePass->Init(CommandLists[(int)Pass::EXPOSURE]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ToneMapPass = std::make_shared<class ToneMapPass>();
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->SetDependencies(SceneColor);
	ToneMapPass->Init(CommandLists[(int)Pass::TONEMAP]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);;
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(CommandLists[(int)Pass::DEBUG]);

	FramebufferViewer = std::make_shared<class FramebufferViewer>();
	FramebufferViewer->Init(CommandLists[(int)Pass::FB_VIEWER]);
}
