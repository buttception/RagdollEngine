#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include <nvrhi/nvrhi.h>

class ForwardRenderer;
namespace ragdoll {
	class FileManager;
}
struct Vertex
{
	Vector3 position;
	Vector3 color;
};

namespace ragdoll {
	struct Attributes {
		std::vector<nvrhi::VertexAttributeDesc> AttribsDesc;
		uint32_t AttribsTotalByteSize{};
		nvrhi::InputLayoutHandle InputLayoutHandle;

		void CreateInputLayoutHandle(nvrhi::DeviceHandle device, nvrhi::ShaderHandle vs) {

			InputLayoutHandle = device->createInputLayout(AttribsDesc.data(), uint32_t(std::size(AttribsDesc)), vs);
		}
	};
	struct Buffer {
		Attributes Attribs;
		uint32_t TotalByteSize{};
		uint32_t IndicesByteOffset{};
		nvrhi::Format IndexFormat;
		nvrhi::BufferHandle VertexBuffer;
		nvrhi::BufferHandle IndexBuffer;
		//can decide if we want to keep in memory next time?

		void CreateBuffers(ForwardRenderer* renderer, const uint8_t* bytes, const std::string meshName);
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