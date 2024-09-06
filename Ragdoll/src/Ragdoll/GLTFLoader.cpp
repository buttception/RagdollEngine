#include "ragdollpch.h"

#include "GLTFLoader.h"
#include "ForwardRenderer.h"
#include "DirectXDevice.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

void GLTFLoader::Init(std::filesystem::path root, ForwardRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm)
{
	Root = root;
	Renderer = renderer;
	FileManager = fm;
}

void GLTFLoader::LoadAndCreateModel(const std::string& fileName, std::unordered_map<std::string, ragdoll::Mesh>& meshesRef)
{
	//COMMAND LIST MUST BE OPEN FOR THIS FUNCTION TO EXECUTE
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err, warn;
	std::filesystem::path path = Root / fileName;
	std::filesystem::path modelRoot = path.parent_path().lexically_relative(Root);
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
	RD_ASSERT(ret == false, "Issue loading {}", path.string());

	//go downwards from meshes
	for (const auto& itMesh : model.meshes) {
		//TODO: RECONCILE BETWEEN THE ATTRIBUTES HERE AND THE ATTRIBUTES MY RENDER USES
		//should not create a new input layout handle, use the one provided by the renderer
		std::vector<Vertex> newBuffer;
		struct TinygltfPair {
			tinygltf::Accessor accessor;
			tinygltf::BufferView bufferView;
		};
		std::unordered_map<std::string, TinygltfPair> attributeToAccessors;
		//for every mesh, create a new mesh object
		meshesRef[itMesh.name];
		auto& mesh = meshesRef[itMesh.name];
		//keep track of what is the currently loaded buffer and data
		int32_t bufferIndex = -1; //-1 is invalid index
		uint32_t binSize = 0;
		uint32_t vertexCount{};
		const uint8_t* data = nullptr;	//nullptr is invalid

		//add the relevant data into the map to use
		//assume only 1 primitives
		const auto& itPrim = itMesh.primitives[0];
		for (const auto& itAttrib : itPrim.attributes) {
			tinygltf::Accessor accessor = model.accessors[itAttrib.second];
			tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
			TinygltfPair pair{ accessor, bufferView };
			attributeToAccessors[itAttrib.first] = pair;
			if (bufferIndex != bufferView.buffer) {
				vertexCount = accessor.count;
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
				//buffer is currently not loaded
				data = FileManager->ImmediateLoad(modelRoot / buffer.uri, binSize);
			}
		}
		//iterate through the map, update the new values
		RD_ASSERT(vertexCount == 0, "There are no vertices?");
		std::vector<Vertex> vertices;
		vertices.resize(vertexCount);
		for (const auto& it : attributeToAccessors) {
			nvrhi::VertexAttributeDesc const* desc = nullptr;
			uint32_t size;
			for (const nvrhi::VertexAttributeDesc& itDesc : Renderer->VertexAttributes) {
				if (itDesc.name == it.first)
				{
					desc = &itDesc;
					switch (it.second.accessor.type) {
					case TINYGLTF_TYPE_VEC3:
						size = 12;
						break;
					case TINYGLTF_TYPE_VEC2:
						size = 8;
						break;
					default:
						RD_ASSERT(true, "Unsupported type, please support it now");
					}
				}
			}
			RD_ASSERT(desc == nullptr, "Loaded mesh contains a attribute not supported by the renderer: {}", it.first);
			for (int i = 0; i < vertexCount; ++i) {
				uint8_t* bytePos = reinterpret_cast<uint8_t*>(&vertices[i]) + desc->offset;
				memcpy(bytePos, data + it.second.bufferView.byteOffset + i * it.second.bufferView.byteStride + it.second.accessor.byteOffset, size);
			}
		}
		//deal with index buffer
		const tinygltf::Accessor& accessor = model.accessors[itPrim.indices];
		const tinygltf::BufferView& bufferView = model.bufferViews[itPrim.indices];
		std::vector<uint32_t> indices;
		indices.resize(accessor.count);
		for (int i = 0; i < accessor.count; ++i) {
			switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_SHORT:
				indices[i] = *reinterpret_cast<int16_t*>(const_cast<uint8_t*>(data + bufferView.byteOffset + i * 2));
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				indices[i] = *reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(data + bufferView.byteOffset + i * 2));
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				indices[i] = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(data + bufferView.byteOffset + i * 2));
				break;
			}
		}
		
		//presume command list is open and will be closed and executed later
		nvrhi::BufferDesc vertexBufDesc;
		vertexBufDesc.byteSize = vertices.size() * sizeof(Vertex);	//the offset is already the size of the vb
		vertexBufDesc.isVertexBuffer = true;
		vertexBufDesc.debugName = itMesh.name + " Vertex Buffer";
		vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
		//smth smth syncrhonization need to be this state to be written

		mesh.Buffers.VertexBuffer = Renderer->Device->m_NvrhiDevice->createBuffer(vertexBufDesc);
		//copy data over
		Renderer->CommandList->beginTrackingBufferState(mesh.Buffers.VertexBuffer, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
		Renderer->CommandList->writeBuffer(mesh.Buffers.VertexBuffer, vertices.data(), vertexBufDesc.byteSize);
		Renderer->CommandList->setPermanentBufferState(mesh.Buffers.VertexBuffer, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

		nvrhi::BufferDesc indexBufDesc;
		indexBufDesc.byteSize = indices.size() * sizeof(uint32_t);
		indexBufDesc.isIndexBuffer = true;
		indexBufDesc.debugName = itMesh.name + " Index Buffer";
		indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

		mesh.Buffers.IndexBuffer = Renderer->Device->m_NvrhiDevice->createBuffer(indexBufDesc);
		Renderer->CommandList->beginTrackingBufferState(mesh.Buffers.IndexBuffer, nvrhi::ResourceStates::CopyDest);
		Renderer->CommandList->writeBuffer(mesh.Buffers.IndexBuffer, indices.data(), indexBufDesc.byteSize);
		Renderer->CommandList->setPermanentBufferState(mesh.Buffers.IndexBuffer, nvrhi::ResourceStates::IndexBuffer);

		for(const auto& it : vertices)
		{
			RD_CORE_INFO("Pos: {}, Normal: {}, Texcoord: {}", it.position, it.normal, it.texcoord);
		}
		for(const auto& it : indices)
		{
			RD_CORE_INFO("Index: {}", it);
		}
	}
	//TODO: create the scene hireachy through entt
	
}
