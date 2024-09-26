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

void DeferredRenderer::Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene)
{
	DeviceRef = device;
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

void DeferredRenderer::Shutdown()
{
	//release nvrhi stuff
	CommandList = nullptr;
	DepthHandle = nullptr;
}

void DeferredRenderer::BeginFrame()
{
	MICROPROFILE_SCOPEI("Render", "Begin Frame", MP_BLUE);
	DeviceRef->BeginFrame();
	DeviceRef->m_NvrhiDevice->runGarbageCollection();
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
		CommandList->clearTextureFloat(DeviceRef->GetCurrentBackbuffer(), nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(SceneColor, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AlbedoHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(NormalHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AORoughnessMetallicHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(ShadowMask, nvrhi::AllSubresources, col);
		
		CommandList->endMarker();
		CommandList->close();
		DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	}
}

void DeferredRenderer::Render(ragdoll::Scene* scene)
{
	BeginFrame();
	
	CommandList->open();

	//gbuffer
	GBufferPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
	//directional light shadow
	ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles, scene->StaticCascadeInstanceInfos, scene->SceneInfo);
	//shadow mask pass
	ShadowMaskPass->DrawShadowMask(scene->SceneInfo);
	//light scene color
	DeferredLightPass->LightPass(scene->SceneInfo);
	nvrhi::FramebufferHandle fb;
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer());
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	//tone map and gamma correct
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->ToneMap(scene->SceneInfo);
	//draw debug items
	DebugPass->SetRenderTarget(fb);
	DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);

	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
}

void DeferredRenderer::CreateResource()
{
	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();

	nvrhi::FramebufferHandle fbArr[4];
	for (int i = 0; i < 4; ++i)
	{
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
			.setDepthAttachment(ShadowMap[i]);
		fbArr[i] = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	}
	ShadowPass = std::make_shared<class ShadowPass>();
	ShadowPass->SetRenderTarget(fbArr);
	ShadowPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(ShadowMask);
	nvrhi::FramebufferHandle fb;
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	ShadowMaskPass = std::make_shared<class ShadowMaskPass>();
	ShadowMaskPass->SetRenderTarget(fb);
	ShadowMaskPass->SetDependencies(ShadowMap, DepthHandle);
	ShadowMaskPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	//create the fb for the graphics pipeline to draw on
	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(AlbedoHandle)
		.addColorAttachment(NormalHandle)
		.addColorAttachment(AORoughnessMetallicHandle)
		.setDepthAttachment(DepthHandle);
	GBuffer = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);

	GBufferPass = std::make_shared<class GBufferPass>();
	GBufferPass->SetRenderTarget(GBuffer);
	GBufferPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(SceneColor);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DeferredLightPass = std::make_shared<class DeferredLightPass>();
	DeferredLightPass->SetRenderTarget(fb);
	DeferredLightPass->SetDependencies(AlbedoHandle, NormalHandle, AORoughnessMetallicHandle, DepthHandle, ShadowMask);
	DeferredLightPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);;
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer());
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	ToneMapPass = std::make_shared<class ToneMapPass>();
	ToneMapPass->SetRenderTarget(fb);
	ToneMapPass->SetDependencies(SceneColor);
	ToneMapPass->Init(DeviceRef->m_NvrhiDevice, CommandList);
}
