#include "ragdollpch.h"
#include <nvrhi/d3d12.h>
#include <dxgi1_5.h>
#include <dxgidebug.h>
#include <Windows.h>
#include <DXGI.h>
#include <dxgidebug.h>
#include <nvrhi/d3d12.h>
#include <nvrhi/validation.h>
#include <d3dcompiler.h>
#include <wrl.h>
using nvrhi::RefCountPtr; 

#include "Ragdoll/Core/Core.h"
#include "Ragdoll/Core/Logger.h"
#include "Ragdoll/Graphics/GLFWContext.h"
#include "Ragdoll/Graphics/Window/Window.h"
#include "Ragdoll/File/FileManager.h"

struct InstanceParameters
{
	bool enableDebugRuntime = false;
	bool headlessDevice = false;
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

class DirectXTest {
public:
	DeviceCreationParameters					m_DeviceParams;
	RefCountPtr<IDXGIFactory2>					m_DxgiFactory2;
	RefCountPtr<IDXGIAdapter>					m_DxgiAdapter;
	RefCountPtr<ID3D12CommandQueue>				m_GraphicsQueue;
	RefCountPtr<ID3D12Device>					m_Device12;
	RefCountPtr<ID3D12CommandQueue>				m_ComputeQueue;
	RefCountPtr<ID3D12CommandQueue>				m_CopyQueue;
	nvrhi::DeviceHandle							m_NvrhiDevice;
	HWND										m_hWnd = nullptr;
	DXGI_SWAP_CHAIN_DESC1						m_SwapChainDesc{};
	RefCountPtr<IDXGISwapChain3>				m_SwapChain;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC				m_FullScreenDesc{};
	bool										m_TearingSupported = false;
	RefCountPtr<ID3D12Fence>					m_FrameFence;
	std::vector<HANDLE>							m_FrameFenceEvents;
	std::vector<RefCountPtr<ID3D12Resource>>	m_SwapChainBuffers;
	std::vector<nvrhi::TextureHandle>			m_RhiSwapChainBuffers;
	UINT64										m_FrameCount = 0;
	nvrhi::GraphicsPipelineHandle				m_GraphicsPipeline;
	nvrhi::CommandListHandle					m_CommandList;

	std::shared_ptr<ragdoll::Window> m_PrimaryWindow;
	std::shared_ptr<ragdoll::FileManager> m_FileManager;

	nvrhi::ShaderHandle ImguiVertexShader;
	nvrhi::ShaderHandle ImguiPixelShader;

	void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr <ragdoll::FileManager> fm);
	void Draw();
	void Present();
	void Shutdown();
private:
	bool CreateDevice();
	bool CreateSwapChain();
	bool CreateRenderTargets();
	void ResizeSwapChain();
	void ReleaseRenderTargets();
	bool BeginFrame();
	void CreateResource();
	void DestroyDeviceAndSwapChain();
};

static bool MoveWindowOntoAdapter(IDXGIAdapter* targetAdapter, RECT& rect)
{
	assert(targetAdapter != NULL);

	HRESULT hres = S_OK;
	unsigned int outputNo = 0;
	while (SUCCEEDED(hres))
	{
		nvrhi::RefCountPtr<IDXGIOutput> pOutput;
		hres = targetAdapter->EnumOutputs(outputNo++, &pOutput);

		if (SUCCEEDED(hres) && pOutput)
		{
			DXGI_OUTPUT_DESC OutputDesc;
			pOutput->GetDesc(&OutputDesc);
			const RECT desktop = OutputDesc.DesktopCoordinates;
			const int centreX = (int)desktop.left + (int)(desktop.right - desktop.left) / 2;
			const int centreY = (int)desktop.top + (int)(desktop.bottom - desktop.top) / 2;
			const int winW = rect.right - rect.left;
			const int winH = rect.bottom - rect.top;
			const int left = centreX - winW / 2;
			const int right = left + winW;
			const int top = centreY - winH / 2;
			const int bottom = top + winH;
			rect.left = std::max(left, (int)desktop.left);
			rect.right = std::min(right, (int)desktop.right);
			rect.bottom = std::min(bottom, (int)desktop.bottom);
			rect.top = std::max(top, (int)desktop.top);

			// If there is more than one output, go with the first found.  Multi-monitor support could go here.
			return true;
		}
	}

	return false;
}
static std::string GetAdapterName(DXGI_ADAPTER_DESC const& aDesc)
{
	size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

	std::string name;
	name.resize(length);
	WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

	return name;
}
static bool IsNvDeviceID(UINT id)
{
	return id == 0x10DE;
}

