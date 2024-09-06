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
};
class ForwardRenderer {
	//handled at the renderer side
	nvrhi::BindingLayoutHandle BindingLayout;
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
	nvrhi::ShaderHandle TestVertexShader;
	nvrhi::ShaderHandle TestPixelShader;
	//keep all the meshes in the renderer for now
	std::unordered_map<std::string, ragdoll::Mesh> Meshes;
	
	void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm);
	void Draw();
	void Shutdown();
private:
	//handled at renderer
	void CreateResource();
};