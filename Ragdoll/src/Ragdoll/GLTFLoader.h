#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include <nvrhi/nvrhi.h>

class ForwardRenderer;
namespace ragdoll {
	class FileManager;
	class EntityManager;
	class Scene;
}

class GLTFLoader {
	std::filesystem::path Root;
	ForwardRenderer* Renderer;
	std::shared_ptr<ragdoll::FileManager> FileManager;
	std::shared_ptr<ragdoll::EntityManager> EntityManager;
	std::shared_ptr<ragdoll::Scene> Scene;
public:
	void Init(std::filesystem::path root, ForwardRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> tl);
	void LoadAndCreateModel(const std::string& fileName);
private:
};