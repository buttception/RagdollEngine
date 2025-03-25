#pragma once

#include <nvrhi/nvrhi.h>
#include <tiny_gltf.h>
#include "Ragdoll/Math/RagdollMath.h"
#include "meshoptimizer.h"

struct Material {
	int32_t AlbedoTextureIndex = -1;
	int32_t NormalTextureIndex = -1;
	int32_t RoughnessMetallicTextureIndex = -1;

	Vector4 Color;
	float Roughness = 1.f;
	float Metallic = 1.f;
	float AlphaCutoff = 0.5f;
	enum class AlphaMode {
		GLTF_OPAQUE,
		GLTF_MASK,
		GLTF_BLEND
	} AlphaMode = AlphaMode::GLTF_OPAQUE;
	bool bIsDoubleSided = false;
};

struct Vertex {
	Vector3 position = Vector3::Zero;
	Vector3 normal = Vector3::Zero;
	Vector3 tangent = Vector3::Zero;
	Vector2 texcoord = Vector2::Zero;
};

struct VertexBufferInfo
{
	uint32_t VerticesOffset;
	uint32_t IndicesOffset;
	uint32_t IndicesCount;
	uint32_t VerticesCount;

	uint32_t MeshletCount;
	uint32_t MeshletGroupOffset;
	uint32_t MeshletGroupPrimitivesOffset;
	uint32_t MeshletGroupVerticesOffset;

	//Best fit box for culling
	DirectX::BoundingBox BestFitBox;
};

struct Submesh 
{
	size_t VertexBufferIndex;
	size_t MaterialIndex;
};

struct Mesh
{
	std::vector<Submesh> Submeshes;
};

struct Image
{
	//raw byte data, loaded via stbi
	uint8_t* RawData{ nullptr };
	bool bIsDDS{ false };
	nvrhi::TextureHandle TextureHandle;
};

struct Texture
{
	int32_t ImageIndex{ -1 };
	int32_t SamplerIndex{ -1 };
};

enum class SamplerTypes
{
	Point_Clamp,
	Point_Repeat,
	Point_Mirror,
	Linear_Clamp,
	Linear_Repeat,
	Trilinear_Clamp,
	Trilinear_Repeat,
	Anisotropic_Repeat,
	Point_Clamp_Reduction,
	COUNT
};


using Bytes = std::pair<const void*, uint32_t>;
template<>
struct std::hash<Bytes> {
	std::size_t operator()(const Bytes& bytes) const noexcept
	{
		return std::hash<std::string_view>{}({ reinterpret_cast<const char*>(bytes.first), bytes.second });
	}
};

static auto hash = std::hash<Bytes>();

template<typename T>
size_t HashBytes(T* ptr) {
	return hash(std::make_pair(ptr, sizeof(T)));
}

constexpr size_t max_vertices = 64;
constexpr size_t max_triangles = 124;	//not 126 because they want it divisible by 4

class ForwardRenderer;
class DirectXDevice;
namespace ragdoll {
	class FileManager;
}
class AssetManager
{
public:
	static AssetManager* GetInstance();
	static void Release() { s_Instance.reset(); s_Instance = nullptr; }

	std::vector<VertexBufferInfo> VertexBufferInfos;
	std::vector<Mesh> Meshes;
	std::vector<Image> Images;
	std::vector<Texture> Textures;
	std::vector<Material> Materials;

	nvrhi::TextureHandle DefaultTex;
	nvrhi::TextureHandle ErrorTex;
	nvrhi::TextureHandle BlueNoise2D;
	std::vector<nvrhi::SamplerHandle> Samplers;
	nvrhi::SamplerHandle ShadowSampler;

	std::unordered_map<size_t, nvrhi::GraphicsPipelineHandle> GPSOs;
	std::unordered_map<size_t, nvrhi::ComputePipelineHandle> CPSOs;
	std::unordered_map<size_t, nvrhi::rt::PipelineHandle> RTSOs;
	std::unordered_map<size_t, nvrhi::MeshletPipelineHandle> MPSOs;
	std::unordered_map<size_t, nvrhi::BindingLayoutHandle> BindingLayouts;

	std::unordered_map<std::string, nvrhi::ShaderHandle> Shaders;
	std::unordered_map<std::string, nvrhi::ShaderLibraryHandle> ShaderLibraries;

	//no more vbo vectors, but instead is a vector of vb ib offsets
	//hold onto 1 vbo and ibo here
	nvrhi::BufferHandle VBO;
	nvrhi::BufferHandle IBO;
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	//meshlet data
	nvrhi::BufferHandle MeshletBuffer;
	nvrhi::BufferHandle MeshletVertexBuffer;
	nvrhi::BufferHandle MeshletPrimitiveBuffer;
	std::vector<meshopt_Meshlet> Meshlets;
	std::vector<uint32_t> MeshletVertices;
	std::vector<uint32_t> MeshletTrianglesPacked;
	//the information on how to use the global buffer
	std::vector<nvrhi::VertexAttributeDesc> InstancedVertexAttributes;
	nvrhi::InputLayoutHandle InstancedInputLayoutHandle;

	//descriptor table for all the bindless stuff
	nvrhi::DescriptorTableHandle DescriptorTable;
	nvrhi::BindingLayoutHandle BindlessLayoutHandle;

	size_t AddImage(Image img);
	nvrhi::GraphicsPipelineHandle GetGraphicsPipeline(const nvrhi::GraphicsPipelineDesc& desc, const nvrhi::FramebufferHandle& fb);
	nvrhi::ComputePipelineHandle GetComputePipeline(const nvrhi::ComputePipelineDesc& desc);
	nvrhi::rt::PipelineHandle GetRaytracePipeline(const nvrhi::rt::PipelineDesc& desc);
	nvrhi::MeshletPipelineHandle GetMeshletPipeline(const nvrhi::MeshletPipelineDesc& desc, const nvrhi::FramebufferHandle& fb);
	nvrhi::BindingLayoutHandle GetBindingLayout(const nvrhi::BindingSetDesc& desc);
	void RecompileShaders();

	void Init(std::shared_ptr<ragdoll::FileManager> fm);
	//this function will just add the vertices and indices, and populate the vector of objects
	size_t AddVertices(const std::vector<Vertex>& newVertices, const std::vector<uint32_t>& newIndices);
	//this function will create the buffer handles and copy the data over
	void UpdateMeshBuffers();
	//will create the global meshlet vectors, buffers will be created when update mesh buffers is called
	void UpdateMeshletsData();

	nvrhi::ShaderHandle GetShader(const std::string& shaderFilename);
	nvrhi::ShaderLibraryHandle GetShaderLibrary(const std::string& shaderFilename);
private:
	nvrhi::CommandListHandle CommandList;	//asset manager commandlist
	std::shared_ptr<ragdoll::FileManager> FileManagerRef;
	inline static std::unique_ptr<AssetManager> s_Instance;
	std::mutex Mutex;
};
