#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include "Ragdoll/AssetManager.h"
#include <nvrhi/nvrhi.h>
#include <taskflow.hpp>

class DirectXDevice;
namespace ragdoll {
	class FileManager;
	class EntityManager;
	class Scene;
}

class GLTFLoader {
	std::filesystem::path Root;
	std::shared_ptr<ragdoll::FileManager> FileManagerRef;
	std::shared_ptr<ragdoll::EntityManager> EntityManagerRef;
	std::shared_ptr<ragdoll::Scene> SceneRef;

	std::vector<uint32_t> IndexStagingBuffer;
	std::vector<Vertex> VertexStagingBuffer;

	nvrhi::CommandListHandle CommandList;

	tf::Executor Executor;
	tf::Taskflow TaskFlow;
public:
	GLTFLoader() : Executor(8) {}

	void Init(std::filesystem::path root, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> tl);
	void LoadAndCreateModel(const std::string& fileName);
private:
};