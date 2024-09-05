#include "ragdollpch.h"
#include "DirectXTest.h"
#include <nvrhi/utils.h>

//========================temp=======================
struct Vertex
{
	Vector3 position;
	Vector3 color;
};
struct CBuffer {
	Matrix world;
	Matrix viewProj;
};

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

void DirectXTest::Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::InputHandler> hdl)
{
	m_PrimaryWindow = win;
	m_FileManager = fm;
	m_InputHandler = hdl;
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
	m_NvrhiDevice->runGarbageCollection();
	nvrhi::TextureSubresourceSet subSet;
	m_CommandList->open();
	auto& tex = m_RhiSwapChainBuffers[m_SwapChain->GetCurrentBackBufferIndex()];

	auto bgCol = m_PrimaryWindow->GetBackgroundColor();
	nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
	m_CommandList->clearTextureFloat(tex, subSet, col);
	m_CommandList->clearDepthStencilTexture(DepthBuffer, subSet, true, 1.f, false, 0);

	//manipulate the cube and camera
	struct Data {
		Vector3 cubePosition = { 0.f, 0.f, 0.f };
		Vector3 cubeScale = { 1.f, 1.f, 1.f };
		Vector3 cubeEulers = { 0.f, 0.f, 0.f };
		Vector3 cameraPos = { 0.f, 0.f, -5.f };
		Vector2 cameraEulers = { 0.f, 0.f };
		float cameraFov = 60.f;
		float cameraNear = 0.01f;
		float cameraFar = 100.f;
		float cameraAspect = 16.f / 9.f;
		float cameraSpeed = 1.f;
		float cameraRotationSpeed = 5.f;
	};
	static Data data;
	ImGui::Begin("Triangle Manipulate");
	ImGui::DragFloat3("Translation", &data.cubePosition.x, 0.01f);
	ImGui::DragFloat3("Scale", &data.cubeScale.x, 0.01f);
	ImGui::DragFloat3("Rotate (Radians)", &data.cubeEulers.x, 0.01f);
	ImGui::DragFloat("Camera FOV (Degrees)", &data.cameraFov, 1.f, 60.f, 120.f);
	ImGui::DragFloat("Camera Near", &data.cameraNear, 0.01f, 0.01f, 1.f);
	ImGui::DragFloat("Camera Far", &data.cameraFar, 10.f, 10.f, 10000.f);
	ImGui::DragFloat("Camera Aspect Ratio", &data.cameraAspect, 0.01f, 0.01f, 5.f);
	ImGui::DragFloat("Camera Speed", &data.cameraSpeed, 0.01f, 0.01f, 5.f);
	ImGui::DragFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 1.f, 10.f, 100.f);
	ImGui::End();

	CBuffer cbuf;
	cbuf.world = Matrix::CreateTranslation(data.cubePosition);
	cbuf.world *= Matrix::CreateScale(data.cubeScale);
	cbuf.world *= Matrix::CreateFromYawPitchRoll(data.cubeEulers.x, data.cubeEulers.y, data.cubeEulers.z);
	Matrix proj = Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(data.cameraFov), data.cameraAspect, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraEulers.x, data.cameraEulers.y, 0.f));
	Matrix view = Matrix::CreateLookAt(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	cbuf.viewProj = view * proj;
	
	//hardcoded handling of movement now
	if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
			data.cameraPos += cameraDir * data.cameraSpeed * m_PrimaryWindow->GetFrameTime();
		else if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
			data.cameraPos -= cameraDir * data.cameraSpeed * m_PrimaryWindow->GetFrameTime();
		Vector3 cameraRight = -cameraDir.Cross(Vector3(0.f, 1.f, 0.f));
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
			data.cameraPos += cameraRight * data.cameraSpeed * m_PrimaryWindow->GetFrameTime();
		else if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
			data.cameraPos -= cameraRight * data.cameraSpeed * m_PrimaryWindow->GetFrameTime();
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			auto& io = ImGui::GetIO();
			data.cameraEulers.x -= io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * m_PrimaryWindow->GetFrameTime();
			data.cameraEulers.y += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * m_PrimaryWindow->GetFrameTime();
		}
	}

	//draw the triangle
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex)
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = m_NvrhiDevice->createFramebuffer(fbDesc);
	nvrhi::GraphicsState state;
	state.pipeline = m_GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { IndexBuffer, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = { {VertexBuffer, 0, offsetof(Vertex, position)},
	{VertexBuffer, 1, offsetof(Vertex, color)} };

	auto layoutDesc = nvrhi::BindingSetDesc();
	layoutDesc.bindings = {
		nvrhi::BindingSetItem::PushConstants(0, sizeof(CBuffer)),
	};
	auto binding = m_NvrhiDevice->createBindingSet(layoutDesc, BindingLayout);
	state.bindings = { binding };
	assert(state.bindings[0]);

	m_CommandList->setGraphicsState(state);
	m_CommandList->setPushConstants(&cbuf, sizeof(CBuffer));
	nvrhi::DrawArguments args;
	args.vertexCount = 36;
	m_CommandList->drawIndexed(args);
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
	nvrhi::TextureDesc depthBufferDesc;
	depthBufferDesc.width = m_PrimaryWindow->GetBufferWidth();
	depthBufferDesc.height = m_PrimaryWindow->GetBufferHeight();
	depthBufferDesc.initialState = nvrhi::ResourceStates::DepthWrite;
	depthBufferDesc.isRenderTarget = true;
	depthBufferDesc.sampleCount = 1;
	depthBufferDesc.dimension = nvrhi::TextureDimension::Texture2D;
	depthBufferDesc.keepInitialState = true;
	depthBufferDesc.mipLevels = 1;
	depthBufferDesc.format = nvrhi::Format::D32;
	depthBufferDesc.isTypeless = true;
	depthBufferDesc.debugName = "Depth";
	DepthBuffer = m_NvrhiDevice->createTexture(depthBufferDesc);

	//create the fb for the graphics pipeline to draw on
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(m_RhiSwapChainBuffers[0])
		.setDepthAttachment(DepthBuffer);
	fb = m_NvrhiDevice->createFramebuffer(fbDesc);

	//can set the requirements
	auto layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		//bind slot 0 with 2 floats
		nvrhi::BindingLayoutItem::PushConstants(0, sizeof(CBuffer)),
	};
	BindingLayout = m_NvrhiDevice->createBindingLayout(layoutDesc);

	//create a cube first
	Vertex vertices[] = {
		// Position         // Normal        // Color
		// Front face
		{ {-1.0f, -1.0f,  1.0f},  {0.0f,  0.0f,  1.0f}},  // Vertex 0
		{ { 1.0f, -1.0f,  1.0f},  {0.0f,  0.0f,  1.0f}},  // Vertex 1
		{ { 1.0f,  1.0f,  1.0f},  {0.0f,  0.0f,  1.0f}},  // Vertex 2
		{ {-1.0f,  1.0f,  1.0f},  {0.0f,  0.0f,  1.0f}},  // Vertex 3

		// Back face
		{ {-1.0f, -1.0f, -1.0f},  {0.0f,  1.0f, 1.0f}},  // Vertex 4
		{ { 1.0f, -1.0f, -1.0f},  {0.0f,  1.0f, 1.0f}},  // Vertex 5
		{ { 1.0f,  1.0f, -1.0f},  {0.0f,  1.0f, 1.0f}},  // Vertex 6
		{ {-1.0f,  1.0f, -1.0f},  {0.0f,  1.0f, 1.0f}},  // Vertex 7

		// Left face
		{ {-1.0f, -1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}},  // Vertex 8
		{ {-1.0f, -1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}},  // Vertex 9
		{ {-1.0f,  1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}},  // Vertex 10
		{ {-1.0f,  1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}},  // Vertex 11

		// Right face
		{ { 1.0f, -1.0f, -1.0f},  {1.0f,  1.0f,  0.0f}},  // Vertex 12
		{ { 1.0f, -1.0f,  1.0f},  {1.0f,  1.0f,  0.0f}},  // Vertex 13
		{ { 1.0f,  1.0f,  1.0f},  {1.0f,  1.0f,  0.0f}},  // Vertex 14
		{ { 1.0f,  1.0f, -1.0f},  {1.0f,  1.0f,  0.0f}},  // Vertex 15

		// Top face
		{ {-1.0f,  1.0f, -1.0f},  {1.0f,  1.0f,  1.0f}},  // Vertex 16
		{ { 1.0f,  1.0f, -1.0f},  {1.0f,  1.0f,  1.0f}},  // Vertex 17
		{ { 1.0f,  1.0f,  1.0f},  {1.0f,  1.0f,  1.0f}},  // Vertex 18
		{ {-1.0f,  1.0f,  1.0f},  {1.0f,  1.0f,  1.0f}},  // Vertex 19

		// Bottom face
		{ {-1.0f, -1.0f, -1.0f},  {0.0f, 1.0f,  0.0f}},  // Vertex 20
		{ { 1.0f, -1.0f, -1.0f},  {0.0f, 1.0f,  0.0f}},  // Vertex 21
		{ { 1.0f, -1.0f,  1.0f},  {0.0f, 1.0f,  0.0f}},  // Vertex 22
		{ {-1.0f, -1.0f,  1.0f},  {0.0f, 1.0f,  0.0f}}   // Vertex 23
	};
	uint32_t indices[36] = {
		// Front face
		0, 1, 2,
		0, 2, 3,

		// Back face
		4, 5, 6,
		4, 6, 7,

		// Left face
		8, 9, 10,
		8, 10, 11,

		// Right face
		12, 13, 14,
		12, 14, 15,

		// Top face
		16, 17, 18,
		16, 18, 19,

		// Bottom face
		20, 21, 22,
		20, 22, 23
	};
	//push the data into the buffer
	nvrhi::VertexAttributeDesc positionAttrib;
	positionAttrib.name = "POSITION";
	positionAttrib.format = nvrhi::Format::RGB32_FLOAT;
	positionAttrib.offset = 0;
	positionAttrib.bufferIndex = 0;
	positionAttrib.elementStride = sizeof(Vertex);
	nvrhi::VertexAttributeDesc colorAttrib;
	colorAttrib.name = "ICOLOR";
	colorAttrib.format = nvrhi::Format::RGB32_FLOAT;
	colorAttrib.offset = 0;
	colorAttrib.bufferIndex = 1;
	colorAttrib.elementStride = sizeof(Vertex);
	nvrhi::VertexAttributeDesc attribs[] = {
		positionAttrib,
		colorAttrib
	};
	nvrhi::InputLayoutHandle inputLayoutHandle = m_NvrhiDevice->createInputLayout(attribs, uint32_t(std::size(attribs)), vs);
	//loading the data
	m_CommandList->open();
	nvrhi::BufferDesc vertexBufDesc;
	vertexBufDesc.byteSize = sizeof(vertices);
	vertexBufDesc.isVertexBuffer = true;
	vertexBufDesc.debugName = "VertexBuffer";
	vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
	//smth smth syncrhonization need to be this state to be written

	VertexBuffer = m_NvrhiDevice->createBuffer(vertexBufDesc);
	//copy data over
	m_CommandList->beginTrackingBufferState(VertexBuffer, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
	m_CommandList->writeBuffer(VertexBuffer, vertices, sizeof(vertices));
	m_CommandList->setPermanentBufferState(VertexBuffer, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

	nvrhi::BufferDesc indexBufDesc;
	indexBufDesc.byteSize = sizeof(indices);
	indexBufDesc.isIndexBuffer = true;
	indexBufDesc.debugName = "IndexBuffer";
	indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

	IndexBuffer = m_NvrhiDevice->createBuffer(indexBufDesc);
	m_CommandList->beginTrackingBufferState(IndexBuffer, nvrhi::ResourceStates::CopyDest);
	m_CommandList->writeBuffer(IndexBuffer, indices, sizeof(indices));
	m_CommandList->setPermanentBufferState(IndexBuffer, nvrhi::ResourceStates::IndexBuffer);

	m_CommandList->close();
	m_NvrhiDevice->executeCommandList(m_CommandList);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();
	pipelineDesc.addBindingLayout(BindingLayout);
	pipelineDesc.setVertexShader(vs);
	pipelineDesc.setFragmentShader(ps);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	pipelineDesc.inputLayout = inputLayoutHandle;

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
