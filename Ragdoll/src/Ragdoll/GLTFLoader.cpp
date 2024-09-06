#include "ragdollpch.h"

#include "GLTFLoader.h"
#include "TestRenderer.h"
#include "DirectXDevice.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

void GLTFLoader::Init(std::filesystem::path root, TestRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm)
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
	auto path = Root / fileName;
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
	RD_ASSERT(ret == false, "Issue loading {}", path.string());

	//go downwards from meshes
	for (const auto& itMesh : model.meshes) {
		//for every mesh, create a new mesh object
		meshesRef[itMesh.name];
		auto& mesh = meshesRef[itMesh.name];
		//keep track of what is the currently loaded buffer and data
		int32_t bufferIndex = -1; //-1 is invalid index
		uint32_t size = 0;
		const uint8_t* data = nullptr;	//nullptr is invalid
		for (const auto& itPrim : itMesh.primitives) {	//there can be more than 1 primitive???
			//for every primitive in the mesh
			for (const auto& itAttrib : itPrim.attributes) {
				//for every attribute in the primitive, it points to the id of a assessor
				const auto& accessor = model.accessors[itAttrib.second];
				//the assessor contains the id to the bufferview
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				//bufferView contains the id to the data
				if (bufferIndex != bufferView.buffer) {
					const auto& buffer = model.buffers[bufferView.buffer];
					//buffer is currently not loaded
					data = FileManager->ImmediateLoad(buffer.uri, size);
					mesh.Buffers.TotalByteSize = size;
				}
				//create the attrib
				nvrhi::VertexAttributeDesc attrib;
				attrib.name = itAttrib.first;	//first is the name of the attrib
				attrib.offset = accessor.byteOffset;
				attrib.bufferIndex = 0; //using only one vb, if got more primitives need add
				//set format by gltf type
				switch (accessor.type) {
				case TINYGLTF_TYPE_VEC3:
					attrib.format = nvrhi::Format::RGB32_FLOAT;
					//TODO: data is interleaved or not
					//mesh.Buffers.Attribs.AttribsTotalByteSize += 3 * sizeof(float);
					//RD_CORE_TRACE("total float size for attrib is {}", mesh.Buffers.Attribs.AttribsTotalByteSize);
					break;
				}
				//push the attrib into the attribs
				mesh.Buffers.Attribs.AttribsDesc.emplace_back(attrib);
			}
			//once done need to update all the element strides
			for (auto& attribDesc : mesh.Buffers.Attribs.AttribsDesc) {
				attribDesc.elementStride = 12;
			}
			//initialize the attributes
			mesh.Buffers.Attribs.CreateInputLayoutHandle(Renderer->Device->m_NvrhiDevice, Renderer->TestVertexShader);
			//now load the index buffer
			const auto& accessor = model.accessors[itPrim.indices];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			mesh.Buffers.IndicesByteOffset = bufferView.byteOffset;
			switch (accessor.componentType) {
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				mesh.Buffers.IndexFormat = nvrhi::Format::R16_UINT;
				break;
			}
			//create the buffer
			mesh.Buffers.CreateBuffers(Renderer, data, itMesh.name.c_str());
		}
	}
	//TODO: create the scene hireachy
	
}

void ragdoll::Buffer::CreateBuffers(TestRenderer* renderer, const uint8_t* bytes, const std::string meshName)
{
	//presume command list is open and will be closed and executed later
	nvrhi::BufferDesc vertexBufDesc;
	vertexBufDesc.byteSize = IndicesByteOffset;	//the offset is already the size of the vb
	vertexBufDesc.isVertexBuffer = true;
	vertexBufDesc.debugName = meshName + " Vertex Buffer";
	vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
	//smth smth syncrhonization need to be this state to be written

	VertexBuffer = renderer->Device->m_NvrhiDevice->createBuffer(vertexBufDesc);
	//copy data over
	renderer->CommandList->beginTrackingBufferState(VertexBuffer, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
	renderer->CommandList->writeBuffer(VertexBuffer, bytes, vertexBufDesc.byteSize);
	renderer->CommandList->setPermanentBufferState(VertexBuffer, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

	nvrhi::BufferDesc indexBufDesc;
	indexBufDesc.byteSize = TotalByteSize - IndicesByteOffset;
	indexBufDesc.isIndexBuffer = true;
	indexBufDesc.debugName = meshName + " Index Buffer";
	indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

	IndexBuffer = renderer->Device->m_NvrhiDevice->createBuffer(indexBufDesc);
	renderer->CommandList->beginTrackingBufferState(IndexBuffer, nvrhi::ResourceStates::CopyDest);
	renderer->CommandList->writeBuffer(IndexBuffer, bytes + IndicesByteOffset, indexBufDesc.byteSize);
	renderer->CommandList->setPermanentBufferState(IndexBuffer, nvrhi::ResourceStates::IndexBuffer);
}
