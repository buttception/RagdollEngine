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
	AORoughnessMetallicHandle = scene->GBufferORM;
	DepthHandle = scene->SceneDepthZ;
	ShadowMask = scene->ShadowMask;
	for (int i = 0; i < 4; ++i)
	{
		ShadowMap[i] = scene->ShadowMap[i];
	}
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
		auto bgCol = PrimaryWindowRef->GetBackgroundColor();
		nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
		CommandList->open();
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
		CommandList->clearTextureFloat(AORoughnessMetallicHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(ShadowMask, nvrhi::AllSubresources, col);
		
		CommandList->endMarker();
		CommandList->close();
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	}
}

void Renderer::Render(ragdoll::Scene* scene, float _dt)
{
	BeginFrame();
	
	CommandList->open();

	//gbuffer
	GBufferPass->SetDependencies(AssetManager::GetInstance()->GetShader("GBufferShader.vs.cso"), AssetManager::GetInstance()->GetShader("GBufferShader.ps.cso"));
	GBufferPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
	//directional light shadow
	ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles, scene->StaticCascadeInstanceInfos, scene->SceneInfo);
	//shadow mask pass
	ShadowMaskPass->DrawShadowMask(scene->SceneInfo);
	//light scene color
	DeferredLightPass->LightPass(scene->SceneInfo);
	//get the exposure needed
	nvrhi::BufferHandle exposure = AutomaticExposurePass->GetAdaptedLuminance(_dt);

	nvrhi::FramebufferHandle fb;
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	//tone map and gamma correct
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->ToneMap(scene->SceneInfo, exposure);

	fbDesc.setDepthAttachment(DepthHandle);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	//draw debug items
	DebugPass->SetRenderTarget(fb);
	DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);

	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);

	//readback all the debug infos needed
	/*float* valPtr;
	if (valPtr = (float*)DeviceRef->m_NvrhiDevice->mapBuffer(AutomaticExposurePass->ReadbackBuffer, nvrhi::CpuAccessMode::Read))
	{
		AdaptedLuminance = *valPtr;
	}
	DeviceRef->m_NvrhiDevice->unmapBuffer(AutomaticExposurePass->ReadbackBuffer);*/
}

void Renderer::CreateResource()
{
	CommandList = DirectXDevice::GetNativeDevice()->createCommandList();

	nvrhi::FramebufferHandle fbArr[4];
	for (int i = 0; i < 4; ++i)
	{
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.setDepthAttachment(ShadowMap[i]);
		fbArr[i] = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	}
	ShadowPass = std::make_shared<class ShadowPass>();
	ShadowPass->SetRenderTarget(fbArr);
	ShadowPass->Init(DirectXDevice::GetNativeDevice(), CommandList);

	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(ShadowMask);
	nvrhi::FramebufferHandle fb;
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ShadowMaskPass = std::make_shared<class ShadowMaskPass>();
	ShadowMaskPass->SetRenderTarget(fb);
	ShadowMaskPass->SetDependencies(ShadowMap, DepthHandle);
	ShadowMaskPass->Init(DirectXDevice::GetNativeDevice(), CommandList);

	//create the fb for the graphics pipeline to draw on
	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(AlbedoHandle)
		.addColorAttachment(NormalHandle)
		.addColorAttachment(AORoughnessMetallicHandle)
		.setDepthAttachment(DepthHandle);
	GBuffer = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);

	GBufferPass = std::make_shared<class GBufferPass>();
	GBufferPass->SetRenderTarget(GBuffer);
	GBufferPass->Init(CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor);
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DeferredLightPass = std::make_shared<class DeferredLightPass>();
	DeferredLightPass->SetRenderTarget(fb);
	DeferredLightPass->SetDependencies(AlbedoHandle, NormalHandle, AORoughnessMetallicHandle, DepthHandle, ShadowMask);
	DeferredLightPass->Init(DirectXDevice::GetNativeDevice(), CommandList);

	AutomaticExposurePass = std::make_shared<class AutomaticExposurePass>();
	AutomaticExposurePass->SetDependencies(SceneColor);
	AutomaticExposurePass->Init(DirectXDevice::GetNativeDevice(), CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer());
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	ToneMapPass = std::make_shared<class ToneMapPass>();
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->SetDependencies(SceneColor);
	ToneMapPass->Init(DirectXDevice::GetNativeDevice(), CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DirectXDevice::GetInstance()->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);;
	fb = DirectXDevice::GetNativeDevice()->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(DirectXDevice::GetNativeDevice(), CommandList);
}
