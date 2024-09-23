#include "ragdollpch.h"
#include "DirectXDevice.h"
#include <nvrhi/utils.h>

//========================temp=======================

DefaultMessageCallback DefaultMessageCallback::s_Instance;
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

std::shared_ptr<DirectXDevice> DirectXDevice::Create(DeviceCreationParameters creationParam, std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm)
{
	std::shared_ptr<DirectXDevice> device = std::make_shared<DirectXDevice>();
	device->m_DeviceParams = creationParam;
	device->m_PrimaryWindow = win;
	device->m_FileManager = fm;
	device->CreateDevice();
	device->CreateSwapChain();
	return device;
}

void DirectXDevice::Shutdown()
{
	if (bIsShutdown)
		return;
	m_NvrhiDevice->waitForIdle();
	m_NvrhiDevice->runGarbageCollection();
	DestroyDeviceAndSwapChain();

	m_DxgiAdapter = nullptr;
	m_DxgiFactory2 = nullptr;
	bIsShutdown = true;
}

nvrhi::TextureHandle DirectXDevice::GetCurrentBackbuffer()
{
	return m_RhiSwapChainBuffers[m_SwapChain->GetCurrentBackBufferIndex()];
}

bool DirectXDevice::CreateDevice()
{
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

bool DirectXDevice::CreateSwapChain()
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

bool DirectXDevice::CreateRenderTargets()
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

void DirectXDevice::ReleaseRenderTargets()
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

void DirectXDevice::ResizeSwapChain()
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

bool DirectXDevice::BeginFrame()
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

void DirectXDevice::Present()
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

void DirectXDevice::DestroyDeviceAndSwapChain()
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
