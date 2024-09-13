#include "ragdollpch.h"

#include "GLTFLoader.h"
#include "ForwardRenderer.h"
#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"

#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Scene.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include <tiny_gltf.h>

void GLTFLoader::Init(std::filesystem::path root, ForwardRenderer* renderer, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene)
{
	Root = root;
	Renderer = renderer;
	FileManager = fm;
	EntityManager = em;
	Scene = scene;
}

void AddToFurthestSibling(ragdoll::Guid child, ragdoll::Guid newChild, std::shared_ptr<ragdoll::EntityManager> em)
{
	TransformComp* trans = em->GetComponent<TransformComp>(child);
	if (trans->m_Sibling.m_RawId != 0) {
		AddToFurthestSibling(trans->m_Sibling, newChild, em);
	}
	else
		trans->m_Sibling = newChild;
}

ragdoll::Guid TraverseNode(int32_t currIndex, int32_t level, const tinygltf::Model& model, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene)
{
	const tinygltf::Node& curr = model.nodes[currIndex];
	//create the entity
	entt::entity ent = em->CreateEntity();
	ragdoll::Guid currId = em->GetGuid(ent);
	if (curr.mesh >= 0) {
		RenderableComp* renderableComp = em->AddComponent<RenderableComp>(ent);
		renderableComp->meshIndex = curr.mesh;
	}

	TransformComp* transComp = em->AddComponent<TransformComp>(ent);
	transComp->glTFId = currIndex;
	//root level entities
	if (level == 0)
	{
		scene->AddEntityAtRootLevel(currId);
	}
	if (curr.matrix.size() > 0) {
		const std::vector<double>& gltfMat = curr.matrix;
		Matrix mat {
			(float)gltfMat[0], (float)gltfMat[1], (float)gltfMat[2], (float)gltfMat[3], 
			(float)gltfMat[4], (float)gltfMat[5], (float)gltfMat[6], (float)gltfMat[7], 
			(float)gltfMat[8], (float)gltfMat[9], (float)gltfMat[10], (float)gltfMat[11],
			(float)gltfMat[12], (float)gltfMat[13], (float)gltfMat[14], (float)gltfMat[15]};
		mat.Decompose(transComp->m_LocalScale, transComp->m_LocalRotation, transComp->m_LocalPosition);
	}
	if (curr.translation.size() > 0)
		transComp->m_LocalPosition = { (float)curr.translation[0], (float)curr.translation[1], (float)curr.translation[2] };
	if (curr.rotation.size() > 0)
		transComp->m_LocalRotation = { (float)curr.rotation[0], (float)curr.rotation[1], (float)curr.rotation[2], (float)curr.rotation[3] };
	if (curr.scale.size() > 0)
		transComp->m_LocalScale = { (float)curr.scale[0], (float)curr.scale[1], (float)curr.scale[2] };

	transComp->m_Dirty = true;

	const tinygltf::Node& parent = model.nodes[currIndex];
	for (const int& childIndex : parent.children) {
		ragdoll::Guid childId = TraverseNode(childIndex, level + 1, model, em, scene);
		if (transComp->m_Child.m_RawId == 0) {
			transComp->m_Child = childId;
		}
		else {
			AddToFurthestSibling(transComp->m_Child, childId, em);
		}
	}

	return currId;
}

enum AttributeType {
	None,
	Position,
	Color,
	Normal,
	Tangent,
	Binormal,
	Texcoord,

