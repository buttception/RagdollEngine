#pragma once
#include <nvrhi/nvrhi.h>
#include "GLTFLoader.h"
namespace ragdoll {
	class Window;
	class FileManager;
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
	//handled at the renderer side
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::BufferHandle VertexBuffer;
	nvrhi::BufferHandle IndexBuffer;

	nvrhi::GraphicsPipelineHandle GraphicsPipeline;

	std::shared_ptr<ragdoll::Window> PrimaryWindow;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	std::shared_ptr<ragdoll::EntityManager> EntityManager;
	int32_t TextureCount{};
public:
	std::shared_ptr<DirectXDevice> Device;
	nvrhi::CommandListHandle CommandList;
	nvrhi::ShaderHandle ImguiVertexShader;
	nvrhi::ShaderHandle ImguiPixelShader;
	nvrhi::ShaderHandle ForwardVertexShader;
	nvrhi::ShaderHandle ForwardPixelShader;
	nvrhi::BufferHandle ConstantBuffer;
	nvrhi::TextureHandle DepthBuffer;
	std::vector<nvrhi::VertexAttributeDesc> VertexAttributes;
	nvrhi::InputLayoutHandle InputLayoutHandle;
	nvrhi::DescriptorTableHandle DescriptorTable;
	nvrhi::BindingLayoutHandle BindlessLayoutHandle;
	
	void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em);
	void Shutdown();

	void CreateCustomMeshes();
	int32_t AddTextureToTable(nvrhi::TextureHandle tex);

	void BeginFrame(CBuffer& Cbug);
	void DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, CBuffer& Cbuf);
private:
	//handled at renderer
	void CreateResource();
};