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
#include "RenderPasses/AutomaticExposurePass.h"
#include "RenderPasses/ToneMapPass.h"
#include "RenderPasses/BloomPass.h"

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
	class Scene;
	struct InstanceGroupInfo;
}
class DirectXDevice;

class Renderer {
public:

	std::shared_ptr<SkyGeneratePass> SkyGeneratePass;
	std::shared_ptr<GBufferPass> GBufferPass;
	std::shared_ptr<CACAOPass> CACAOPass;
	std::shared_ptr<ShadowPass> ShadowPass;
	std::shared_ptr<ShadowMaskPass> ShadowMaskPass;
	std::shared_ptr<DeferredLightPass> DeferredLightPass;
	std::shared_ptr<SkyPass> SkyPass;
	std::shared_ptr<BloomPass> BloomPass;
	std::shared_ptr<AutomaticExposurePass> AutomaticExposurePass;
	std::shared_ptr<ToneMapPass> ToneMapPass;
	std::shared_ptr<DebugPass> DebugPass;

	nvrhi::TextureHandle SkyTexture;
	nvrhi::TextureHandle SkyThetaGammaTable;
	nvrhi::TextureHandle SceneColor;
	nvrhi::TextureHandle AlbedoHandle;
	nvrhi::TextureHandle NormalHandle;
	nvrhi::TextureHandle AORoughnessMetallicHandle;
	nvrhi::TextureHandle DepthHandle;
	nvrhi::FramebufferHandle GBuffer;
	nvrhi::TextureHandle ShadowMap[4];
	nvrhi::TextureHandle ShadowMask;
	const std::vector<BloomMip>* Mips;
	nvrhi::TextureHandle DeinterleavedDepth;
	nvrhi::TextureHandle DeinterleavedNormals;
	nvrhi::TextureHandle SSAOPong;
	nvrhi::CommandListHandle CommandList;

	//debug infos
	float AdaptedLuminance;

	void Init(std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene);
	void Shutdown();

	void BeginFrame();
	void Render(ragdoll::Scene* scene, float _dt);
private:
	std::shared_ptr<ragdoll::Window> PrimaryWindowRef;
	//handled at renderer
	void CreateResource();
};