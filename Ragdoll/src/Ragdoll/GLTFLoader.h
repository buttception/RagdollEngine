#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include <nvrhi/nvrhi.h>

class ForwardRenderer;
namespace ragdoll {
	class FileManager;
	class EntityManager;
}
namespace ragdoll {
	struct Buffer {
		uint32_t IndicesByteOffset{};
		nvrhi::Format IndexFormat;
		nvrhi::BufferHandle VertexBuffer;
		nvrhi::BufferHandle IndexBuffer;
	};
}

class GLTFLoader {
	std::filesystem::path Root;
	ForwardRenderer* Renderer;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	std::shared_ptr<ragdoll::EntityManager> EntityManager;
public:
	void Init(std::filesystem::path root, ForwardRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em);
	void LoadAndCreateModel(const std::string& fileName);
private:
};