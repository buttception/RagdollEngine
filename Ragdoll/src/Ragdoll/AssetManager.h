#pragma once

#include <nvrhi/nvrhi.h>
#include <tiny_gltf.h>

struct Mesh
{
	nvrhi::BufferHandle VertexBufferHandle;
	nvrhi::BufferHandle IndexBufferHandle;
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

class AssetManager
{
public:
	static AssetManager* GetInstance();

	std::vector<Mesh> Meshes;
	std::vector<Image> Images;
	std::vector<Texture> Textures;
	tinygltf::Model Model;
private:
	inline static std::unique_ptr<AssetManager> s_Instance;
};
