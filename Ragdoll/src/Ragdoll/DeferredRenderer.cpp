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
	AlbedoHandle = scene->GBufferAlbedo;
	NormalHandle = scene->GBufferNormal;
	AORoughnessMetallicHandle = scene->GBufferORM;
	DepthHandle = scene->SceneDepthZ;
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
		col = 0.f;
		CommandList->clearTextureFloat(DeviceRef->GetCurrentBackbuffer(), nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AlbedoHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(NormalHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AORoughnessMetallicHandle, nvrhi::AllSubresources, col);
		
		CommandList->endMarker();
		CommandList->close();
		DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	}
}

void DeferredRenderer::Render(ragdoll::Scene* scene)
{
	BeginFrame();
	
	CommandList->open();

	GBufferPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);

	nvrhi::FramebufferHandle fb;
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer());
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DeferredLightPass->SetRenderTarget(fb);
	DeferredLightPass->LightPass(scene->SceneInfo);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DebugPass->SetRenderTarget(fb);
	DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);

	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
}

void DeferredRenderer::CreateResource()
{
	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();

	//create the fb for the graphics pipeline to draw on
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(AlbedoHandle)
		.addColorAttachment(NormalHandle)
		.addColorAttachment(AORoughnessMetallicHandle)
		.setDepthAttachment(DepthHandle);
	GBuffer = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);

	GBufferPass = std::make_shared<class GBufferPass>();
	GBufferPass->SetRenderTarget(GBuffer);
	GBufferPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	nvrhi::FramebufferHandle fb;
	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer());
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DeferredLightPass = std::make_shared<class DeferredLightPass>();
	DeferredLightPass->SetRenderTarget(fb);
	DeferredLightPass->SetDependencies(AlbedoHandle, NormalHandle, AORoughnessMetallicHandle, DepthHandle);
	DeferredLightPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);;
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(DeviceRef->m_NvrhiDevice, CommandList);
}
