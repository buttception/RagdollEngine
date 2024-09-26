#include "ragdollpch.h"

#include <nvrhi/utils.h>
#include <microprofile.h>
#include <nvrhi/d3d12-backend.h>
#include "ForwardRenderer.h"
#include "DirectXDevice.h"

#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"
#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Scene.h"
#include "GeometryBuilder.h"

void ForwardRenderer::Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene)
{
	DeviceRef = device;
	PrimaryWindowRef = win;
	DepthHandle = scene->SceneDepthZ;
	for (int i = 0; i < 4; ++i)
	{
		ShadowMap[i] = scene->ShadowMap[i];
	}
	CreateResource();
}

void ForwardRenderer::Shutdown()
{
	//release nvrhi stuff
	CommandList = nullptr;
	DepthHandle = nullptr;
}

void ForwardRenderer::BeginFrame()
{
	MICROPROFILE_SCOPEI("Render", "Begin Frame", MP_BLUE);
	DeviceRef->BeginFrame();
	DeviceRef->m_NvrhiDevice->runGarbageCollection();
	nvrhi::TextureHandle tex = DeviceRef->GetCurrentBackbuffer();
	{
		nvrhi::TextureSubresourceSet subSet;
		auto bgCol = PrimaryWindowRef->GetBackgroundColor();
		nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
		CommandList->open();
		CommandList->beginMarker("ClearBackBuffer");
		CommandList->clearTextureFloat(tex, subSet, col);
		CommandList->clearDepthStencilTexture(DepthHandle, subSet, true, 0.f, false, 0);
		for (int i = 0; i < 4; ++i)
		{
			CommandList->clearDepthStencilTexture(ShadowMap[i], subSet, true, 0.f, false, 0);
		}
		CommandList->endMarker();
		CommandList->close();
		DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	}
}

void ForwardRenderer::Render(ragdoll::Scene* scene)
{
	BeginFrame();

	nvrhi::FramebufferHandle fb;
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	ForwardPass->SetRenderTarget(fb);
	DebugPass->SetRenderTarget(fb);

	CommandList->open();
	ShadowPass->DrawAllInstances(scene->StaticCascadeInstanceBufferHandles, scene->StaticCascadeInstanceInfos, scene->SceneInfo);
	ForwardPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
	DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);
	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	
}

void ForwardRenderer::CreateResource()
{
	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();

	//create the fb for the graphics pipeline to draw on
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

	nvrhi::FramebufferHandle fb;
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthHandle);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	ForwardPass = std::make_shared<class ForwardPass>();
	ForwardPass->SetRenderTarget(fb);
	ForwardPass->SetDependencies(ShadowMap);
	ForwardPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(DeviceRef->m_NvrhiDevice, CommandList);
}
