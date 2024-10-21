#include "ragdollpch.h"
#include "GLTFLoader.h"
#include <microprofile.h>

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

void GLTFLoader::Init(std::filesystem::path root, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene)
{
	Root = root;
	FileManagerRef = fm;
	EntityManagerRef = em;
	SceneRef = scene;
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

ragdoll::Guid TraverseNode(int32_t currIndex, int32_t level, uint32_t meshIndicesOffset, const tinygltf::Model& model, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene, Vector3& min, Vector3& max)
{
	static std::stack<Matrix> modelStack;
	const tinygltf::Node& curr = model.nodes[currIndex];
	//create the entity
	entt::entity ent = em->CreateEntity();
	ragdoll::Guid currId = em->GetGuid(ent);
	if (curr.mesh >= 0) {
		RenderableComp* renderableComp = em->AddComponent<RenderableComp>(ent);
		renderableComp->meshIndex = curr.mesh + meshIndicesOffset;
	}

	TransformComp* transComp = em->AddComponent<TransformComp>(ent);
	transComp->glTFId = currIndex;
	//root level entities
	if (level == 0)
	{
		scene->AddEntityAtRootLevel(currId);
	}

	Matrix mat;
	if (curr.matrix.size() > 0) {
		const std::vector<double>& gltfMat = curr.matrix;
		mat = {
			(float)gltfMat[0], (float)gltfMat[1], (float)gltfMat[2], (float)gltfMat[3], 
			(float)gltfMat[4], (float)gltfMat[5], (float)gltfMat[6], (float)gltfMat[7], 
			(float)gltfMat[8], (float)gltfMat[9], (float)gltfMat[10], (float)gltfMat[11],
			(float)gltfMat[12], (float)gltfMat[13], (float)gltfMat[14], (float)gltfMat[15]};
		mat.Decompose(transComp->m_LocalScale, transComp->m_LocalRotation, transComp->m_LocalPosition);
	}
	else {
		if (curr.translation.size() > 0)
			transComp->m_LocalPosition = { (float)curr.translation[0], (float)curr.translation[1], (float)curr.translation[2] };
		if (curr.rotation.size() > 0)
			transComp->m_LocalRotation = { (float)curr.rotation[0], (float)curr.rotation[1], (float)curr.rotation[2], (float)curr.rotation[3] };
		if (curr.scale.size() > 0)
			transComp->m_LocalScale = { (float)curr.scale[0], (float)curr.scale[1], (float)curr.scale[2] };
		mat = Matrix::CreateScale(transComp->m_LocalScale) * Matrix::CreateFromQuaternion(transComp->m_LocalRotation) * Matrix::CreateTranslation(transComp->m_LocalPosition);
	}
	//since i need the modelstack already i no need transform system to update
	//transComp->m_Dirty = true;
	if (!modelStack.empty())
		transComp->m_PrevModelToWorld = transComp->m_ModelToWorld = modelStack.top() * mat;
	else
		transComp->m_PrevModelToWorld = transComp->m_ModelToWorld = mat;
	modelStack.push(mat);

	//get max extents in world space
	if (curr.mesh >= 0)
	{
		for (const Submesh& submesh : AssetManager::GetInstance()->Meshes[curr.mesh + meshIndicesOffset].Submeshes)
		{
			DirectX::BoundingBox box = AssetManager::GetInstance()->VertexBufferInfos[submesh.VertexBufferIndex].BestFitBox;
			box.Transform(box, transComp->m_ModelToWorld);
			Vector3 corners[8];
			box.GetCorners(corners);
			for (int i = 0; i < 8; ++i) {
				min.x = std::min({ corners[i].x, min.x });
				min.y = std::min({ corners[i].y, min.y });
				min.z = std::min({ corners[i].z, min.z });
				max.x = std::max({ corners[i].x, max.x });
				max.y = std::max({ corners[i].y, max.y });
				max.z = std::max({ corners[i].z, max.z });
			}
		}
	}

	const tinygltf::Node& parent = model.nodes[currIndex];
	for (const int& childIndex : parent.children) {
		ragdoll::Guid childId = TraverseNode(childIndex, level + 1, meshIndicesOffset, model, em, scene, min, max);
		if (transComp->m_Child.m_RawId == 0) {
			transComp->m_Child = childId;
		}
		else {
			AddToFurthestSibling(transComp->m_Child, childId, em);
		}
	}

	modelStack.pop();

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
	MICROPROFILE_SCOPEI("Load", "Load GLTF", MP_CYAN);
	//ownself open command list
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err, warn;
	std::filesystem::path path = Root / fileName;
	std::filesystem::path modelRoot = path.parent_path().lexically_relative(Root);
	{
		MICROPROFILE_SCOPEI("Load", "Load GLTF File", MP_DARKCYAN);
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
		RD_ASSERT(ret == false, "Issue loading {}", path.string());
	}

	uint32_t meshIndicesOffset = AssetManager::GetInstance()->Meshes.size();
	{
		MICROPROFILE_SCOPEI("Load", "Load Meshes", MP_DARKCYAN);
		// go downwards from meshes
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
			uint32_t materialIndicesOffset = AssetManager::GetInstance()->Materials.size();
			for (const tinygltf::Primitive& itPrim : itMesh.primitives)
			{
				MICROPROFILE_SCOPEI("Render", "Mesh", MP_LIGHTCYAN);
				Submesh submesh{};
				DirectX::BoundingBox box;
				VertexBufferInfo buffer;
				IndexStagingBuffer.clear();
				VertexStagingBuffer.clear();

				//set the material index for the primitive
				submesh.MaterialIndex = itPrim.material + materialIndicesOffset;

				//load the indices first
				{
					const tinygltf::Accessor& accessor = model.accessors[itPrim.indices];
					const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
					IndexStagingBuffer.resize(accessor.count);
					uint8_t* data = model.buffers[bufferView.buffer].data.data();
					uint32_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
					//manually assign to reconcil things like short to uint
					for (int i = 0; i < accessor.count; ++i) {
						switch (accessor.componentType) {
						case TINYGLTF_COMPONENT_TYPE_SHORT:
							IndexStagingBuffer[i] = *reinterpret_cast<int16_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							IndexStagingBuffer[i] = *reinterpret_cast<uint16_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
							IndexStagingBuffer[i] = *reinterpret_cast<uint32_t*>(data + byteOffset + i * tinygltf::GetComponentSizeInBytes(accessor.componentType));
							break;
						default:
							RD_ASSERT(true, "Unsupport index type for {}", itMesh.name);
						}
						if (accessor.maxValues.size() != 0)
							RD_ASSERT(IndexStagingBuffer[i] > accessor.maxValues[0], "");
					}
#if 0
					RD_CORE_TRACE("Loaded {} indices at byte offest {}", accessor.count, accessor.byteOffset);
					RD_CORE_TRACE("Largest index is {}", *std::max_element(indices.begin(), indices.end()));
#endif
					buffer.IndicesCount = accessor.count;
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
					VertexStagingBuffer.resize(vertexCount);
					for (const auto& it : attributeToAccessors) {
						nvrhi::VertexAttributeDesc const* desc = nullptr;
						const tinygltf::Accessor& accessor = it.second;
						const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
						AttributeType type;
						for (const nvrhi::VertexAttributeDesc& itDesc : AssetManager::GetInstance()->InstancedVertexAttributes) {
							if (it.first.find(itDesc.name) != std::string::npos)
							{
								if (itDesc.name == "POSITION")
									type = AttributeType::Position;
								else if (itDesc.name == "COLOR")
									continue;	//skip vertex color
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
							Vertex& v = VertexStagingBuffer[i];
							uint8_t* bytePos = reinterpret_cast<uint8_t*>(&v) + desc->offset;
							uint32_t byteOffset = accessor.byteOffset + bufferView.byteOffset;
							uint32_t stride = bufferView.byteStride == 0 ? size : bufferView.byteStride;
							memcpy(bytePos, data + byteOffset + i * stride, size);
						}

						if (type == AttributeType::Position)
						{
							if (accessor.maxValues.size() == 3 && accessor.minValues.size() == 3)
							{
								Vector3 max{ (float)accessor.maxValues[0], (float)accessor.maxValues[1], (float)accessor.maxValues[2] };
								Vector3 min{ (float)accessor.minValues[0], (float)accessor.minValues[1], (float)accessor.minValues[2] };
								DirectX::BoundingBox::CreateFromPoints(box, min, max);
							}
							else
							{
								Vector3 min, max;
								min = max = VertexStagingBuffer[0].position;
								for (const Vertex& v : VertexStagingBuffer) {
									min.x = std::min(v.position.x, min.x); max.x = std::max(v.position.x, max.x);
									min.y = std::min(v.position.y, min.y); max.y = std::max(v.position.y, max.y);
									min.z = std::min(v.position.z, min.z); max.z = std::max(v.position.z, max.z);
								}
								DirectX::BoundingBox::CreateFromPoints(box, min, max);
							}
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
						for (size_t i = 0; i < IndexStagingBuffer.size(); i += 3) {
							Vertex& v0 = VertexStagingBuffer[IndexStagingBuffer[i]];
							Vertex& v1 = VertexStagingBuffer[IndexStagingBuffer[i + 1]];
							Vertex& v2 = VertexStagingBuffer[IndexStagingBuffer[i + 2]];

							Vector3 edge1 = v1.position - v0.position;
							Vector3 edge2 = v2.position - v0.position;

							Vector2 deltaUV1 = v1.texcoord - v0.texcoord;
							Vector2 deltaUV2 = v2.texcoord - v0.texcoord;

							float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

							Vector3 tangent;
							if (tangentExist)
								tangent = VertexStagingBuffer[IndexStagingBuffer[i]].tangent;
							else {
								tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
								tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
								tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

								// Normalize and store tangent
								tangent.Normalize();
								v0.tangent = v1.tangent = v2.tangent = tangent;
							}
						}
					}
				}

				//add to the asset manager vertices
				{
					submesh.VertexBufferIndex = AssetManager::GetInstance()->AddVertices(VertexStagingBuffer, IndexStagingBuffer);
					AssetManager::GetInstance()->VertexBufferInfos[submesh.VertexBufferIndex].BestFitBox = box;
					mesh.Submeshes.emplace_back(submesh);
				}
			}
#if 0
			RD_CORE_INFO("Mesh: {}", itMesh.name);
			for (const auto& it : vertices)
			{
				RD_CORE_INFO("Pos: {}, Normal: {}, Texcoord: {}", it.position, it.normal, it.texcoord);
			}
			for (const auto& it : indices)
			{
				RD_CORE_INFO("Index: {}", it);
			}
#endif
			AssetManager::GetInstance()->Meshes.emplace_back(mesh);
		}
	}
	//create the buffers
	{
		AssetManager::GetInstance()->UpdateVBOIBO();
	}

	uint32_t textureIndicesOffset;
	{
		CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
		CommandList->open();
		MICROPROFILE_SCOPEI("Load", "Load Textures", MP_DARKCYAN);
		MICROPROFILE_GPU_SET_CONTEXT(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer, MicroProfileGetGlobalGpuThreadLog());
		{
			MICROPROFILE_SCOPEGPUI("Load Textures", MP_LIGHTYELLOW1);
			//load the images
			std::unordered_map<int32_t, int32_t> gltfSourceToImageIndex{};
			for (int i = 0; i < model.images.size(); ++i)
			{
				const tinygltf::Image& itImg = model.images[i];
				nvrhi::TextureDesc texDesc;
				texDesc.width = itImg.width;
				texDesc.height = itImg.height;
				texDesc.dimension = nvrhi::TextureDimension::Texture2D;
				switch (itImg.component)
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
				img.TextureHandle = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
				RD_ASSERT(img.TextureHandle == nullptr, "Issue creating texture handle: {}", itImg.uri);

				//upload the texture data
				CommandList->writeTexture(img.TextureHandle, 0, 0, itImg.image.data(), itImg.width * itImg.component);
				//write to descriptor table
				int32_t index = AssetManager::GetInstance()->AddImage(img);
				gltfSourceToImageIndex[i] = index;
			}
			textureIndicesOffset = AssetManager::GetInstance()->Textures.size();
			for (const tinygltf::Texture& itTex : model.textures)
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
				tex.ImageIndex = gltfSourceToImageIndex.at(itTex.source);
				AssetManager::GetInstance()->Textures.emplace_back(tex);

				TINYGLTF_MODE_POINTS;
			}
		}
		CommandList->close();
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);

		MicroProfileFlip(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer);
	}

	{
		MICROPROFILE_SCOPEI("Load", "Load Textures", MP_CYAN);
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
			//get the textures
			if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0)
				mat.AlbedoTextureIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index + textureIndicesOffset;
			if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
				mat.RoughnessMetallicTextureIndex = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index + textureIndicesOffset;
			if (gltfMat.normalTexture.index >= 0)
				mat.NormalTextureIndex = gltfMat.normalTexture.index + textureIndicesOffset;
			mat.bIsLit = true;
			AssetManager::GetInstance()->Materials.emplace_back(mat);
		}
	}
	
	{
		MICROPROFILE_SCOPEI("Load", "Get Scene Extents", MP_CYAN);
		Vector3 min{ FLT_MAX, FLT_MAX, FLT_MAX }, max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		//create all the entities and their components
		for (const int& rootIndex : model.scenes[0].nodes) {	//iterating through the root nodes
			TraverseNode(rootIndex, 0, meshIndicesOffset, model, EntityManagerRef, SceneRef, min, max);
		}
		float offset = 1.01f;
		Octree::Max = max * offset;
		Octree::Min = min * offset;
		SceneRef->StaticOctree.Clear();
#if 0
		TransformLayer->DebugPrintHierarchy();
#endif
	}
}
