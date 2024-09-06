#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include <nvrhi/nvrhi.h>

class ForwardRenderer;
namespace ragdoll {
	class FileManager;
}
namespace ragdoll {
	struct Buffer {
		uint32_t IndicesByteOffset{};
		nvrhi::Format IndexFormat;
		nvrhi::BufferHandle VertexBuffer;
		nvrhi::BufferHandle IndexBuffer;
	};
	struct Mesh {
		std::string Name;
		//meshes refers to a bunch of primitives
		//this will contain all the attributes and buffers
		Buffer Buffers;
		//TODO: it also refers to a material


	};
}

class GLTFLoader {
	std::filesystem::path Root;
	ForwardRenderer* Renderer;
	std::shared_ptr<ragdoll::FileManager> FileManager;
public:
	void Init(std::filesystem::path root, ForwardRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm);
	void LoadAndCreateModel(const std::string& fileName, std::unordered_map<std::string, ragdoll::Mesh>& meshesRef);
private:
};