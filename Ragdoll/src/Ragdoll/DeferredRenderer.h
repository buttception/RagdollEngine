#pragma once
#include <nvrhi/nvrhi.h>

#include "RenderPasses/SkyGeneratePass.h"
#include "RenderPasses/GBufferPass.h"
#include "RenderPasses/ShadowPass.h"
#include "RenderPasses/ShadowMaskPass.h"
#include "RenderPasses/DeferredLightPass.h"
#include "RenderPasses/SkyPass.h"
#include "RenderPasses/CACAOPass.h"
#include "RenderPasses/DebugPass.h"
#include "RenderPasses/FramebufferViewer.h"
#include "RenderPasses/AutomaticExposurePass.h"
#include "RenderPasses/ToneMapPass.h"
#include "RenderPasses/BloomPass.h"
#include "RenderPasses/XeGTAOPass.h"
#include "RenderPasses/FinalPass.h"

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
	class Scene;
	struct SceneRenderTargets;
	struct InstanceGroupInfo;
}
class DirectXDevice;
class ImguiRenderer;

class Renderer {
public:
	std::shared_ptr<SkyGeneratePass> SkyGeneratePass;
	std::shared_ptr<GBufferPass> GBufferPass;
	std::shared_ptr<CACAOPass> CACAOPass;
	std::shared_ptr<XeGTAOPass> XeGTAOPass;
	std::shared_ptr<ShadowPass> ShadowPass;
	std::shared_ptr<ShadowMaskPass> ShadowMaskPass;
	std::shared_ptr<DeferredLightPass> DeferredLightPass;
	std::shared_ptr<SkyPass> SkyPass;
	std::shared_ptr<BloomPass> BloomPass;
	std::shared_ptr<AutomaticExposurePass> AutomaticExposurePass;
	std::shared_ptr<ToneMapPass> ToneMapPass;
	std::shared_ptr<FinalPass> FinalPass;
	std::shared_ptr<DebugPass> DebugPass;
	std::shared_ptr<FramebufferViewer> FramebufferViewer;

	enum class Pass {
		SKY_GENERATE,
		GBUFFER,
		AO,
		SHADOW_DEPTH0,
		SHADOW_DEPTH1,
		SHADOW_DEPTH2,
		SHADOW_DEPTH3,
		SHADOW_MASK,
		LIGHT,
		SKY,
		BLOOM,
		EXPOSURE,
		TONEMAP,
		FINAL,
		DEBUG,
		FB_VIEWER,

		COUNT
	};

	std::vector<nvrhi::CommandListHandle> CommandLists;

	ragdoll::SceneRenderTargets* RenderTargets;
	nvrhi::CommandListHandle CommandList;

	//debug infos
	float AdaptedLuminance;

	void Init(std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene);
	void Shutdown();

	void BeginFrame();
	void Render(ragdoll::Scene* scene, float _dt, std::shared_ptr<ImguiRenderer> imgui);
private:
	std::shared_ptr<ragdoll::Window> PrimaryWindowRef;
	bool bIsOddFrame{ false };
	//handled at renderer
	void CreateResource();
};