#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
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

class Renderer {
public:
	std::shared_ptr<DirectXDevice> Device;

	//TODO: see if can move into imgui renderer
	nvrhi::ShaderHandle ImguiVertexShader;
	nvrhi::ShaderHandle ImguiPixelShader;

	nvrhi::CommandListHandle CommandList;
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;
	nvrhi::GraphicsPipelineHandle WireframePipeline;

	//the global vertex buffer
	std::vector<nvrhi::VertexAttributeDesc> VertexAttributes;
	nvrhi::InputLayoutHandle InputLayoutHandle;
	
	//descriptor table for all the bindless stuff
	nvrhi::DescriptorTableHandle DescriptorTable;
	nvrhi::BindingLayoutHandle BindlessLayoutHandle;
	uint32_t TextureCount{};

	void CreateDevice(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm);
	int32_t AddTextureToTable(nvrhi::TextureHandle tex);

	virtual void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em) = 0;
	virtual void Shutdown() = 0;

	virtual void BeginFrame() = 0;
	virtual void DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, CBuffer& Cbuf) = 0;
	virtual void DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, uint32_t instanceCount, CBuffer& Cbuf) = 0;
protected:
	std::shared_ptr<ragdoll::Window> PrimaryWindow;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	std::shared_ptr<ragdoll::EntityManager> EntityManager;

};