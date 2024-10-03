#pragma once
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
#include "Ragdoll/Input/InputHandler.h"
#include <imgui.h>

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

class DirectXDevice {
	inline static std::unique_ptr<DirectXDevice> s_Instance;
	bool bIsShutdown{ false };
	bool bIsCreated{ false };
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

	std::shared_ptr<ragdoll::Window> m_PrimaryWindow;
	std::shared_ptr<ragdoll::FileManager> m_FileManager;

	static DirectXDevice* GetInstance();
	static nvrhi::DeviceHandle GetNativeDevice();
	static void Release() { s_Instance.reset(); s_Instance = nullptr; }

	void Create(DeviceCreationParameters creationParam, std::shared_ptr<ragdoll::Window> win, std::shared_ptr <ragdoll::FileManager> fm);
	bool BeginFrame();
	void Present();
	void Shutdown();

	nvrhi::TextureHandle GetCurrentBackbuffer();
private:
	bool CreateDevice();
	bool CreateSwapChain();
	bool CreateRenderTargets();
	void ResizeSwapChain();
	void ReleaseRenderTargets();
	void DestroyDeviceAndSwapChain();
};