#pragma once
#include <nvrhi/nvrhi.h>

#include "RenderPasses/ForwardPass.h"
#include "RenderPasses/DebugPass.h"

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
	class Scene;
	struct InstanceGroupInfo;
}
class DirectXDevice;

struct CBuffer {
	Matrix viewProj;
	Vector4 lightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
	Vector4 sceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
	Vector3 lightDirection = { 1.f, -1.f, 1.f };
	int instanceOffset{ 0 };
	Vector3 cameraPosition;
};

class ForwardRenderer {
public:
	std::shared_ptr<ForwardPass> ForwardPass;
	std::shared_ptr<DebugPass> DebugPass;

	nvrhi::ShaderHandle ForwardVertexShader;
	nvrhi::ShaderHandle ForwardPixelShader;
	nvrhi::BufferHandle ConstantBufferHandle;
	nvrhi::TextureHandle DepthBuffer;
	nvrhi::InputLayoutHandle InputLayoutHandle;

	nvrhi::CommandListHandle CommandList;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;
	nvrhi::GraphicsPipelineHandle WireframePipeline;

	void Init(std::shared_ptr<DirectXDevice> device, std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em);
	void Shutdown();

	void BeginFrame();
	void Render(ragdoll::Scene* scene);
private:
	std::shared_ptr<ragdoll::Window> PrimaryWindowRef;
	std::shared_ptr<ragdoll::FileManager> FileManagerRef;
	std::shared_ptr<ragdoll::EntityManager> EntityManagerRef;
	std::shared_ptr<DirectXDevice> DeviceRef;
	//handled at renderer
	void CreateResource();
};