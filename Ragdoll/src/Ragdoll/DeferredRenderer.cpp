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
		RenderTargets->CurrMoment = RenderTargets->Moment1;
		RenderTargets->PrevMoment = RenderTargets->Moment0;
		RenderTargets->CurrScratch = RenderTargets->Scratch1;
		RenderTargets->PrevScratch = RenderTargets->Scratch0;
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
		RenderTargets->PrevLuminanceHistory = RenderTargets->LuminanceHistory1;;
		RenderTargets->CurrMoment = RenderTargets->Moment0;
		RenderTargets->PrevMoment = RenderTargets->Moment1;
		RenderTargets->CurrScratch = RenderTargets->Scratch0;
		RenderTargets->PrevScratch = RenderTargets->Scratch1;
	}
	{
		//this process is long because it is waiting for the gpu to be done first
		RD_SCOPE(Render, Readback);
		//readback all the debug infos needed
		float* valPtr;
		int* intPtr;
		{
			RD_SCOPE(Render, Map);
			if (valPtr = (float*)DirectXDevice::GetNativeDevice()->mapBuffer(AutomaticExposurePass->ReadbackBuffer, nvrhi::CpuAccessMode::Read))
			{
				AdaptedLuminance = *valPtr;
			}
			DirectXDevice::GetNativeDevice()->unmapBuffer(AutomaticExposurePass->ReadbackBuffer);

			scene->DebugInfo.PassedFrustumCullCount = 0;
			if (GBufferPass->PassedFrustumTestCountBuffer)
			{
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->PassedFrustumTestCountBuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.PassedFrustumCullCount = *intPtr;
				}
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->PassedFrustumTestCountBuffer);
			}
			if (GBufferPass->Phase1NonOccludedCountBuffer && GBufferPass->Phase2NonOccludedCountBuffer)
			{
				scene->DebugInfo.PassedOcclusion1CullCount = scene->DebugInfo.PassedOcclusion2CullCount = 0;
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->Phase1NonOccludedCountBuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.PassedOcclusion1CullCount = *intPtr;
				}
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->Phase2NonOccludedCountBuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.PassedOcclusion2CullCount = *intPtr;
				}
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->Phase1NonOccludedCountBuffer);
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->Phase2NonOccludedCountBuffer);
			}

			if (GBufferPass->MeshletDegenerateConeCulledCountbuffer && GBufferPass->MeshletFrustumCulledCountBuffer)
			{
				scene->DebugInfo.MeshletConeCullCount = scene->DebugInfo.MeshletFrustumCullCount = 0;
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->MeshletDegenerateConeCulledCountbuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.MeshletConeCullCount = *intPtr;
				}
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->MeshletFrustumCulledCountBuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.MeshletFrustumCullCount = *intPtr;
				}
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->MeshletOcclusionCulledPhase1CountBuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.MeshletOcclusion1CullCount = *intPtr;
				}
				if (intPtr = (int*)DirectXDevice::GetNativeDevice()->mapBuffer(GBufferPass->MeshletOcclusionCulledPhase2CountBuffer, nvrhi::CpuAccessMode::Read))
				{
					scene->DebugInfo.MeshletOcclusion2CullCount = *intPtr;
				}
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->MeshletOcclusionCulledPhase1CountBuffer);
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->MeshletOcclusionCulledPhase2CountBuffer);
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->MeshletDegenerateConeCulledCountbuffer);
				DirectXDevice::GetNativeDevice()->unmapBuffer(GBufferPass->MeshletFrustumCulledCountBuffer);
			}
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

	uint32_t ProxyCount = scene->StaticProxies.size();
	Taskflow.emplace([this, &scene, GPUScene, ProxyCount]() {
		if (!scene->SceneInfo.bEnableMeshletShading)
		{
			GBufferPass->Draw(
				GPUScene,
				ProxyCount,
				scene->SceneInfo,
				scene->DebugInfo,
				RenderTargets,
				scene->SceneInfo.bEnableOcclusionCull);
		}
		else
		{
			GBufferPass->DrawMeshlets(
				GPUScene,
				ProxyCount,
				scene->SceneInfo,
				scene->DebugInfo,
				RenderTargets);
		}
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

	if (!scene->SceneInfo.bRaytraceDirectionalLight)
	{
		Taskflow.emplace([this, &scene, GPUScene]() {
			ShadowPass->DrawAllInstances(0, GPUScene, scene->StaticProxies.size(), scene->SceneInfo, RenderTargets);
		});
		Taskflow.emplace([this, &scene, GPUScene]() {
			ShadowPass->DrawAllInstances(1, GPUScene, scene->StaticProxies.size(), scene->SceneInfo, RenderTargets);
			});
		Taskflow.emplace([this, &scene, GPUScene]() {
			ShadowPass->DrawAllInstances(2, GPUScene, scene->StaticProxies.size(), scene->SceneInfo, RenderTargets);
			});
		Taskflow.emplace([this, &scene, GPUScene]() {
			ShadowPass->DrawAllInstances(3, GPUScene, scene->StaticProxies.size(), scene->SceneInfo, RenderTargets);
			});
		activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH0]);
		activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH1]);
		activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH2]);
		activeList.emplace_back(CommandLists[(int)Pass::SHADOW_DEPTH3]);
	}

	Taskflow.emplace([this, &scene, GPUScene]() {
		if(!scene->SceneInfo.bRaytraceDirectionalLight)
			ShadowMaskPass->DrawShadowMask(scene->SceneInfo, RenderTargets);
		else
			ShadowMaskPass->RaytraceShadowMask(scene->SceneInfo, GPUScene, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SHADOW_MASK]);

	if (scene->DebugInfo.bEnableLightGrid)
	{
		Taskflow.emplace([this, &scene, GPUScene]() {
			DeferredLightPass->LightGridPass(scene->SceneInfo, RenderTargets, GPUScene);
		});
	}
	else 
	{
		Taskflow.emplace([this, &scene, GPUScene]() {
			DeferredLightPass->LightPass(scene->SceneInfo, RenderTargets, GPUScene);
		});
	}
	activeList.emplace_back(CommandLists[(int)Pass::LIGHT]);

	Taskflow.emplace([this, &scene]() {
		SkyPass->DrawSky(scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::SKY]);

	if (scene->SceneInfo.bEnableBloom)
	{
		Taskflow.emplace([this, &scene]() {
			BloomPass->Bloom(scene->SceneInfo, RenderTargets);
			});
		activeList.emplace_back(CommandLists[(int)Pass::BLOOM]);
	}

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
		DebugPass->DrawDebug(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->LineBufferHandle, scene->LineVertices.size(), scene->SceneInfo, RenderTargets);
	});
	activeList.emplace_back(CommandLists[(int)Pass::DEBUG]);

	if (scene->DebugInfo.DbgTarget)
	{
		Taskflow.emplace([this, &scene, GPUScene]() {
			FramebufferViewer->DrawTarget(GPUScene, scene->DebugInfo.DbgTarget, scene->DebugInfo.Add, scene->DebugInfo.Mul, scene->DebugInfo.CompCount, RenderTargets);
		});
		activeList.emplace_back(CommandLists[(int)Pass::FB_VIEWER]);
	}
	else if (scene->DebugInfo.bShowLightGrid)
	{
		Taskflow.emplace([this, &scene, GPUScene]() {
			FramebufferViewer->DrawLightGridHitMap(GPUScene, scene->SceneInfo, scene->DebugInfo, RenderTargets);
			});
		activeList.emplace_back(CommandLists[(int)Pass::FB_VIEWER]);
	}
	else {
		if (!scene->SceneInfo.bEnableDLSS && !scene->SceneInfo.bEnableFSR)
		{
			Taskflow.emplace([this]() {
				FinalPass->MeshletPass(RenderTargets, false);
				});
			activeList.emplace_back(CommandLists[(int)Pass::FINAL]);
		}
		else if (scene->SceneInfo.bEnableFSR)
		{
			Taskflow.emplace([this]() {
				FinalPass->MeshletPass(RenderTargets, true);
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
	BloomPass->Init(CommandLists[(int)Pass::BLOOM], RenderTargets);

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
