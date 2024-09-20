#pragma once
#include <nvrhi/nvrhi.h>

#include "RenderPasses/GBufferPass.h"
#include "RenderPasses/DeferredLightPass.h"
#include "RenderPasses/DebugPass.h"

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
	class Scene;
	struct InstanceGroupInfo;
}
class DirectXDevice;

class DeferredRenderer {
public:
	std::shared_ptr<GBufferPass> GBufferPass;
	std::shared_ptr<DeferredLightPass> DeferredLightPass;
	std::shared_ptr<DebugPass> DebugPass;

	nvrhi::TextureHandle AlbedoHandle;
	nvrhi::TextureHandle NormalHandle;
	nvrhi::TextureHandle RoughnessMetallicHandle;
	nvrhi::TextureHandle DepthBuffer;
	nvrhi::FramebufferHandle GBuffer;
	nvrhi::CommandListHandle CommandList;

	void Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win);
	void Shutdown();

	void BeginFrame();
	void Render(ragdoll::Scene* scene);
private:
	std::shared_ptr<ragdoll::Window> PrimaryWindowRef;
	std::shared_ptr<DirectXDevice> DeviceRef;
	//handled at renderer
	void CreateResource();
};