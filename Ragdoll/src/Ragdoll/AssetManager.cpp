#include "ragdollpch.h"

#include "AssetManager.h"
#include "ForwardRenderer.h"
#include "DirectXDevice.h"

AssetManager* AssetManager::GetInstance()
{
	if(!s_Instance)
	{
		s_Instance = std::make_unique<AssetManager>();
	}
	return s_Instance.get();
}

int32_t AssetManager::AddImage(Image img)
{
	DeviceRef->m_NvrhiDevice->writeDescriptorTable(DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(Images.size(), img.TextureHandle));
	Images.emplace_back(img);
	return Images.size() - 1;
}

void AssetManager::Init(std::shared_ptr<DirectXDevice> deviceRef, std::shared_ptr<ragdoll::FileManager> fm)
{
	DeviceRef = deviceRef;
	FileManagerRef = fm;
	//set up the command list
	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();
	//setup the global buffer details
	//create the vertex layout and index used by the shader
	nvrhi::VertexAttributeDesc vPositionAttrib;
	vPositionAttrib.name = "POSITION";
	vPositionAttrib.offset = offsetof(Vertex, position);
	vPositionAttrib.elementStride = sizeof(Vertex);
	vPositionAttrib.format = nvrhi::Format::RGB32_FLOAT;
	nvrhi::VertexAttributeDesc vNormalAttrib;
	vNormalAttrib.name = "NORMAL";
	vNormalAttrib.offset = offsetof(Vertex, normal);
	vNormalAttrib.elementStride = sizeof(Vertex);
	vNormalAttrib.format = nvrhi::Format::RGB32_FLOAT;
	nvrhi::VertexAttributeDesc vTangentAttrib;
	vTangentAttrib.name = "TANGENT";
	vTangentAttrib.offset = offsetof(Vertex, tangent);
	vTangentAttrib.elementStride = sizeof(Vertex);
	vTangentAttrib.format = nvrhi::Format::RGB32_FLOAT;
	nvrhi::VertexAttributeDesc vTexcoordAttrib;
	vTexcoordAttrib.name = "TEXCOORD";
	vTexcoordAttrib.offset = offsetof(Vertex, texcoord);
	vTexcoordAttrib.elementStride = sizeof(Vertex);
	vTexcoordAttrib.format = nvrhi::Format::RG32_FLOAT;
	VertexAttributes = {
		vPositionAttrib,
		vNormalAttrib,
		vTangentAttrib,
		vTexcoordAttrib,
	};

	//create a default texture
	CommandList->open();
	nvrhi::TextureDesc textureDesc;
	textureDesc.width = 2;
	textureDesc.height = 2;
	textureDesc.format = nvrhi::Format::RGBA8_UNORM;
	textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
	textureDesc.isRenderTarget = false;
	textureDesc.isTypeless = false;
	textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
	textureDesc.keepInitialState = true;

	textureDesc.debugName = "Default Texture";
	AssetManager::GetInstance()->DefaultTex = DeviceRef->m_NvrhiDevice->createTexture(textureDesc);
	uint8_t color[4][4] = { 
		{255, 255, 255, 255},
		{255, 255, 255, 255},
		{255, 255, 255, 255},
		{255, 255, 255, 255},
	};
	CommandList->writeTexture(AssetManager::GetInstance()->DefaultTex, 0, 0, &color, 8);

	textureDesc.debugName = "Error Texture";
	AssetManager::GetInstance()->ErrorTex = DeviceRef->m_NvrhiDevice->createTexture(textureDesc);
	uint8_t errorColor[4][4] = {
		{255, 105, 180, 255},
		{0, 0, 0, 255},
		{255, 105, 180, 255},
		{0, 0, 0, 255},
	};
	CommandList->writeTexture(AssetManager::GetInstance()->ErrorTex, 0, 0, &errorColor, 8);

#pragma region Samplers Creation
	//create the samplers
	nvrhi::SamplerDesc samplerDesc;
	AssetManager::GetInstance()->Samplers.resize((int)SamplerTypes::COUNT);
	//create all the samplers used
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	Samplers[(int)SamplerTypes::Point_Clamp] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	Samplers[(int)SamplerTypes::Point_Wrap] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Point_Repeat] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	Samplers[(int)SamplerTypes::Linear_Clamp] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	Samplers[(int)SamplerTypes::Linear_Wrap] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Linear_Repeat] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	Samplers[(int)SamplerTypes::Trilinear_Clamp] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	Samplers[(int)SamplerTypes::Trilinear_Wrap] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Trilinear_Repeat] = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);

	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.borderColor = 1.f;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Border;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Border;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Border;
	samplerDesc.reductionType = nvrhi::SamplerReductionType::Comparison;
	ShadowSampler = DeviceRef->m_NvrhiDevice->createSampler(samplerDesc);
