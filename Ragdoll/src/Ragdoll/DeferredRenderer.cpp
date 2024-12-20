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
#include "GPUScene.h"
#include "GeometryBuilder.h"
#include "ImGuiRenderer.h"
#include "NVSDK.h"

#include "Executor.h"
#include "Profiler.h"

void Renderer::Init(std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene)
{
	PrimaryWindowRef = win;
	//get the textures needed
	RenderTargets = &scene->RenderTargets;
	CreateResource();
}

void Renderer::Shutdown()
{
	//release nvrhi stuff
	CommandList = nullptr;
	RenderTargets = nullptr;
}

void Renderer::BeginFrame()
{
	RD_SCOPE(Load, Begin Frame);
	DirectXDevice::GetInstance()->BeginFrame();
	RD_GPU_SCOPE("Clear Targets", CommandList);
	auto bgCol = PrimaryWindowRef->GetBackgroundColor();
	nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
	CommandList->beginMarker("ClearGBuffer");
	CommandList->clearDepthStencilTexture(RenderTargets->CurrDepthBuffer, nvrhi::AllSubresources, true, 0.f, false, 0);
	for (int i = 0; i < 4; ++i)
	{
		CommandList->clearDepthStencilTexture(RenderTargets->ShadowMap[i], nvrhi::AllSubresources, true, 0.f, false, 0);
	}
	col = 0.f;
	CommandList->clearTextureFloat(DirectXDevice::GetInstance()->GetCurrentBackbuffer(), nvrhi::AllSubresources, col);
	CommandList->clearTextureFloat(RenderTargets->SceneColor, nvrhi::AllSubresources, col);
	CommandList->clearTextureFloat(RenderTargets->GBufferAlbedo, nvrhi::AllSubresources, col);
	CommandList->clearTextureFloat(RenderTargets->GBufferNormal, nvrhi::AllSubresources, col);
	CommandList->clearTextureFloat(RenderTargets->GBufferRM, nvrhi::AllSubresources, col);
	CommandList->clearTextureFloat(RenderTargets->AONormalized, nvrhi::AllSubresources, 1.f);
	CommandList->clearTextureFloat(RenderTargets->VelocityBuffer, nvrhi::AllSubresources, col);
	CommandList->clearTextureFloat(RenderTargets->ShadowMask, nvrhi::AllSubresources, col);
		
	CommandList->endMarker();
}

