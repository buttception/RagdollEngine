#include "ragdollpch.h"
#include "GLTFLoader.h"

#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"

#include "Executor.h"
#include "Profiler.h"

#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Scene.h"

// Define these only in *one* .cc file.
#define TINYGLTF_IMPLEMENTATION
#define MULTITHREAD_LOAD 1
#if MULTITHREAD_LOAD == 1
#define TINYGLTF_NO_EXTERNAL_IMAGE
#endif
#define TINYGLTF_USE_CPP14
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

ragdoll::Guid TraverseNode(int32_t currIndex, int32_t level, uint32_t meshIndicesOffset, const tinygltf::Model& model, std::shared_ptr<ragdoll::EntityManager> em, std::shared_ptr<ragdoll::Scene> scene)
{
	static std::stack<Matrix> modelStack;
	const tinygltf::Node& curr = model.nodes[currIndex];
	//create the entity
	entt::entity ent = em->CreateEntity();
	ragdoll::Guid currId = em->GetGuid(ent);

	TransformComp* transComp = em->AddComponent<TransformComp>(ent);
	transComp->glTFId = currIndex;
	transComp->m_Dirty = true;
	//root level entities
	if (level == 0)
	{
		scene->AddEntityAtRootLevel(currId);
	}

	if (curr.matrix.size() > 0) {
		const std::vector<double>& gltfMat = curr.matrix;
		Matrix mat = {
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
	}

	if (curr.mesh >= 0) {
		RenderableComp* renderableComp = em->AddComponent<RenderableComp>(ent);
		renderableComp->meshIndex = curr.mesh + meshIndicesOffset;
	}
	else
	{
		//if there is no mesh, there is a chance it is camera
		for (ragdoll::SceneCamera& SceneCamera : scene->SceneInfo.Cameras)
		{
			if (SceneCamera.Name == curr.name)
			{
				SceneCamera.Position = transComp->m_LocalPosition;
				SceneCamera.Rotation = transComp->m_LocalRotation.ToEuler();
				SceneCamera.Rotation.y += DirectX::XM_PI;
				SceneCamera.Rotation.x = -SceneCamera.Rotation.x;
				break;
			}
		}
	}

	const tinygltf::Node& parent = model.nodes[currIndex];
	for (const int& childIndex : parent.children) {
		ragdoll::Guid childId = TraverseNode(childIndex, level + 1, meshIndicesOffset, model, em, scene);
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
	RD_SCOPE(Load, Load GLTF);
	//ownself open command list
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err, warn;
	std::filesystem::path path = Root / fileName;
	std::filesystem::path modelRoot = path.parent_path().lexically_relative(Root);
	{
		RD_SCOPE(Load, Load GLTF File);
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
		RD_ASSERT(ret == false, "Issue loading {}", path.string());
	}

	{
		//need to add cameras first with name, so when i load the node it can be assigned
		RD_SCOPE(Load, Load Cameras);
		SceneRef->SceneInfo.Cameras.clear();
		for (const tinygltf::Camera& itCam : model.cameras)
		{
			SceneRef->SceneInfo.Cameras.push_back({ itCam.name });
		}
	}

	uint32_t meshIndicesOffset = static_cast<uint32_t>(AssetManager::GetInstance()->Meshes.size());
	//load meshes
	{
		RD_SCOPE(Load, Load Meshes);
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
			size_t materialIndicesOffset = AssetManager::GetInstance()->Materials.size();
			for (const tinygltf::Primitive& itPrim : itMesh.primitives)
			{
				RD_SCOPE(Load, Mesh);
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
					size_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
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
					buffer.IndicesCount = (uint32_t)accessor.count;
				}

				//add the relevant data into the map to use
				{
					for (const auto& itAttrib : itPrim.attributes) {
						tinygltf::Accessor vertexAccessor = model.accessors[itAttrib.second];
						tinygltf::BufferView bufferView = model.bufferViews[vertexAccessor.bufferView];
						attributeToAccessors[itAttrib.first] = vertexAccessor;
						vertexCount = (uint32_t)vertexAccessor.count;
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
						for (uint32_t i = 0; i < vertexCount; ++i) {
							Vertex& v = VertexStagingBuffer[i];
							uint8_t* bytePos = reinterpret_cast<uint8_t*>(&v) + desc->offset;
							size_t byteOffset = accessor.byteOffset + bufferView.byteOffset;
							size_t stride = bufferView.byteStride == 0 ? size : bufferView.byteStride;
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
	uint32_t textureIndicesOffset = static_cast<uint32_t>(AssetManager::GetInstance()->Textures.size());
	uint32_t imageIndicesOffset = static_cast<uint32_t>(AssetManager::GetInstance()->Images.size());

	//load materials
	{
		RD_SCOPE(Load, Load Materials);
		//load all of the materials
		for (const tinygltf::Material& gltfMat : model.materials)
		{
			Material mat;
			mat.Metallic = (float)gltfMat.pbrMetallicRoughness.metallicFactor;
			mat.Roughness = (float)gltfMat.pbrMetallicRoughness.roughnessFactor;
			mat.Color = Vector4(
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[0]),
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[1]),
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[2]),
				static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[3]));
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

	//load textures
	{
		RD_SCOPE(Load, Load Textures);
		{
#if MULTITHREAD_LOAD == 1
			//load the images but have 1 thread per image
			AssetManager::GetInstance()->Images.resize(model.images.size() + imageIndicesOffset);
			std::vector<nvrhi::ICommandList*> cmdLists(model.images.size());
			std::vector<nvrhi::CommandListHandle> cmdListHandles(model.images.size());
			tf::Taskflow TaskFlow;
			for (int i = 0; i < model.images.size(); ++i)
			{
				cmdLists[i] = cmdListHandles[i] = DirectXDevice::GetNativeDevice()->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
				nvrhi::CommandListHandle hdl = cmdListHandles[i];

				Image* img = &AssetManager::GetInstance()->Images[i + imageIndicesOffset];
				tinygltf::Image* itImg = &model.images[i];
				//push the loading into the taskflow
				TaskFlow.emplace(
					[itImg, path, img, imageIndicesOffset, i, hdl]()
					{
						{
							RD_SCOPE(Load, STB Load);
							std::filesystem::path modelPath = path.parent_path() / itImg->uri;
							//load raw bytes, do not use stbi load
							std::ifstream file(modelPath, std::ios::binary | std::ios::ate);
							RD_ASSERT(!file, "Unable to open file {}:{}", strerror(errno), modelPath.string());
							int32_t size = static_cast<int32_t>(file.tellg());
							file.seekg(0, std::ios::beg);
							std::vector<uint8_t> data(size);
							RD_ASSERT(!file.read((char*)data.data(), size), "Failed to read file {}", itImg->uri);
							//use stbi_info_from_memory to check header on info for how to load
							int w = -1, h = -1, comp = -1, req_comp = 0;
							RD_ASSERT(!stbi_info_from_memory(data.data(), size, &w, &h, &comp), "stb unable to read image {}", itImg->uri);
							int bits = 8;
							int pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
							if (comp == 3)
								req_comp = 4;
							//dont support hdr images
							//use stbi_load_from_memory to load the image data, set up all the desc with that info
							uint8_t* raw = stbi_load_from_memory(data.data(), size, &w, &h, &comp, req_comp);
							RD_ASSERT(!raw, "Issue loading {}", itImg->uri);
							itImg->width = w;
							itImg->height = h;
							itImg->component = comp;
							itImg->bits = bits;
							itImg->pixel_type = pixel_type;
							//do not free the image here, free it when creating textures in gpu
							img->RawData = raw;
						}

						{
							RD_SCOPE(Load, Create Texture);
							nvrhi::TextureDesc texDesc;
							texDesc.width = itImg->width;
							texDesc.height = itImg->height;
							texDesc.dimension = nvrhi::TextureDimension::Texture2D;
							switch (itImg->component)
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
							texDesc.debugName = itImg->uri;
							texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
							texDesc.isRenderTarget = false;
							texDesc.keepInitialState = true;

							img->TextureHandle = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
							RD_ASSERT(img->TextureHandle == nullptr, "Issue creating texture handle: {}", itImg->uri);

							DirectXDevice::GetInstance()->m_NvrhiDevice->writeDescriptorTable(AssetManager::GetInstance()->DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(i + imageIndicesOffset, img->TextureHandle));
						}
						
						{
							RD_SCOPE(Load, Write Texture);
							hdl->open();
							//upload the texture data
							hdl->writeTexture(img->TextureHandle, 0, 0, img->RawData, itImg->width* (itImg->component == 3 ? 4 : itImg->component));
							hdl->close();
						}
					}
				);
			}
			//execute all the task
			SExecutor::Executor.run(TaskFlow).wait();

			DirectXDevice::GetNativeDevice()->executeCommandLists(cmdLists.data(), cmdLists.size());

			//free all raw bytes
			for (auto& it : AssetManager::GetInstance()->Images) {
				stbi_image_free(it.RawData);
				it.RawData = nullptr;
			}
#else
			CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
			CommandList->open();
			MICROPROFILE_GPU_SET_CONTEXT(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer, MicroProfileGetGlobalGpuThreadLog());
			{
				//MICROPROFILE_SCOPEGPUI("Create Textures", MP_AUTO);
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
			}
			CommandList->close();
			DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
#endif
		}
	}

	{
		RD_SCOPE(Load, Attaching Samplers);
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
			tex.ImageIndex = itTex.source + imageIndicesOffset;
			AssetManager::GetInstance()->Textures.emplace_back(tex);
		}
	}
	
	{
		RD_SCOPE(Load, Creating Hierarchy);
		//create all the entities and their components
		for (const int& rootIndex : model.scenes[0].nodes) {	//iterating through the all nodes
			TraverseNode(rootIndex, 0, meshIndicesOffset, model, EntityManagerRef, SceneRef);
		}
		SceneRef->UpdateTransforms();
		{
			//iterate through comps and set prev matrix as curr matrix
			auto EcsView = EntityManagerRef->GetRegistry().view<TransformComp>();
			for (const entt::entity& ent : EcsView) {
				TransformComp* comp = EntityManagerRef->GetComponent<TransformComp>(ent);
				comp->m_PrevModelToWorld = comp->m_ModelToWorld;
			}
		}
		{
			Vector3 Min{ FLT_MAX, FLT_MAX, FLT_MAX };
			Vector3 Max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
			//get the min and max extends
			auto EcsView = EntityManagerRef->GetRegistry().view<TransformComp, RenderableComp>();
			for (const entt::entity& ent : EcsView)
			{
				const RenderableComp* rComp = EntityManagerRef->GetComponent<RenderableComp>(ent);
				const Mesh& mesh = AssetManager::GetInstance()->Meshes[rComp->meshIndex];
				const TransformComp* tComp = EntityManagerRef->GetComponent<TransformComp>(ent);
				for (const Submesh& submesh : mesh.Submeshes)
				{
					const VertexBufferInfo& Info = AssetManager::GetInstance()->VertexBufferInfos[submesh.VertexBufferIndex];
					DirectX::BoundingBox Box = Info.BestFitBox;
					Box.Transform(Box, tComp->m_ModelToWorld);
					Vector3 Corners[8];
					Box.GetCorners(Corners);
					for (const Vector3& corner : Corners)
					{
						Min = DirectX::XMVectorMin(Min, corner);
						Max = DirectX::XMVectorMax(Max, corner);
					}
				}
			}
			DirectX::BoundingBox::CreateFromPoints(SceneRef->SceneInfo.SceneBounds, Min, Max);
		}
#if 0
		TransformLayer->DebugPrintHierarchy();
#endif
	}
}
