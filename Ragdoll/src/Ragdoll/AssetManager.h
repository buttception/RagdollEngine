#pragma once

#include <nvrhi/nvrhi.h>
#include <tiny_gltf.h>

struct Material {
	int32_t AlbedoIndex = -1;
	int32_t NormalIndex = -1;
	int32_t MetallicRoughnessIndex = -1;

	Vector4 Color;
	float Roughness = 1.f;
	float Metallic = 1.f;

	bool bIsLit = false;
};

struct VertexBufferObject
{
	nvrhi::BufferHandle VertexBufferHandle;
	nvrhi::BufferHandle IndexBufferHandle;
	uint32_t TriangleCount;
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
	nvrhi::SamplerHandle SamplerHandle;
};

enum class SamplerTypes
{
	POINT_CLAMP,
	LINEAR_CLAMP,
};

class AssetManager
{
public:
	static AssetManager* GetInstance();

	std::vector<VertexBufferObject> VBOs;
	std::vector<Mesh> Meshes;
	std::vector<Image> Images;
	std::vector<Texture> Textures;
	std::vector<Material> Materials;

	nvrhi::TextureHandle DefaultTex;
	nvrhi::SamplerHandle DefaultSampler;
private:
	inline static std::unique_ptr<AssetManager> s_Instance;
};
