#pragma once

#include <nvrhi/nvrhi.h>
#include <tiny_gltf.h>
#include "Ragdoll/Math/RagdollMath.h"

struct Material {
	int32_t AlbedoTextureIndex = -1;
	int32_t NormalTextureIndex = -1;
	int32_t RoughnessMetallicTextureIndex = -1;

	Vector4 Color;
	float Roughness = 1.f;
	float Metallic = 1.f;

	bool bIsLit = false;
};

struct Vertex {
	Vector3 position = Vector3::Zero;
	Vector3 normal = Vector3::Zero;
	Vector3 tangent = Vector3::Zero;
	Vector2 texcoord = Vector2::Zero;
};

struct VertexBufferInfo
{
	uint32_t VBOffset;
	uint32_t IBOffset;
	uint32_t IndicesCount;
	uint32_t VerticesCount;

	//Best fit box for culling
	DirectX::BoundingBox BestFitBox;
};

struct Submesh 
{
	int32_t VertexBufferIndex;
	int32_t MaterialIndex;
};

struct Mesh
{
	std::vector<Submesh> Submeshes;
};

struct Image
{
	nvrhi::TextureHandle TextureHandle;
};

struct Texture
{
	int32_t ImageIndex;
	int32_t SamplerIndex;
};

enum class SamplerTypes
{
	Point_Clamp,
	Point_Wrap,
	Point_Repeat,
	Linear_Clamp,
	Linear_Wrap,
	Linear_Repeat,
	Trilinear_Clamp,
	Trilinear_Wrap,
	Trilinear_Repeat,
	COUNT
};

class ForwardRenderer;
class AssetManager
{
public:
	static AssetManager* GetInstance();

	std::vector<VertexBufferInfo> VertexBufferInfos;
	std::vector<Mesh> Meshes;
	std::vector<Image> Images;
	std::vector<Texture> Textures;
	std::vector<Material> Materials;

	nvrhi::TextureHandle DefaultTex;
	nvrhi::SamplerHandle DefaultSampler;
	std::vector<nvrhi::SamplerHandle> Samplers;

	//no more vbo vectors, but instead is a vector of vb ib offsets
	//hold onto 1 vbo and ibo here
	nvrhi::BufferHandle VBO;
	nvrhi::BufferHandle IBO;
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;
	//this function will just add the vertices and indices, and populate the vector of objects
	uint32_t AddVertices(const std::vector<Vertex>& newVertices, const std::vector<uint32_t>& newIndices);
	//this function will create the buffer handles and copy the data over
	void UpdateVBOIBO(ForwardRenderer* device);
private:
	inline static std::unique_ptr<AssetManager> s_Instance;
};
