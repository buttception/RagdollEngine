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

void ForwardRenderer::Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em)
{
	DeviceRef = device;
	PrimaryWindowRef = win;
	FileManagerRef = fm;
	EntityManagerRef = em;
	CreateResource();
}

void ForwardRenderer::Shutdown()
{
	//release nvrhi stuff
	BindingLayoutHandle = nullptr;
	BindingSetHandle = nullptr;
	GraphicsPipeline = nullptr;
	WireframePipeline = nullptr;
	CommandList = nullptr;
	ForwardVertexShader = nullptr;
	ForwardPixelShader = nullptr;
	ConstantBufferHandle = nullptr;
	DepthBuffer = nullptr;
	InputLayoutHandle = nullptr;
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
		CommandList->clearDepthStencilTexture(DepthBuffer, subSet, true, 0.f, false, 0);
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
		.setDepthAttachment(DepthBuffer);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);
	ForwardPass->SetRenderTarget(fb);
	DebugPass->SetRenderTarget(fb);

	CommandList->open();
	ForwardPass->DrawAllInstances(scene->StaticInstanceBufferHandle, scene->StaticInstanceGroupInfos, scene->SceneInfo);
	DebugPass->DrawBoundingBoxes(scene->StaticInstanceDebugBufferHandle, scene->StaticDebugInstanceDatas.size(), scene->SceneInfo);
	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	
}

void ForwardRenderer::CreateResource()
{
	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();
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
	depthBufferDesc.debugName = "Depth";
	DepthBuffer = DeviceRef->m_NvrhiDevice->createTexture(depthBufferDesc);

	//create the fb for the graphics pipeline to draw on
	nvrhi::FramebufferHandle fb;
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(DeviceRef->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);
	fb = DeviceRef->m_NvrhiDevice->createFramebuffer(fbDesc);

	ForwardPass = std::make_shared<class ForwardPass>();
	ForwardPass->SetRenderTarget(fb);
	ForwardPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	DebugPass = std::make_shared<class DebugPass>();
	DebugPass->SetRenderTarget(fb);
	DebugPass->Init(DeviceRef->m_NvrhiDevice, CommandList);

	const static int32_t seed = 42;
	std::srand(seed);
}