void DirectXTest::Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm)
{
	m_PrimaryWindow = win;
	m_FileManager = fm;
	CreateDevice();
	CreateSwapChain();
	CreateResource();
}

void DirectXTest::Shutdown()
{
	DestroyDeviceAndSwapChain();

	m_DxgiAdapter = nullptr;
	m_DxgiFactory2 = nullptr;
}

bool DirectXTest::CreateDevice()
{
	m_DeviceParams.enableDebugRuntime = true;
	m_DeviceParams.enableNvrhiValidationLayer = true;
	m_DeviceParams.swapChainBufferCount = 2;
	m_DeviceParams.backBufferWidth = m_PrimaryWindow->GetBufferWidth();
	m_DeviceParams.backBufferHeight = m_PrimaryWindow->GetBufferHeight();
#define HR_RETURN(hr) if(FAILED(hr)) return false;
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

		std::string name = GetAdapterName(aDesc);
		bool isNvidia = IsNvDeviceID(aDesc.VendorId);
		RD_CORE_INFO("Adapter name:{}", name);
		RD_CORE_INFO("Is nvidia:{}", isNvidia);
	}

	HRESULT hr = D3D12CreateDevice(
		m_DxgiAdapter,
		m_DeviceParams.featureLevel,
		IID_PPV_ARGS(&m_Device12));
	HR_RETURN(hr)

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

bool DirectXTest::CreateSwapChain()
{
	UINT windowStyle = m_DeviceParams.startFullscreen
		? (WS_POPUP | WS_SYSMENU | WS_VISIBLE)
		: m_DeviceParams.startMaximized
		? (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE)
		: (WS_OVERLAPPEDWINDOW | WS_VISIBLE);

	RECT rect = { 0, 0, LONG(m_DeviceParams.backBufferWidth), LONG(m_DeviceParams.backBufferHeight) };
	AdjustWindowRect(&rect, windowStyle, FALSE);

	if (MoveWindowOntoAdapter(m_DxgiAdapter, rect))
	{
		glfwSetWindowPos(m_PrimaryWindow->GetGlfwWindow(), rect.left, rect.top);
	}
	m_hWnd = glfwGetWin32Window(m_PrimaryWindow->GetGlfwWindow());

	HRESULT hr = E_FAIL;

	RECT clientRect;
	GetClientRect(m_hWnd, &clientRect);
	UINT width = clientRect.right - clientRect.left;
	UINT height = clientRect.bottom - clientRect.top;

	ZeroMemory(&m_SwapChainDesc, sizeof(m_SwapChainDesc));
	m_SwapChainDesc.Width = width;
	m_SwapChainDesc.Height = height;
	m_SwapChainDesc.SampleDesc.Count = m_DeviceParams.swapChainSampleCount;
	m_SwapChainDesc.SampleDesc.Quality = 0;
	m_SwapChainDesc.BufferUsage = m_DeviceParams.swapChainUsage;
	m_SwapChainDesc.BufferCount = m_DeviceParams.swapChainBufferCount;
	m_SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	m_SwapChainDesc.Flags = m_DeviceParams.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

	// Special processing for sRGB swap chain formats.
	// DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
	// So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
	switch (m_DeviceParams.swapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
	{
	case nvrhi::Format::SRGBA8_UNORM:
		m_SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case nvrhi::Format::SBGRA8_UNORM:
		m_SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	default:
		m_SwapChainDesc.Format = nvrhi::d3d12::convertFormat(m_DeviceParams.swapChainFormat);
		break;
	}

	RefCountPtr<IDXGIFactory5> pDxgiFactory5;
	if (SUCCEEDED(m_DxgiFactory2->QueryInterface(IID_PPV_ARGS(&pDxgiFactory5))))
	{
		BOOL supported = 0;
		if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
			m_TearingSupported = (supported != 0);
	}

	if (m_TearingSupported)
	{
		m_SwapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

	m_FullScreenDesc = {};
	m_FullScreenDesc.RefreshRate.Numerator = m_DeviceParams.refreshRate;
	m_FullScreenDesc.RefreshRate.Denominator = 1;
	m_FullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	m_FullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	m_FullScreenDesc.Windowed = !m_DeviceParams.startFullscreen;

	RefCountPtr<IDXGISwapChain1> pSwapChain1;
	hr = m_DxgiFactory2->CreateSwapChainForHwnd(m_GraphicsQueue, m_hWnd, &m_SwapChainDesc, &m_FullScreenDesc, nullptr, &pSwapChain1);
	HR_RETURN(hr)

		hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
	HR_RETURN(hr)

		if (!CreateRenderTargets())
			return false;

	hr = m_Device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FrameFence));
	HR_RETURN(hr)

		for (UINT bufferIndex = 0; bufferIndex < m_SwapChainDesc.BufferCount; bufferIndex++)
		{
			m_FrameFenceEvents.push_back(CreateEvent(nullptr, false, true, nullptr));
		}

	return true;
}

