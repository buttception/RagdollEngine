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
	Matrix invWorldMatrix;
	Matrix viewProj;
	Vector4 lightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
	Vector4 sceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
	Vector4 albedoFactor = { 1.f,1.f,1.f,1.f };
	Vector3 lightDirection = { 1.f, -1.f, 1.f };
	Vector3 cameraPosition;

	float roughness;
	float metallic;

	int32_t useAlbedo{ false };
	int32_t useNormalMap{ false };
	int32_t useMetallicRoughnessMap{ false };
};
struct Vertex {
	Vector3 position = Vector3::Zero;
	Vector3 normal = Vector3::Zero;
	Vector3 tangent = Vector3::Zero;
	Vector3 binormal = Vector3::Zero;
	Vector2 texcoord = Vector2::Zero;
};
class ForwardRenderer {
	//handled at the renderer side
	nvrhi::BindingLayoutHandle BindingLayoutHandle;
	nvrhi::BindingSetHandle BindingSetHandle;
	nvrhi::BufferHandle VertexBuffer;
	nvrhi::BufferHandle IndexBuffer;

	nvrhi::TextureHandle DepthBuffer;
	nvrhi::GraphicsPipelineHandle GraphicsPipeline;

	std::shared_ptr<ragdoll::Window> PrimaryWindow;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	std::shared_ptr<ragdoll::EntityManager> EntityManager;
public:
	std::shared_ptr<DirectXDevice> Device;
	nvrhi::CommandListHandle CommandList;
	nvrhi::ShaderHandle ImguiVertexShader;
	nvrhi::ShaderHandle ImguiPixelShader;
	nvrhi::ShaderHandle ForwardVertexShader;
	nvrhi::ShaderHandle ForwardPixelShader;
	nvrhi::BufferHandle ConstantBuffer;
	std::vector<nvrhi::VertexAttributeDesc> VertexAttributes;
	nvrhi::InputLayoutHandle InputLayoutHandle;
	
	void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em);
	void Draw();
	void Shutdown();
private:
	//handled at renderer
	void CreateResource();
};