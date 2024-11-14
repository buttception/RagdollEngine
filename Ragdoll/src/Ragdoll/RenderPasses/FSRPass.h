#pragma once

#include <ffx_api/ffx_api.hpp>
#include <ffx_api/dx12/ffx_api_dx12.hpp>
#include <ffx_api/ffx_upscale.hpp>

class FSRPass {
	ffx::Context UpscaleContext{ nullptr };
	ffx::CreateBackendDX12Desc BackendDesc{};

	ffx::CreateContextDescUpscale UpscaleContextDesc;

public:
	void Init(Vector2 RenderRes, Vector2 DisplayRes, bool Debug);
	void Release();
};