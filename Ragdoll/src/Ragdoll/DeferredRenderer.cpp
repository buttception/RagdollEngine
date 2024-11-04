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
#include "ImGuiRenderer.h"
#include "NVSDK.h"

#include "Executor.h"
#include "Profiler.h"

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
	FinalColor = scene->FinalColor;
	Edges = scene->Edges;
	UpscaledBuffer = scene->UpscaledBuffer;
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
	RD_SCOPE(Load, Begin Frame);
	DirectXDevice::GetInstance()->BeginFrame();
	RD_GPU_SCOPE("Clear Targets", CommandList);
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

void Renderer::Render(ragdoll::Scene* scene, float _dt, std::shared_ptr<ImguiRenderer> imgui)
{
	{
		//this process is long because it is waiting for the gpu to be done first
		RD_SCOPE(Render, Readback);
		//readback all the debug infos needed
		float* valPtr;
		{
			RD_SCOPE(Render, Map);
			if (valPtr = (float*)DirectXDevice::GetNativeDevice()->mapBuffer(AutomaticExposurePass->ReadbackBuffer, nvrhi::CpuAccessMode::Read))
			{
				AdaptedLuminance = *valPtr;
			}
		}
		{
			RD_SCOPE(Render, Unmap);
			DirectXDevice::GetNativeDevice()->unmapBuffer(AutomaticExposurePass->ReadbackBuffer);
		}
	}

	RD_SCOPE(Render, Full Frame)
	tf::Taskflow Taskflow;
	std::vector<nvrhi::ICommandList*> activeList;
	Taskflow.emplace([this]() {
		DirectXDevice::GetNativeDevice()->runGarbageCollection();
	});

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
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[0], scene->StaticCascadeInstanceInfos[0], scene->SceneInfo, 0);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH0]);
	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[1], scene->StaticCascadeInstanceInfos[1], scene->SceneInfo, 1);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH1]);
	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[2], scene->StaticCascadeInstanceInfos[2], scene->SceneInfo, 2);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH2]);
	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[3], scene->StaticCascadeInstanceInfos[3], scene->SceneInfo, 3);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH3]);

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
		ToneMapPass->ToneMap(scene->SceneInfo, exposure);
	});
	activeList.emplace_back(CommandLists[(int)Pass::TONEMAP]);

	Taskflow.emplace([this, &scene]() {
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
	
	if (!scene->SceneInfo.bEnableDLSS)
	{
		Taskflow.emplace([this]() {
			nvrhi::FramebufferDesc desc;
			desc.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
			nvrhi::FramebufferHandle fb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
			FinalPass->SetRenderTarget(fb);
			FinalPass->SetDependencies(FinalColor);
			FinalPass->DrawQuad();
			});
		activeList.emplace_back(CommandLists[(int)Pass::FINAL]);
	}

	Taskflow.emplace([&imgui]() {
		imgui->Render();
	});

	SExecutor::Executor.run(Taskflow).wait();
	//submit the logs in the order of execution
	{
		RD_SCOPE(Render, ExecuteCommandList);
		MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, CommandList->Work);
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
		RD_GPU_SUBMIT(activeList);
		DirectXDevice::GetNativeDevice()->executeCommandLists(activeList.data(), activeList.size());
	}

	if (scene->SceneInfo.bEnableDLSS && !scene->DebugInfo.DbgTarget)
	{
		//DLSS pass
		NVSDK::Evaluate(FinalColor, UpscaledBuffer, DepthHandle, VelocityBuffer, scene);

		nvrhi::FramebufferDesc desc;
		desc.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
		nvrhi::FramebufferHandle fb = DirectXDevice::GetNativeDevice()->createFramebuffer(desc);
		FinalPass->SetRenderTarget(fb);
		FinalPass->SetDependencies(UpscaledBuffer);
		FinalPass->DrawQuad();
		MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, CommandLists[(int)Pass::FINAL]->Work);
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandLists[(int)Pass::FINAL]);
	}

	MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, imgui->CommandList->Work);
	DirectXDevice::GetNativeDevice()->executeCommandList(imgui->CommandList);

	EnterCommandListSectionGpu::Reset();
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
	ShadowPass->Init(CommandLists[(int)Pass::SHADOW_DEPTH0], CommandLists[(int)Pass::SHADOW_DEPTH1], CommandLists[(int)Pass::SHADOW_DEPTH2], CommandLists[(int)Pass::SHADOW_DEPTH3]);

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
		.addColorAttachment(FinalColor);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ToneMapPass = std::make_shared<class ToneMapPass>();
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->SetDependencies(SceneColor);
	ToneMapPass->Init(CommandLists[(int)Pass::TONEMAP]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	FinalPass = std::make_shared<class FinalPass>();
	FinalPass->SetRenderTarget(fb);
	FinalPass->SetDependencies(FinalColor);
	FinalPass->Init(CommandLists[(int)Pass::FINAL]);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor)
		.setDepthAttachment(DepthHandle);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(CommandLists[(int)Pass::DEBUG]);

	FramebufferViewer = std::make_shared<class FramebufferViewer>();
	FramebufferViewer->Init(CommandLists[(int)Pass::FB_VIEWER]);
}