	Count
};

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
		//should not create a new input layout handle, use the one provided by the renderer
		std::vector<Vertex> newBuffer;
		std::unordered_map<std::string, tinygltf::Accessor> attributeToAccessors;
		//for every mesh, create a new mesh object
		Mesh mesh;
		//keep track of what is the currently loaded buffer and data
		uint32_t binSize = 0;
		uint32_t vertexCount{};

		//load all the submeshes
		for (const tinygltf::Primitive& itPrim : itMesh.primitives) 
		{
			Submesh submesh;
			VertexBufferObject buffer;
			std::vector<uint32_t> indices;
			std::vector<Vertex> vertices;

			//set the material index for the primitive
			submesh.MaterialIndex = itPrim.material;

			//load the indices first
			{
				const tinygltf::Accessor& accessor = model.accessors[itPrim.indices];
				const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
				indices.resize(accessor.count);
				uint8_t* data = model.buffers[bufferView.buffer].data.data();
				uint32_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
				//manually assign to reconcil things like short to uint
				for (int i = 0; i < accessor.count; ++i) {
					switch (accessor.componentType) {
					case TINYGLTF_COMPONENT_TYPE_SHORT:
						indices[i] = *reinterpret_cast<int16_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						indices[i] = *reinterpret_cast<uint16_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						indices[i] = *reinterpret_cast<uint32_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
						break;
					default:
						RD_ASSERT(true, "Unsupport index type for {}", itMesh.name);
					}
					if(accessor.maxValues.size() != 0)
						RD_ASSERT(indices[i] > accessor.maxValues[0], "");
				}
#if 0
				RD_CORE_TRACE("Loaded {} indices at byte offest {}", accessor.count, accessor.byteOffset);
				RD_CORE_TRACE("Largest index is {}", *std::max_element(indices.begin(), indices.end()));
#endif
				buffer.TriangleCount = accessor.count;
			}

			//add the relevant data into the map to use
			{
				for (const auto& itAttrib : itPrim.attributes) {
					tinygltf::Accessor vertexAccessor = model.accessors[itAttrib.second];
					tinygltf::BufferView bufferView = model.bufferViews[vertexAccessor.bufferView];
					attributeToAccessors[itAttrib.first] = vertexAccessor;
					vertexCount = vertexAccessor.count;
				}
				RD_ASSERT(vertexCount == 0, "There are no vertices?");
			}

			//for every attribute, check if there is one that corresponds with renderer attributes
			bool tangentExist = false, binormalExist = false;
			{
				vertices.resize(vertexCount);
				for (const auto& it : attributeToAccessors) {
					nvrhi::VertexAttributeDesc const* desc = nullptr;
					const tinygltf::Accessor& accessor = it.second;
					const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
					AttributeType type;
					for (const nvrhi::VertexAttributeDesc& itDesc : Renderer->VertexAttributes) {
						if (it.first.find(itDesc.name) != std::string::npos)
						{
							if (itDesc.name == "POSITION")
								type = AttributeType::Position;
							else if (itDesc.name == "COLOR")
								type = AttributeType::Color;
							else if (itDesc.name == "NORMAL")
								type = AttributeType::Normal;
							else if (itDesc.name == "TANGENT") {
								type = AttributeType::Tangent;
								tangentExist = true;
							}
							else if (itDesc.name == "BINORMAL") {
								type = AttributeType::Binormal;
								binormalExist = true;
							}
							else if (itDesc.name == "TEXCOORD")
								type = AttributeType::Texcoord;
							desc = &itDesc;
						}
					}
					RD_ASSERT(desc == nullptr, "Loaded mesh contains a attribute not supported by the renderer: {}", it.first);
					uint32_t size = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
					//memcpy the data from the buffer over into the vertices
					uint8_t* data = model.buffers[bufferView.buffer].data.data();
					for (int i = 0; i < vertexCount; ++i) {
						Vertex& v = vertices[i];
						uint8_t* bytePos = reinterpret_cast<uint8_t*>(&v) + desc->offset;
						uint32_t byteOffset = accessor.byteOffset + bufferView.byteOffset;
						uint32_t stride = bufferView.byteStride == 0 ? size : bufferView.byteStride;
						memcpy(bytePos, data + byteOffset + i * stride, size);
					}
#if 0
					RD_CORE_INFO("Loaded {} vertices of attribute: {}", vertexCount, desc->name);
#endif
				}
			}

			//if tangent or binormal does not exist, generate them
			{
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
			}

			//create all the vertex and index buffers
			//presume command list is open and will be closed and executed later
			{
				nvrhi::BufferDesc vertexBufDesc;
				vertexBufDesc.byteSize = vertices.size() * sizeof(Vertex);	//the offset is already the size of the vb
				vertexBufDesc.isVertexBuffer = true;
				vertexBufDesc.debugName = itMesh.name + " Vertex Buffer";
				vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
				//smth smth syncrhonization need to be this state to be written

				buffer.VertexBufferHandle = Renderer->Device->m_NvrhiDevice->createBuffer(vertexBufDesc);
				//copy data over
				Renderer->CommandList->beginTrackingBufferState(buffer.VertexBufferHandle, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
				Renderer->CommandList->writeBuffer(buffer.VertexBufferHandle, vertices.data(), vertexBufDesc.byteSize);
				Renderer->CommandList->setPermanentBufferState(buffer.VertexBufferHandle, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

				nvrhi::BufferDesc indexBufDesc;
				indexBufDesc.byteSize = indices.size() * sizeof(uint32_t);
				indexBufDesc.isIndexBuffer = true;
				indexBufDesc.debugName = itMesh.name + " Index Buffer";
				indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

				buffer.IndexBufferHandle = Renderer->Device->m_NvrhiDevice->createBuffer(indexBufDesc);
				Renderer->CommandList->beginTrackingBufferState(buffer.IndexBufferHandle, nvrhi::ResourceStates::CopyDest);
				Renderer->CommandList->writeBuffer(buffer.IndexBufferHandle, indices.data(), indexBufDesc.byteSize);
				Renderer->CommandList->setPermanentBufferState(buffer.IndexBufferHandle, nvrhi::ResourceStates::IndexBuffer);

				submesh.VertexBufferIndex = AssetManager::GetInstance()->VBOs.size();
				mesh.Submeshes.emplace_back(submesh);
				AssetManager::GetInstance()->VBOs.emplace_back(buffer);
			}
		}
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
		SamplerTypes type;
		if (itTex.sampler < 0)
		{
			//no samplers so use a default one
		}
		else 
		{
			//create the sampler
			const tinygltf::Sampler gltfSampler = model.samplers[itTex.sampler];
			switch (gltfSampler.wrapS)
			{
			case TINYGLTF_TEXTURE_WRAP_REPEAT:
				switch (gltfSampler.minFilter)
				{
				case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				case TINYGLTF_TEXTURE_FILTER_LINEAR:
					if (gltfSampler.magFilter == 1)
						type = SamplerTypes::Trilinear_Repeat;
					else
						type = SamplerTypes::Linear_Repeat;
					break;
				case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				case TINYGLTF_TEXTURE_FILTER_NEAREST:
					type = SamplerTypes::Point_Repeat;
					break;
				case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				case -1:
					RD_CORE_WARN("Sampler do not exist, giving trilinear");
					type = SamplerTypes::Trilinear_Repeat;
					break;
				default:
					RD_ASSERT(true, "Unknown min filter");
				}
				break;
			case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
				switch (gltfSampler.minFilter)
				{
				case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				case TINYGLTF_TEXTURE_FILTER_LINEAR:
					if (gltfSampler.magFilter == 1)
						type = SamplerTypes::Trilinear_Clamp;
					else
						type = SamplerTypes::Linear_Clamp;
					break;
				case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				case TINYGLTF_TEXTURE_FILTER_NEAREST:
					type = SamplerTypes::Point_Clamp;
					break;
				case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				case -1:
					RD_CORE_WARN("Sampler do not exist, giving trilinear");
					type = SamplerTypes::Trilinear_Clamp;
					break;
				default:
					RD_ASSERT(true, "Unknown min filter");
				}
				break;
			case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
				switch (gltfSampler.minFilter)
				{
				case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				case TINYGLTF_TEXTURE_FILTER_LINEAR:
					if (gltfSampler.magFilter == 1)
						type = SamplerTypes::Trilinear_Repeat;
					else
						type = SamplerTypes::Linear_Repeat;
					break;
				case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				case TINYGLTF_TEXTURE_FILTER_NEAREST:
					type = SamplerTypes::Point_Repeat;
					break;
				case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				case -1:
					RD_CORE_WARN("Sampler do not exist, giving trilinear");
					type = SamplerTypes::Trilinear_Repeat;
					break;
				default:
					RD_ASSERT(true, "Unknown min filter");
				}
				break;
			}
		}
		tex.SamplerIndex = (int)type;
		tex.ImageIndex = itTex.source;
		AssetManager::GetInstance()->Textures.emplace_back(tex);

		TINYGLTF_MODE_POINTS;
	}
	Renderer->CommandList->close();
	Renderer->Device->m_NvrhiDevice->executeCommandList(Renderer->CommandList);

	//load all of the materials
	for (const tinygltf::Material& gltfMat : model.materials) 
	{
		Material mat;
		mat.Metallic = gltfMat.pbrMetallicRoughness.metallicFactor;
		mat.Roughness = gltfMat.pbrMetallicRoughness.roughnessFactor;
		mat.Color = Vector4(
			gltfMat.pbrMetallicRoughness.baseColorFactor[0],
			gltfMat.pbrMetallicRoughness.baseColorFactor[1],
			gltfMat.pbrMetallicRoughness.baseColorFactor[2],
			gltfMat.pbrMetallicRoughness.baseColorFactor[3]);
		mat.AlbedoIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
		mat.MetallicRoughnessIndex = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index;
		mat.NormalIndex = gltfMat.normalTexture.index;
		mat.bIsLit = true;
		AssetManager::GetInstance()->Materials.emplace_back(mat);
	}

	//create all the entities and their components
	for (const int& rootIndex : model.scenes[0].nodes) {	//iterating through the root nodes
		TraverseNode(rootIndex, 0, model, EntityManager, Scene);
	}
#if 0
	TransformLayer->DebugPrintHierarchy();
#endif
}
