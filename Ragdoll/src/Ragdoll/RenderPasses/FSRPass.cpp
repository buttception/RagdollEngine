#include "ragdollpch.h"
#include "FSRPass.h"

#include "Ragdoll/DirectXDevice.h"

void FSRPass::Init(Vector2 RenderRes, Vector2 DisplayRes, bool Debug)
{
	//create the ffx context
	BackendDesc.device = DirectXDevice::GetInstance()->m_Device12;

	UpscaleContextDesc.maxUpscaleSize = { (uint32_t)DisplayRes.x, (uint32_t)DisplayRes.y };
	UpscaleContextDesc.maxRenderSize = { (uint32_t)RenderRes.x, (uint32_t)RenderRes.y };
	UpscaleContextDesc.flags =
		Debug ? FFX_UPSCALE_ENABLE_DEBUG_CHECKING : 0 |
		FFX_UPSCALE_ENABLE_DEPTH_INFINITE |
		FFX_UPSCALE_ENABLE_DEPTH_INVERTED;

	ffx::ReturnCode ret = ffx::CreateContext(UpscaleContext, nullptr, UpscaleContextDesc, BackendDesc);
	RD_ASSERT(ret != ffx::ReturnCode::Ok, "Issue creating ffx context with {}", (uint32_t)ret);
}
