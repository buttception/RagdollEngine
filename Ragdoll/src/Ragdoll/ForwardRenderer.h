#pragma once
#include <nvrhi/nvrhi.h>

#include "RenderPasses/ForwardPass.h"
#include "RenderPasses/DebugPass.h"
#include "RenderPasses/ShadowPass.h"

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
	class Scene;
	struct InstanceGroupInfo;
}
class DirectXDevice;

class ForwardRenderer {
public:
	std::shared_ptr<ForwardPass> ForwardPass;
	std::shared_ptr<DebugPass> DebugPass;
	std::shared_ptr<ShadowPass> ShadowPass;

	nvrhi::TextureHandle DepthHandle;
	nvrhi::TextureHandle ShadowMap[4];
	nvrhi::CommandListHandle CommandList;

	void Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win, ragdoll::Scene* scene);
	void Shutdown();

	void BeginFrame();
	void Render(ragdoll::Scene* scene);
private:
	std::shared_ptr<ragdoll::Window> PrimaryWindowRef;
	std::shared_ptr<DirectXDevice> DeviceRef;
	//handled at renderer
	void CreateResource();
};