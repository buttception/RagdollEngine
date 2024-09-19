#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include "Ragdoll/AssetManager.h"
#include <nvrhi/nvrhi.h>

class Renderer;
namespace ragdoll {
	class FileManager;
	class EntityManager;
	class Scene;
}

class GLTFLoader {
	std::filesystem::path Root;
	Renderer* Renderer;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	std::shared_ptr<ragdoll::EntityManager> EntityManager;
	std::shared_ptr<ragdoll::Scene> Scene;

	std::vector<uint32_t> IndexStagingBuffer;
	std::vector<Vertex> VertexStagingBuffer;
public:
	void Init(std::filesystem::path root, class Renderer* renderer, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> tl);
	void LoadAndCreateModel(const std::string& fileName);
private:
};