#include "ragdollpch.h"

#include "GLTFLoader.h"
#include "ForwardRenderer.h"
#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"

#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Ragdoll/Components/MaterialComp.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include <tiny_gltf.h>

#include "../../../Editor/src/Asset/Asset.h"

void GLTFLoader::Init(std::filesystem::path root, ForwardRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em)
{
	Root = root;
	Renderer = renderer;
	FileManager = fm;
	EntityManager = em;
}

void TraverseNode(int32_t currIndex, const tinygltf::Model& model, std::shared_ptr<ragdoll::EntityManager> em)
{
	const tinygltf::Node& curr = model.nodes[currIndex];
	//create the entity
	entt::entity ent = em->CreateEntity();
	em->AddComponent<TransformComp>(ent);
	if (curr.mesh >= 0) {
		RenderableComp* renderableComp = em->AddComponent<RenderableComp>(ent);
		renderableComp->meshIndex = curr.mesh;
		MaterialComp* matComp = em->AddComponent<MaterialComp>(ent);
		if (model.meshes[curr.mesh].primitives[0].material >= 0) {
			tinygltf::Material gltfMat = model.materials[model.meshes[curr.mesh].primitives[0].material];

			matComp->Metallic = gltfMat.pbrMetallicRoughness.metallicFactor;
			matComp->Roughness = gltfMat.pbrMetallicRoughness.roughnessFactor;
			matComp->Color = Vector4(
				gltfMat.pbrMetallicRoughness.baseColorFactor[0],
				gltfMat.pbrMetallicRoughness.baseColorFactor[1],
				gltfMat.pbrMetallicRoughness.baseColorFactor[2],
				gltfMat.pbrMetallicRoughness.baseColorFactor[3]);
			matComp->AlbedoIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
			matComp->MetallicRoughnessIndex = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index;
			matComp->NormalIndex = gltfMat.normalTexture.index;
			matComp->bIsLit = true;
		}
		else
			matComp->bIsLit = false;
	}
	//TODO: transforms

	const tinygltf::Node& root = model.nodes[currIndex];
	for (const int& childIndex : root.children) {
		TraverseNode(childIndex, model, em);
	}
}

