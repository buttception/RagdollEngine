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

void DeferredRenderer::Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win)
{
	DeviceRef = device;
	PrimaryWindowRef = win;
	CreateResource();
}

void DeferredRenderer::Shutdown()
{
	//release nvrhi stuff
	CommandList = nullptr;
	DepthBuffer = nullptr;
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
		CommandList->clearDepthStencilTexture(DepthBuffer, nvrhi::AllSubresources, true, 0.f, false, 0);
		col = 0.f;
		CommandList->clearTextureFloat(DeviceRef->GetCurrentBackbuffer(), nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(AlbedoHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(NormalHandle, nvrhi::AllSubresources, col);
		CommandList->clearTextureFloat(RoughnessMetallicHandle, nvrhi::AllSubresources, col);
		
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
		.setDepthAttachment(DepthBuffer);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DebugPass->SetRenderTarget(fb);
	DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);

	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
}

void DeferredRenderer::CreateResource()
{
	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();
	//check if the depth buffer i want is already in the asset manager
	if (!AssetManager::GetInstance()->RenderTargetTextures.contains("SceneDepthZ")) {
		//create a depth buffer
		nvrhi::TextureDesc depthBufferDesc;
		depthBufferDesc.width = PrimaryWindowRef->GetBufferWidth();
		depthBufferDesc.height = PrimaryWindowRef->GetBufferHeight();
		depthBufferDesc.initialState = nvrhi::ResourceStates::DepthWrite;
		depthBufferDesc.isRenderTarget = true;
		depthBufferDesc.sampleCount = 1;
		depthBufferDesc.dimension = nvrhi::TextureDimension::Texture2D;
		depthBufferDesc.keepInitialState = true;
		depthBufferDesc.mipLevels = 1;
		depthBufferDesc.format = nvrhi::Format::D32;
		depthBufferDesc.isTypeless = true;
		depthBufferDesc.debugName = "SceneDepthZ";
		DepthBuffer = DeviceRef->m_NvrhiDevice->createTexture(depthBufferDesc);
		AssetManager::GetInstance()->RenderTargetTextures["SceneDepthZ"] = DepthBuffer;
	}
	else
		DepthBuffer = AssetManager::GetInstance()->RenderTargetTextures.at("SceneDepthZ");

	//create the gbuffer stuff
	nvrhi::TextureDesc texDesc;
	texDesc.width = PrimaryWindowRef->GetBufferWidth();
	texDesc.height = PrimaryWindowRef->GetBufferHeight();
	texDesc.initialState = nvrhi::ResourceStates::RenderTarget;
	texDesc.clearValue = 0.f;
	texDesc.useClearValue = true;
	texDesc.isRenderTarget = true;
	texDesc.sampleCount = 1;
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.keepInitialState = true;
	texDesc.mipLevels = 1;
	texDesc.isTypeless = false;
	//gbuffer albedo
	std::string name = "GBufferAlbedo";
	if (!AssetManager::GetInstance()->RenderTargetTextures.contains(name)) {
		texDesc.format = nvrhi::Format::RGBA8_UNORM;
		texDesc.debugName = name;
		AlbedoHandle = DeviceRef->m_NvrhiDevice->createTexture(texDesc);
		AssetManager::GetInstance()->RenderTargetTextures[name] = AlbedoHandle;
	}
	else
		AlbedoHandle = AssetManager::GetInstance()->RenderTargetTextures.at(name);
	name = "GBufferNormal";
	if (!AssetManager::GetInstance()->RenderTargetTextures.contains(name)) {
		texDesc.format = nvrhi::Format::RG16_UNORM;
		texDesc.debugName = name;
		NormalHandle = DeviceRef->m_NvrhiDevice->createTexture(texDesc);
		AssetManager::GetInstance()->RenderTargetTextures[name] = NormalHandle;
	}
	else
		NormalHandle = AssetManager::GetInstance()->RenderTargetTextures.at(name);
	name = "GBufferRM";
	if (!AssetManager::GetInstance()->RenderTargetTextures.contains(name)) {
		texDesc.format = nvrhi::Format::RG8_UNORM;
		texDesc.debugName = name;
		RoughnessMetallicHandle = DeviceRef->m_NvrhiDevice->createTexture(texDesc);
		AssetManager::GetInstance()->RenderTargetTextures[name] = RoughnessMetallicHandle;
	}
	else
		RoughnessMetallicHandle = AssetManager::GetInstance()->RenderTargetTextures.at(name);

	//create the fb for the graphics pipeline to draw on
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(AlbedoHandle)
		.addColorAttachment(NormalHandle)
		.addColorAttachment(RoughnessMetallicHandle)
		.setDepthAttachment(DepthBuffer);
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
	DeferredLightPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);;
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(DeviceRef->m_NvrhiDevice, CommandList);
}