#pragma endregion

	//create the bindless descriptor table
	nvrhi::BindlessLayoutDesc bindlessDesc;

	bindlessDesc.visibility = nvrhi::ShaderType::Pixel;
	bindlessDesc.maxCapacity = 1024;
	bindlessDesc.firstSlot = 0;
	bindlessDesc.registerSpaces = {
		nvrhi::BindingLayoutItem::Texture_SRV(1)
	};

	BindlessLayoutHandle = DeviceRef->m_NvrhiDevice->createBindlessLayout(bindlessDesc);
	DescriptorTable = DeviceRef->m_NvrhiDevice->createDescriptorTable(BindlessLayoutHandle);
	//TODO: not sure why i am forced to resize
	DeviceRef->m_NvrhiDevice->resizeDescriptorTable(DescriptorTable, bindlessDesc.maxCapacity, true);

	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
}

uint32_t AssetManager::AddVertices(const std::vector<Vertex>& newVertices, const std::vector<uint32_t>& newIndices)
{
	uint32_t vCurrOffset = Vertices.size();
	uint32_t iCurrOffset = Indices.size();
	Vertices.resize(Vertices.size() + newVertices.size());
	Indices.resize(Indices.size() + newIndices.size());
	memcpy(Vertices.data() + vCurrOffset, newVertices.data(), newVertices.size() * sizeof(Vertex));
	memcpy(Indices.data() + iCurrOffset, newIndices.data(), newIndices.size() * sizeof(uint32_t));
	//create the vertexbuffer info
	VertexBufferInfo info;
	info.VBOffset = vCurrOffset;
	info.IBOffset = iCurrOffset;
	info.VerticesCount = newVertices.size();
	info.IndicesCount = newIndices.size();
	VertexBufferInfos.emplace_back(info);
	return VertexBufferInfos.size() - 1;
}

void AssetManager::UpdateVBOIBO()
{
	CommandList->open();
	CommandList->beginMarker("Update global buffer");

	nvrhi::BufferDesc vertexBufDesc;
	vertexBufDesc.byteSize = Vertices.size() * sizeof(Vertex);	//the offset is already the size of the vb
	vertexBufDesc.isVertexBuffer = true;
	vertexBufDesc.debugName = "Global vertex buffer";
	vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
	//smth smth syncrhonization need to be this state to be written

	VBO = DeviceRef->m_NvrhiDevice->createBuffer(vertexBufDesc);
	//copy data over
	CommandList->beginTrackingBufferState(VBO, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
	CommandList->writeBuffer(VBO, Vertices.data(), vertexBufDesc.byteSize);
	CommandList->setPermanentBufferState(VBO, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

	nvrhi::BufferDesc indexBufDesc;
	indexBufDesc.byteSize = Indices.size() * sizeof(uint32_t);
	indexBufDesc.isIndexBuffer = true;
	indexBufDesc.debugName = "Global index buffer";
	indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

	IBO = DeviceRef->m_NvrhiDevice->createBuffer(indexBufDesc);
	CommandList->beginTrackingBufferState(IBO, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(IBO, Indices.data(), indexBufDesc.byteSize);
	CommandList->setPermanentBufferState(IBO, nvrhi::ResourceStates::IndexBuffer);

	CommandList->endMarker();
	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
}

nvrhi::ShaderHandle AssetManager::GetShader(const std::string& shaderFilename)
{
	if (Shaders.contains(shaderFilename)) {
		return Shaders.at(shaderFilename);
	}
	//if no shader, try to load the shader
	nvrhi::ShaderType type;
	if (shaderFilename.find(".vs.") != std::string::npos) {
		type = nvrhi::ShaderType::Vertex;
	}
	else if (shaderFilename.find(".ps.") != std::string::npos) {
		type = nvrhi::ShaderType::Pixel;
	}
	uint32_t size{};
	const uint8_t* data = FileManagerRef->ImmediateLoad(shaderFilename, size);
	nvrhi::ShaderHandle shader = DeviceRef->m_NvrhiDevice->createShader(
		nvrhi::ShaderDesc(type),
		data, size);
	Shaders[shaderFilename] = shader;
	return Shaders.at(shaderFilename);
}