bool DirectXTest::CreateRenderTargets()
{
	m_SwapChainBuffers.resize(m_SwapChainDesc.BufferCount);
	m_RhiSwapChainBuffers.resize(m_SwapChainDesc.BufferCount);

	for (UINT n = 0; n < m_SwapChainDesc.BufferCount; n++)
	{
		const HRESULT hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_SwapChainBuffers[n]));
		HR_RETURN(hr)

			nvrhi::TextureDesc textureDesc;
		textureDesc.width = m_DeviceParams.backBufferWidth;
		textureDesc.height = m_DeviceParams.backBufferHeight;
		textureDesc.sampleCount = m_DeviceParams.swapChainSampleCount;
		textureDesc.sampleQuality = m_DeviceParams.swapChainSampleQuality;
		textureDesc.format = m_DeviceParams.swapChainFormat;
		textureDesc.debugName = "SwapChainBuffer";
		textureDesc.isRenderTarget = true;
		textureDesc.isUAV = false;
		textureDesc.initialState = nvrhi::ResourceStates::Present;
		textureDesc.keepInitialState = true;

		m_RhiSwapChainBuffers[n] = m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(m_SwapChainBuffers[n]), textureDesc);
	}

	return true;
}

void DirectXTest::ReleaseRenderTargets()
{
	// Make sure that all frames have finished rendering
	m_NvrhiDevice->waitForIdle();

	// Release all in-flight references to the render targets
	m_NvrhiDevice->runGarbageCollection();

	// Set the events so that WaitForSingleObject in OneFrame will not hang later
	for (auto e : m_FrameFenceEvents)
		SetEvent(e);

	// Release the old buffers because ResizeBuffers requires that
	m_RhiSwapChainBuffers.clear();
	m_SwapChainBuffers.clear();
}

void DirectXTest::ResizeSwapChain()
{
	ReleaseRenderTargets();

	if (!m_NvrhiDevice)
		return;

	if (!m_SwapChain)
		return;

	const HRESULT hr = m_SwapChain->ResizeBuffers(m_DeviceParams.swapChainBufferCount,
		m_DeviceParams.backBufferWidth,
		m_DeviceParams.backBufferHeight,
		m_SwapChainDesc.Format,
		m_SwapChainDesc.Flags);

	if (FAILED(hr))
	{
		RD_CORE_FATAL("Resize failed");
	}

	bool ret = CreateRenderTargets();
	if (!ret)
	{
		RD_CORE_FATAL("CreateRenderTarget failed");
	}
}

bool DirectXTest::BeginFrame()
{
	DXGI_SWAP_CHAIN_DESC1 newSwapChainDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC newFullScreenDesc;
	if (SUCCEEDED(m_SwapChain->GetDesc1(&newSwapChainDesc)) && SUCCEEDED(m_SwapChain->GetFullscreenDesc(&newFullScreenDesc)))
	{
		if (m_FullScreenDesc.Windowed != newFullScreenDesc.Windowed)
		{
			//BackBufferResizing();

			m_FullScreenDesc = newFullScreenDesc;
			m_SwapChainDesc = newSwapChainDesc;
			m_DeviceParams.backBufferWidth = newSwapChainDesc.Width;
			m_DeviceParams.backBufferHeight = newSwapChainDesc.Height;

			if (newFullScreenDesc.Windowed)
				glfwSetWindowMonitor(m_PrimaryWindow->GetGlfwWindow(), nullptr, 50, 50, newSwapChainDesc.Width, newSwapChainDesc.Height, 0);

			ResizeSwapChain();
			//BackBufferResized();
		}

	}

	auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

	WaitForSingleObject(m_FrameFenceEvents[bufferIndex], INFINITE);

	return true;
}

