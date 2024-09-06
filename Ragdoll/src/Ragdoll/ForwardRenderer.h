#pragma once
#include <nvrhi/nvrhi.h>
#include "GLTFLoader.h"
namespace ragdoll {
	class Window;
	class FileManager;
}
class DirectXDevice;

struct CBuffer {
	Matrix world;
	Matrix viewProj;
	Vector4 lightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
	Vector4 sceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
	Vector3 lightDirection = { 1.f, -1.f, 1.f };
};
class ForwardRenderer {
	//handled at the renderer side
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::BufferHandle IndexBuffer;
	nvrhi::BufferHandle VertexBuffer;
	nvrhi::TextureHandle DepthBuffer;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;

	std::shared_ptr<ragdoll::Window> PrimaryWindow;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	//my gltf loader
	GLTFLoader Loader;
public:
	std::shared_ptr<DirectXDevice> Device;
	nvrhi::CommandListHandle CommandList;
	nvrhi::ShaderHandle ImguiVertexShader;
	nvrhi::ShaderHandle ImguiPixelShader;
	nvrhi::ShaderHandle ForwardVertexShader;
	nvrhi::ShaderHandle ForwardPixelShader;
	nvrhi::BufferHandle ConstantBuffer;
	//keep all the meshes in the renderer for now
	std::unordered_map<std::string, ragdoll::Mesh> Meshes;
	
	void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm);
	void Draw();
	void Shutdown();
private:
	//handled at renderer
	void CreateResource();
};