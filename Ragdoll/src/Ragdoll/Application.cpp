/*!
\file		Application.cpp
\date		05/08/2024

\author		Devin Tan
\email		devintrh@gmail.com

\copyright	MIT License

			Copyright © 2024 Tan Rui Hao Devin

			Permission is hereby granted, free of charge, to any person obtaining a copy
			of this software and associated documentation files (the "Software"), to deal
			in the Software without restriction, including without limitation the rights
			to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
			copies of the Software, and to permit persons to whom the Software is
			furnished to do so, subject to the following conditions:

			The above copyright notice and this permission notice shall be included in all
			copies or substantial portions of the Software.

			THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
			IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
			FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
			AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
			LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
			OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
			SOFTWARE.
__________________________________________________________________________________*/

#include "ragdollpch.h"

#include "Application.h"

#include "Core/Logger.h"
#include "Core/Core.h"
#include "Entity/EntityManager.h"
#include "Graphics/Window/Window.h"
#include "Graphics/GLFWContext.h"
#include "Graphics/OpenGLContext.h"
#include "Event/WindowEvents.h"
#include "Event/KeyEvents.h"
#include "Event/MouseEvent.h"
#include "glad/glad.h"
#include "Input/InputHandler.h"
#include "Graphics/Renderer/RenderGraph.h"
#include "Graphics/Transform/Transform.h"
#include "Layer/LayerStack.h"
#include "Graphics/Transform/TransformLayer.h"
#include "Resource/ResourceManager.h"
#include "File/FileManager.h"

#include "Ragdoll/Graphics/Renderer/RenderData.h"
ragdoll::Transform* t1, *t2, *t3, *t4, *t5;
#include "nvrhi/d3d12.h"
#include <dxgi1_5.h>
#include <dxgidebug.h>
#include <Windows.h>
#include <DXGI.h>
#include <nvrhi/d3d12.h>
#include <nvrhi/validation.h>
using nvrhi::RefCountPtr;