void DirectXTest::Draw()
{
	nvrhi::TextureSubresourceSet subSet;
	m_CommandList->open();
	auto& tex = m_RhiSwapChainBuffers[m_SwapChain->GetCurrentBackBufferIndex()];

	auto bgCol = m_PrimaryWindow->GetBackgroundColor();
	nvrhi::Color col = nvrhi::Color( bgCol.x, bgCol.y, bgCol.z, bgCol.w );
	m_CommandList->clearTextureFloat(tex, subSet, col);
	//draw the triangle
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex);
	nvrhi::FramebufferHandle pipelineFb = m_NvrhiDevice->createFramebuffer(fbDesc);
	nvrhi::GraphicsState state;
	state.pipeline = m_GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	m_CommandList->setGraphicsState(state);
	nvrhi::DrawArguments args;
	args.vertexCount = 3;
	m_CommandList->draw(args);
	m_CommandList->close();
	m_NvrhiDevice->executeCommandList(m_CommandList);
}

void DirectXTest::Present()
{
	//if (!m_windowVisible)
		//return;

	auto bufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

	UINT presentFlags = 0;
	if (!m_DeviceParams.vsyncEnabled && m_FullScreenDesc.Windowed && m_TearingSupported)
		presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

	m_SwapChain->Present(m_DeviceParams.vsyncEnabled ? 1 : 0, presentFlags);

	m_FrameFence->SetEventOnCompletion(m_FrameCount, m_FrameFenceEvents[bufferIndex]);
	m_GraphicsQueue->Signal(m_FrameFence, m_FrameCount);
	m_FrameCount++;
}

void DirectXTest::CreateResource()
{
	m_CommandList = m_NvrhiDevice->createCommandList();
	nvrhi::ShaderHandle vs;
	nvrhi::ShaderHandle ps;
	nvrhi::InputLayoutHandle inputLayout;
	nvrhi::FramebufferHandle fb;
	nvrhi::BindingLayoutHandle bindingLayout;

	nvrhi::BufferHandle vb;
	nvrhi::CommandListHandle cmdList;
	//load outputted shaders objects
	m_FileManager->ImmediateLoad(ragdoll::FileIORequest(ragdoll::GuidGenerator::Generate(), "test.vs.cso", [&](ragdoll::Guid id, const uint8_t* data, uint32_t size) {
		vs = m_NvrhiDevice->createShader(
			nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex),
			data, size);
		}
	));
	m_FileManager->ImmediateLoad(ragdoll::FileIORequest(ragdoll::GuidGenerator::Generate(), "test.ps.cso", [&](ragdoll::Guid id, const uint8_t* data, uint32_t size) {
		ps = m_NvrhiDevice->createShader(
			nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel),
			(char*)data, size);
		}
	));
	m_FileManager->ImmediateLoad(ragdoll::FileIORequest(ragdoll::GuidGenerator::Generate(), "imgui.vs.cso", [&](ragdoll::Guid id, const uint8_t* data, uint32_t size) {
		ImguiVertexShader = m_NvrhiDevice->createShader(
			nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex),
			data, size);
		}
	));
	m_FileManager->ImmediateLoad(ragdoll::FileIORequest(ragdoll::GuidGenerator::Generate(), "imgui.ps.cso", [&](ragdoll::Guid id, const uint8_t* data, uint32_t size) {
		ImguiPixelShader = m_NvrhiDevice->createShader(
			nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel),
			data, size);
		}
	));
	//create a depth buffer
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(m_RhiSwapChainBuffers[0]);
	fb = m_NvrhiDevice->createFramebuffer(fbDesc);
	//can set the requirements
	auto layoutDesc = nvrhi::BindingLayoutDesc()
		.setVisibility(nvrhi::ShaderType::All);
	bindingLayout = m_NvrhiDevice->createBindingLayout(layoutDesc);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc()
		.setVertexShader(vs)
		.setPixelShader(ps);
	//why does disabling depth crash
	pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

	m_GraphicsPipeline = m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, fb);
}

void DirectXTest::DestroyDeviceAndSwapChain()
{
	m_RhiSwapChainBuffers.clear();

	ReleaseRenderTargets();

	m_NvrhiDevice = nullptr;

	for (auto fenceEvent : m_FrameFenceEvents)
	{
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}

	m_FrameFenceEvents.clear();

	if (m_SwapChain)
	{
		m_SwapChain->SetFullscreenState(false, nullptr);
	}

	m_SwapChainBuffers.clear();

	m_FrameFence = nullptr;
	m_SwapChain = nullptr;
	m_GraphicsQueue = nullptr;
	m_ComputeQueue = nullptr;
	m_CopyQueue = nullptr;
	m_Device12 = nullptr;
}