void GLTFLoader::LoadAndCreateModel(const std::string& fileName)
{
	//ownself open command list
	Renderer->CommandList->open();
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
		Mesh mesh;
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
		bool tangentExist = false, binormalExist = false;
		RD_ASSERT(vertexCount == 0, "There are no vertices?");
		std::vector<Vertex> vertices;
		vertices.resize(vertexCount);
		for (const auto& it : attributeToAccessors) {
			nvrhi::VertexAttributeDesc const* desc = nullptr;
			uint32_t size;
			for (const nvrhi::VertexAttributeDesc& itDesc : Renderer->VertexAttributes) {
				if (it.first.find(itDesc.name) != std::string::npos)
				{
					if (it.first.find("TANGENT") != std::string::npos)
						tangentExist = true;
					if (it.first.find("BINORMAL") != std::string::npos)
						binormalExist = true;
					desc = &itDesc;
					switch (it.second.accessor.type) {
					case TINYGLTF_TYPE_VEC4:
						size = 16;
						break;
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
				uint32_t offset = i * (it.second.bufferView.byteStride == 0 ? size : it.second.bufferView.byteStride);
				memcpy(bytePos, data + it.second.bufferView.byteOffset + offset + it.second.accessor.byteOffset, size);
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
		mesh.VertexCount = accessor.count;

		//TODO: generate only if it has a normal map but no bitangent and tangent
		if (!tangentExist || !binormalExist)
		{
			//generate the tangents and binormals
			for (size_t i = 0; i < indices.size(); i += 3) {
				Vertex& v0 = vertices[indices[i]];
				Vertex& v1 = vertices[indices[i + 1]];
				Vertex& v2 = vertices[indices[i + 2]];

				Vector3 edge1 = v1.position - v0.position;
				Vector3 edge2 = v2.position - v0.position;

				Vector2 deltaUV1 = v1.texcoord - v0.texcoord;
				Vector2 deltaUV2 = v2.texcoord - v0.texcoord;

				float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

				Vector3 tangent;
				if (tangentExist)
					tangent = vertices[indices[i]].tangent;
				else {
					tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
					tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
					tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

					// Normalize and store tangent
					tangent.Normalize();
					v0.tangent = v1.tangent = v2.tangent = tangent;
				}

				// Compute bitangent
				if (!binormalExist)
				{
					Vector3 binormal = v0.normal.Cross(tangent) * ((deltaUV1.x * deltaUV2.y) - (deltaUV2.x * deltaUV1.y));
					v0.binormal = v1.binormal = v2.binormal = binormal;
				}
			}
		}
		
		
		//presume command list is open and will be closed and executed later
		nvrhi::BufferDesc vertexBufDesc;
		vertexBufDesc.byteSize = vertices.size() * sizeof(Vertex);	//the offset is already the size of the vb
		vertexBufDesc.isVertexBuffer = true;
		vertexBufDesc.debugName = itMesh.name + " Vertex Buffer";
		vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
		//smth smth syncrhonization need to be this state to be written

		mesh.VertexBufferHandle = Renderer->Device->m_NvrhiDevice->createBuffer(vertexBufDesc);
		//copy data over
		Renderer->CommandList->beginTrackingBufferState(mesh.VertexBufferHandle, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
		Renderer->CommandList->writeBuffer(mesh.VertexBufferHandle, vertices.data(), vertexBufDesc.byteSize);
		Renderer->CommandList->setPermanentBufferState(mesh.VertexBufferHandle, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

		nvrhi::BufferDesc indexBufDesc;
		indexBufDesc.byteSize = indices.size() * sizeof(uint32_t);
		indexBufDesc.isIndexBuffer = true;
		indexBufDesc.debugName = itMesh.name + " Index Buffer";
		indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

		mesh.IndexBufferHandle = Renderer->Device->m_NvrhiDevice->createBuffer(indexBufDesc);
		Renderer->CommandList->beginTrackingBufferState(mesh.IndexBufferHandle, nvrhi::ResourceStates::CopyDest);
		Renderer->CommandList->writeBuffer(mesh.IndexBufferHandle, indices.data(), indexBufDesc.byteSize);
		Renderer->CommandList->setPermanentBufferState(mesh.IndexBufferHandle, nvrhi::ResourceStates::IndexBuffer);

#if 0
		RD_CORE_INFO("Mesh: {}", itMesh.name);
		for(const auto& it : vertices)
		{
			RD_CORE_INFO("Pos: {}, Normal: {}, Texcoord: {}", it.position, it.normal, it.texcoord);
		}
		for(const auto& it : indices)
		{
			RD_CORE_INFO("Index: {}", it);
		}
#endif
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	//load the images
	for(const tinygltf::Image& itImg : model.images)
	{
		nvrhi::TextureDesc texDesc;
		texDesc.width = itImg.width;
		texDesc.height = itImg.height;
		texDesc.dimension = nvrhi::TextureDimension::Texture2D;
		switch(itImg.component)
		{
		case 1:
			texDesc.format = nvrhi::Format::R8_UNORM;
			break;
		case 2:
			texDesc.format = nvrhi::Format::RG8_UNORM;
			break;
		case 3:
			texDesc.format = nvrhi::Format::RGBA8_UNORM;
			break;
		case 4:
			texDesc.format = nvrhi::Format::RGBA8_UNORM;
			break;
		default:
			RD_ASSERT(true, "Unsupported texture channel count");
		}
		texDesc.debugName = itImg.uri;
		texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
		texDesc.isRenderTarget = false;
		texDesc.keepInitialState = true;

		Image img;
		img.TextureHandle = Renderer->Device->m_NvrhiDevice->createTexture(texDesc);
		RD_ASSERT(img.TextureHandle == nullptr, "Issue creating texture handle: {}", itImg.uri);

		//upload the texture data
		Renderer->CommandList->writeTexture(img.TextureHandle, 0, 0, itImg.image.data(), itImg.width * itImg.component);

		AssetManager::GetInstance()->Images.emplace_back(img);
	}
	for(const tinygltf::Texture& itTex : model.textures)
	{
		//textures contain a sampler and an image
		Texture tex;
		nvrhi::SamplerDesc samplerDesc;
		if (itTex.sampler < 0)
		{
			//no samplers so use a default one
		}
		else 
		{
			//create the sampler
			const tinygltf::Sampler gltfSampler = model.samplers[itTex.sampler];
			switch (gltfSampler.minFilter)
			{
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
			case -1:
				samplerDesc.minFilter = true;
				samplerDesc.mipFilter = true;
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				samplerDesc.minFilter = true;
				samplerDesc.mipFilter = false;
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
				samplerDesc.minFilter = true;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
				samplerDesc.minFilter = false;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				samplerDesc.minFilter = false;
				samplerDesc.mipFilter = true;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				samplerDesc.minFilter = false;
				samplerDesc.mipFilter = false;
				break;
			default:
				RD_ASSERT(true, "Unknown min filter");
			}
			switch (gltfSampler.magFilter)
			{
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
				samplerDesc.magFilter = true;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
				samplerDesc.magFilter = false;
				break;
			}
			switch (gltfSampler.wrapS)
			{
			case TINYGLTF_TEXTURE_WRAP_REPEAT:
				samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
				break;
			case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
				samplerDesc.addressU = nvrhi::SamplerAddressMode::ClampToEdge;
				break;
			case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
				samplerDesc.addressU = nvrhi::SamplerAddressMode::MirroredRepeat;
				break;
			}
			switch (gltfSampler.wrapT)
			{
			case TINYGLTF_TEXTURE_WRAP_REPEAT:
				samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
				break;
			case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
				samplerDesc.addressV = nvrhi::SamplerAddressMode::ClampToEdge;
				break;
			case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
				samplerDesc.addressV = nvrhi::SamplerAddressMode::MirroredRepeat;
				break;
			}
		}
		tex.SamplerHandle = Renderer->Device->m_NvrhiDevice->createSampler(samplerDesc);
		tex.ImageIndex = itTex.source;
		AssetManager::GetInstance()->Textures.emplace_back(tex);
	}
	Renderer->CommandList->close();
	Renderer->Device->m_NvrhiDevice->executeCommandList(Renderer->CommandList);

	//create all the entities and their components
	for (const int& rootIndex : model.scenes[0].nodes) {	//iterating through the root nodes
		TraverseNode(rootIndex, model, EntityManager);
	}
}