void Renderer::Render(ragdoll::Scene* scene, ragdoll::FGPUScene* GPUScene, float _dt, std::shared_ptr<ImguiRenderer> imgui)
{
	//swaps the depth buffer being drawn to
	bIsOddFrame = !bIsOddFrame;
	if (bIsOddFrame)
	{
		RenderTargets->CurrDepthBuffer = RenderTargets->SceneDepthZ1;
		RenderTargets->PrevDepthBuffer = RenderTargets->SceneDepthZ0;
		RenderTargets->CurrUpscaledBuffer = RenderTargets->UpscaledBuffer1;
		RenderTargets->PrevUpscaledBuffer = RenderTargets->UpscaledBuffer0;
		RenderTargets->CurrLuminance = RenderTargets->Luminance1;
		RenderTargets->PrevLuminance = RenderTargets->Luminance0;
		RenderTargets->CurrAccumulation = RenderTargets->Accumulation1;
		RenderTargets->PrevAccumulation = RenderTargets->Accumulation0;
		RenderTargets->CurrLuminanceHistory = RenderTargets->LuminanceHistory1;
		RenderTargets->PrevLuminanceHistory = RenderTargets->LuminanceHistory0;
	}
	else
	{
		RenderTargets->CurrDepthBuffer = RenderTargets->SceneDepthZ0;
		RenderTargets->PrevDepthBuffer = RenderTargets->SceneDepthZ1;
		RenderTargets->CurrUpscaledBuffer = RenderTargets->UpscaledBuffer0;
		RenderTargets->PrevUpscaledBuffer = RenderTargets->UpscaledBuffer1;
		RenderTargets->CurrLuminance = RenderTargets->Luminance0;
		RenderTargets->PrevLuminance = RenderTargets->Luminance1;
		RenderTargets->CurrAccumulation = RenderTargets->Accumulation0;
		RenderTargets->PrevAccumulation = RenderTargets->Accumulation1;
		RenderTargets->CurrLuminanceHistory = RenderTargets->LuminanceHistory0;
		RenderTargets->PrevLuminanceHistory = RenderTargets->LuminanceHistory1;
	}
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
		SkyGeneratePass->GenerateSky(scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SKY_GENERATE]);

	Taskflow.emplace([this, &scene, GPUScene]() {
		GBufferPass->DrawAllInstances(GPUScene, scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::GBUFFER]);

	if (scene->SceneInfo.UseCACAO)
	{
		Taskflow.emplace([this, &scene]() {
			CACAOPass->GenerateAO(scene->SceneInfo, RenderTargets);
		});
		activeList.emplace_back(CommandLists[(int)Pass::AO]);
	}
	if (scene->SceneInfo.UseXeGTAO)
	{
		Taskflow.emplace([this, &scene]() {
			XeGTAOPass->GenerateAO(scene->SceneInfo, RenderTargets);
		});
		activeList.emplace_back(CommandLists[(int)Pass::AO]);
	}

	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[0], scene->StaticCascadeInstanceInfos[0], scene->SceneInfo, 0, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH0]);
	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[1], scene->StaticCascadeInstanceInfos[1], scene->SceneInfo, 1, RenderTargets);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH1]);
	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[2], scene->StaticCascadeInstanceInfos[2], scene->SceneInfo, 2, RenderTargets);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH2]);
	Taskflow.emplace([this, &scene]() {
		ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles[3], scene->StaticCascadeInstanceInfos[3], scene->SceneInfo, 3, RenderTargets);
		});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH3]);

	Taskflow.emplace([this, &scene]() {
		ShadowMaskPass->DrawShadowMask(scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_MASK]);

	Taskflow.emplace([this, &scene]() {
		DeferredLightPass->LightPass(scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::LIGHT]);

	Taskflow.emplace([this, &scene]() {
		SkyPass->DrawSky(scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SKY]);

	Taskflow.emplace([this, &scene]() {
		BloomPass->Bloom(scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::BLOOM]);

	Taskflow.emplace([this, _dt]() {
		AutomaticExposurePass->GetAdaptedLuminance(_dt, RenderTargets);
		});
	activeList.emplace_back(CommandLists[(int)Pass::EXPOSURE]);

	nvrhi::BufferHandle exposure = AutomaticExposurePass->AdaptedLuminanceHandle;
	Taskflow.emplace([this, &scene, exposure]() {
		ToneMapPass->ToneMap(scene->SceneInfo, exposure, RenderTargets);
		});

	activeList.emplace_back(CommandLists[(int)Pass::TONEMAP]);

	if (scene->SceneInfo.bEnableFSR)
	{
		Taskflow.emplace([this, &scene, _dt]() {
			FSRPass->Upscale(scene->SceneInfo, RenderTargets, _dt);
			});
		activeList.emplace_back(CommandLists[(int)Pass::TAA]);
	}
	
	if (scene->SceneInfo.bEnableIntelTAA)
	{
		Taskflow.emplace([this, &scene, _dt]() {
			IntelTAAPass->TemporalAA(RenderTargets, scene->SceneInfo, Vector2(scene->JitterOffsetsX[scene->PhaseIndex], scene->JitterOffsetsY[scene->PhaseIndex]));
			});
		activeList.emplace_back(CommandLists[(int)Pass::TAA]);
	}

	Taskflow.emplace([this, &scene]() {
		DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::DEBUG]);

	if (scene->DebugInfo.DbgTarget)
	{
		Taskflow.emplace([this, &scene]() {
			FramebufferViewer->DrawTarget(scene->DebugInfo.DbgTarget, scene->DebugInfo.Add, scene->DebugInfo.Mul, scene->DebugInfo.CompCount, RenderTargets);
		});
		activeList.emplace_back(CommandLists[(int)Pass::FB_VIEWER]);
	}
	else {
		if (!scene->SceneInfo.bEnableDLSS && !scene->SceneInfo.bEnableFSR)
		{
			Taskflow.emplace([this]() {
				FinalPass->DrawQuad(RenderTargets, false);
				});
			activeList.emplace_back(CommandLists[(int)Pass::FINAL]);
		}
		else if (scene->SceneInfo.bEnableFSR)
		{
			Taskflow.emplace([this]() {
				FinalPass->DrawQuad(RenderTargets, true);
				});
			activeList.emplace_back(CommandLists[(int)Pass::FINAL]);
		}
	}

	Taskflow.emplace([&imgui]() {
		imgui->Render();
	});

	SExecutor::Executor.run(Taskflow).wait();
	//submit the logs in the order of executions
	{
		RD_SCOPE(Render, ExecuteCommandList);
		MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, CommandList->Work);
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
		RD_GPU_SUBMIT(activeList);
		DirectXDevice::GetNativeDevice()->executeCommandLists(activeList.data(), activeList.size());
	}

	if (scene->Config.bInitDLSS && scene->SceneInfo.bEnableDLSS && !scene->DebugInfo.DbgTarget)
	{
		//DLSS pass
		NVSDK::Evaluate(RenderTargets->FinalColor, RenderTargets->PresentationBuffer, RenderTargets->CurrDepthBuffer, RenderTargets->VelocityBuffer, scene);

		FinalPass->DrawQuad(RenderTargets, true);
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
	SkyGeneratePass->Init(CommandLists[(int)Pass::SKY_GENERATE]);

	ShadowPass = std::make_shared<class ShadowPass>();
	ShadowPass->Init(CommandLists[(int)Pass::SHADOW_DEPTH0], CommandLists[(int)Pass::SHADOW_DEPTH1], CommandLists[(int)Pass::SHADOW_DEPTH2], CommandLists[(int)Pass::SHADOW_DEPTH3]);

	ShadowMaskPass = std::make_shared<class ShadowMaskPass>();
	ShadowMaskPass->Init(CommandLists[(int)Pass::SHADOW_MASK]);

	GBufferPass = std::make_shared<class GBufferPass>();
	GBufferPass->Init(CommandLists[(int)Pass::GBUFFER]);

	CACAOPass = std::make_shared<class CACAOPass>();
	CACAOPass->Init(CommandLists[(int)Pass::AO]);

	XeGTAOPass = std::make_shared<class XeGTAOPass>();
	XeGTAOPass->Init(CommandLists[(int)Pass::AO]);

	DeferredLightPass = std::make_shared<class DeferredLightPass>();
	DeferredLightPass->Init(CommandLists[(int)Pass::LIGHT]);

	SkyPass = std::make_shared<class SkyPass>();
	SkyPass->Init(CommandLists[(int)Pass::SKY]);
	
	BloomPass = std::make_shared<class BloomPass>();
	BloomPass->Init(CommandLists[(int)Pass::BLOOM]);

	AutomaticExposurePass = std::make_shared<class AutomaticExposurePass>();
	AutomaticExposurePass->Init(CommandLists[(int)Pass::EXPOSURE]);

	ToneMapPass = std::make_shared<class ToneMapPass>();
	ToneMapPass->Init(CommandLists[(int)Pass::TONEMAP]);

	IntelTAAPass = std::make_shared<class IntelTAAPass>();
	IntelTAAPass->Init(CommandLists[(int)Pass::TAA]);

	FSRPass = std::make_shared<class FSRPass>();
	FSRPass->Init(CommandLists[(int)Pass::TAA]);

	FinalPass = std::make_shared<class FinalPass>();
	FinalPass->Init(CommandLists[(int)Pass::FINAL]);

	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->Init(CommandLists[(int)Pass::DEBUG]);

	FramebufferViewer = std::make_shared<class FramebufferViewer>();
	FramebufferViewer->Init(CommandLists[(int)Pass::FB_VIEWER]);
}