namespace ragdoll
{
	void Application::Init(const ApplicationConfig& config)
	{
		UNREFERENCED_PARAMETER(config);
		Logger::Init();
		RD_CORE_INFO("spdlog initialized for use.");

		GLFWContext::Init();

		m_PrimaryWindow = std::make_shared<Window>();
		m_PrimaryWindow->Init();
		//bind the application callback to the window
		m_PrimaryWindow->SetEventCallback(RD_BIND_EVENT_FN(Application::OnEvent));
		//setup opengl context
		m_Context = std::make_shared<OpenGLContext>();
		m_Context->Init(m_PrimaryWindow);
		//setup input handler
		m_InputHandler = std::make_shared<InputHandler>();
		m_InputHandler->Init();
		//create the entity manager
		m_EntityManager = std::make_shared<EntityManager>();
		//create the resource manager
		m_ResourceManager = std::make_shared<ResourceManager>();
		m_ResourceManager->Init(m_PrimaryWindow);
		//create the render graph
		m_RenderGraph = std::make_shared<RenderGraph>();
		m_RenderGraph->Init(m_PrimaryWindow, m_ResourceManager, m_EntityManager);
		//create the file manager
		m_FileManager = std::make_shared<FileManager>();
		m_FileManager->Init();

		//layers stuff
		m_LayerStack = std::make_shared<LayerStack>();
		//adding the transform layer
		m_TransformLayer = std::make_shared<TransformLayer>(m_EntityManager);
		m_LayerStack->PushLayer(m_TransformLayer);

		//testing d3d12
		CreateDevice();

		//init all layers
		m_LayerStack->Init();

		//test the transform layer
		auto e1 = m_EntityManager->CreateEntity();
		auto e2 = m_EntityManager->CreateEntity();
		auto e3 = m_EntityManager->CreateEntity();
		auto e4 = m_EntityManager->CreateEntity();
		auto e5 = m_EntityManager->CreateEntity();
		t1 = m_EntityManager->AddComponent<Transform>(e1);
		t1->m_LocalPosition = { 0.3f, 0.f, 0.f };
		m_TransformLayer->SetEntityAsRoot(m_EntityManager->GetGuid(e1));
		t2 = m_EntityManager->AddComponent<Transform>(e2);
		t2->m_LocalPosition = { 0.3f, 0.f, 0.f };
		t2->m_Parent = m_EntityManager->GetGuid(e1);
		t1->m_Child = m_EntityManager->GetGuid(e2);
		t3 = m_EntityManager->AddComponent<Transform>(e3);
		t3->m_LocalPosition = { 0.3f, 0.f, 0.f };
		t3->m_Parent = m_EntityManager->GetGuid(e2);
		t2->m_Child = m_EntityManager->GetGuid(e3);
		t4 = m_EntityManager->AddComponent<Transform>(e4);
		t4->m_LocalPosition = { 0.f, 0.3f, 0.f };
		t4->m_Parent = m_EntityManager->GetGuid(e1);
		t2->m_Sibling = m_EntityManager->GetGuid(e4);
		t5 = m_EntityManager->AddComponent<Transform>(e5);
		t5->m_LocalPosition = { 0.f, 0.3f, 0.f };
		t5->m_Parent = m_EntityManager->GetGuid(e4);
		t4->m_Child = m_EntityManager->GetGuid(e5);

		//add all of this into the render graph data
		std::vector<RenderData> renderData;
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e1), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e2), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e3), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e4), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e5), 0 }});
		m_ResourceManager->AddRenderData("test", std::move(renderData));
	}

	void Application::Run()
	{
		while(m_Running)
		{
			//input update must be called before window polls for inputs
			m_InputHandler->Update(m_PrimaryWindow->GetDeltaTime());
			m_FileManager->Update();
			m_PrimaryWindow->StartRender();

			for(auto& layer : *m_LayerStack)
			{
				layer->Update(static_cast<float>(m_PrimaryWindow->GetDeltaTime()));
			}

			m_RenderGraph->Execute();

			m_PrimaryWindow->EndRender();
		}
	}

	void Application::Shutdown()
	{
		m_FileManager->Shutdown();
		m_PrimaryWindow->Shutdown();
		GLFWContext::Shutdown();
		RD_CORE_INFO("ragdoll Engine application shut down successfull");
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		RD_DISPATCH_EVENT(dispatcher, WindowCloseEvent, event, Application::OnWindowClose);
		RD_DISPATCH_EVENT(dispatcher, WindowResizeEvent, event, Application::OnWindowResize);
		RD_DISPATCH_EVENT(dispatcher, WindowMoveEvent, event, Application::OnWindowMove);
		RD_DISPATCH_EVENT(dispatcher, KeyPressedEvent, event, Application::OnKeyPressed);
		RD_DISPATCH_EVENT(dispatcher, KeyReleasedEvent, event, Application::OnKeyReleased);
		RD_DISPATCH_EVENT(dispatcher, KeyTypedEvent, event, Application::OnKeyTyped);
		RD_DISPATCH_EVENT(dispatcher, MouseMovedEvent, event, Application::OnMouseMove);
		RD_DISPATCH_EVENT(dispatcher, MouseButtonPressedEvent, event, Application::OnMouseButtonPressed);
		RD_DISPATCH_EVENT(dispatcher, MouseButtonReleasedEvent, event, Application::OnMouseButtonReleased);
		RD_DISPATCH_EVENT(dispatcher, MouseScrolledEvent, event, Application::OnMouseScrolled);

		//run the event on all layers
		for(auto& layer : *m_LayerStack)
		{
			layer->OnEvent(event);
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		return false;
	}

	bool Application::OnWindowMove(WindowMoveEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		return false;
	}

	bool Application::OnKeyPressed(KeyPressedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnKeyPressed(event);
		return false;
	}

	bool Application::OnKeyReleased(KeyReleasedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnKeyReleased(event);
		return false;
	}

	bool Application::OnKeyTyped(KeyTypedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnKeyTyped(event);
		return false;
	}

	bool Application::OnMouseMove(MouseMovedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseMove(event);
		return false;
	}

	bool Application::OnMouseButtonPressed(MouseButtonPressedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseButtonPressed(event);
		return false;
	}

	bool Application::OnMouseButtonReleased(MouseButtonReleasedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseButtonReleased(event);
		return false;
	}

	bool Application::OnMouseScrolled(MouseScrolledEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseScrolled(event);
		return false;
	}

	//=====================================================================================================
	struct InstanceParameters
	{
		bool enableDebugRuntime = false;
		bool headlessDevice = false;

#if DONUT_WITH_VULKAN
		std::vector<std::string> requiredVulkanInstanceExtensions;
		std::vector<std::string> requiredVulkanLayers;
		std::vector<std::string> optionalVulkanInstanceExtensions;
		std::vector<std::string> optionalVulkanLayers;
#endif
	};
	struct DeviceCreationParameters : public InstanceParameters
	{
		bool startMaximized = false;
		bool startFullscreen = false;
		bool allowModeSwitch = true;
		int windowPosX = -1;            // -1 means use default placement
		int windowPosY = -1;
		uint32_t backBufferWidth = 1280;
		uint32_t backBufferHeight = 720;
		uint32_t refreshRate = 0;
		uint32_t swapChainBufferCount = 3;
		nvrhi::Format swapChainFormat = nvrhi::Format::SRGBA8_UNORM;
		uint32_t swapChainSampleCount = 1;
		uint32_t swapChainSampleQuality = 0;
		uint32_t maxFramesInFlight = 2;
		bool enableNvrhiValidationLayer = false;
		bool vsyncEnabled = false;
		bool enableRayTracingExtensions = false; // for vulkan
		bool enableComputeQueue = false;
		bool enableCopyQueue = false;

		// Severity of the information log messages from the device manager, like the device name or enabled extensions.
		//log::Severity infoLogSeverity = log::Severity::Info;

		// Index of the adapter (DX11, DX12) or physical device (Vk) on which to initialize the device.
		// Negative values mean automatic detection.
		// The order of indices matches that returned by DeviceManager::EnumerateAdapters.
		int adapterIndex = -1;

		// set to true to enable DPI scale factors to be computed per monitor
		// this will keep the on-screen window size in pixels constant
		//
		// if set to false, the DPI scale factors will be constant but the system
		// may scale the contents of the window based on DPI
		//
		// note that the backbuffer size is never updated automatically; if the app
		// wishes to scale up rendering based on DPI, then it must set this to true
		// and respond to DPI scale factor changes by resizing the backbuffer explicitly
		bool enablePerMonitorDPI = false;

		DXGI_USAGE swapChainUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
	};

	class IRenderPass;

	struct AdapterInfo
	{
		std::string name;
		uint32_t vendorID = 0;
		uint32_t deviceID = 0;
		uint64_t dedicatedVideoMemory = 0;
		nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
	};
	struct DefaultMessageCallback : public nvrhi::IMessageCallback
	{
		static DefaultMessageCallback& GetInstance() { return s_Instance; }

		void message(nvrhi::MessageSeverity severity, const char* messageText) override {
			switch (severity) {
			case nvrhi::MessageSeverity::Info:
				RD_CORE_INFO(messageText);
				break;
			case nvrhi::MessageSeverity::Warning:
				RD_CORE_WARN(messageText);
				break;
			case nvrhi::MessageSeverity::Error:
				RD_CORE_ERROR(messageText);
				break;
			case nvrhi::MessageSeverity::Fatal:
				RD_CORE_FATAL(messageText);
				break;
			}
		}

		static DefaultMessageCallback s_Instance;
	};
	DefaultMessageCallback DefaultMessageCallback::s_Instance;
	
	bool Application::CreateDevice()
	{
#define HR_RETURN(hr) if(FAILED(hr)) return false;
		DeviceCreationParameters m_DeviceParams;
		if (m_DeviceParams.enableDebugRuntime)
		{
			RefCountPtr<ID3D12Debug> pDebug;
			HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));
			pDebug->EnableDebugLayer();
		}
		int adapterIndex = m_DeviceParams.adapterIndex;
		if (adapterIndex < 0)
			adapterIndex = 0;
		//create factory
		RefCountPtr<IDXGIFactory2> m_DxgiFactory2;
		RefCountPtr<IDXGIAdapter> m_DxgiAdapter;
		RefCountPtr<ID3D12CommandQueue> m_GraphicsQueue;
		RefCountPtr<ID3D12Device> m_Device12;
		RefCountPtr<ID3D12CommandQueue> m_ComputeQueue;
		RefCountPtr<ID3D12CommandQueue> m_CopyQueue;
		nvrhi::DeviceHandle m_NvrhiDevice;
		HRESULT res = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_DxgiFactory2));
		if (res != S_OK)
			__debugbreak();

		if (m_DxgiFactory2->EnumAdapters(adapterIndex, &m_DxgiAdapter))
		{
			if (adapterIndex == 0)
				RD_CORE_ERROR("Cannot find any DXGI adapters in the system.");
			else
				RD_CORE_ERROR("The specified DXGI adapter %d does not exist.", adapterIndex);
			__debugbreak();
		}

		{
			DXGI_ADAPTER_DESC aDesc;
			m_DxgiAdapter->GetDesc(&aDesc);
		}

		HRESULT hr = D3D12CreateDevice(
			m_DxgiAdapter,
			m_DeviceParams.featureLevel,
			IID_PPV_ARGS(&m_Device12));

			if (m_DeviceParams.enableDebugRuntime)
			{
				RefCountPtr<ID3D12InfoQueue> pInfoQueue;
				m_Device12->QueryInterface(&pInfoQueue);

				if (pInfoQueue)
				{
#ifdef _DEBUG
					pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
					pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif

					D3D12_MESSAGE_ID disableMessageIDs[] = {
						D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
						D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH, // descriptor validation doesn't understand acceleration structures
					};

					D3D12_INFO_QUEUE_FILTER filter = {};
					filter.DenyList.pIDList = disableMessageIDs;
					filter.DenyList.NumIDs = sizeof(disableMessageIDs) / sizeof(disableMessageIDs[0]);
					pInfoQueue->AddStorageFilterEntries(&filter);
				}
			}

		D3D12_COMMAND_QUEUE_DESC queueDesc;
		ZeroMemory(&queueDesc, sizeof(queueDesc));
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.NodeMask = 1;
		hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue));
		HR_RETURN(hr)
			m_GraphicsQueue->SetName(L"Graphics Queue");

		if (m_DeviceParams.enableComputeQueue)
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
			hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_ComputeQueue));
			HR_RETURN(hr)
				m_ComputeQueue->SetName(L"Compute Queue");
		}

		if (m_DeviceParams.enableCopyQueue)
		{
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			hr = m_Device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CopyQueue));
			HR_RETURN(hr)
				m_CopyQueue->SetName(L"Copy Queue");
		}

		nvrhi::d3d12::DeviceDesc deviceDesc;
		deviceDesc.errorCB = &DefaultMessageCallback::GetInstance();
		deviceDesc.pDevice = m_Device12;
		deviceDesc.pGraphicsCommandQueue = m_GraphicsQueue;
		deviceDesc.pComputeCommandQueue = m_ComputeQueue;
		deviceDesc.pCopyCommandQueue = m_CopyQueue;

		m_NvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);

		if (m_DeviceParams.enableNvrhiValidationLayer)
		{
			m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
		}

		return true;
	}
}
