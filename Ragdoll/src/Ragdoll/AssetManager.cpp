#include "ragdollpch.h"

#include "nvrhi/utils.h"
#include "Profiler.h"

#include "AssetManager.h"
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
	DirectXDevice::GetInstance()->m_NvrhiDevice->writeDescriptorTable(DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(Images.size(), img.TextureHandle));
	Images.emplace_back(img);
	return Images.size() - 1;
}

uint32_t HashString(const std::string& str) {
	return hash(std::make_pair(str.c_str(), str.length()));
}

uint32_t Hash(const nvrhi::GraphicsPipelineDesc& desc) {
	struct GraphicsPipelineAbstraction
	{
		//5 shaders debug name hashes
		uint32_t VSHash{}, HSHash{}, DSHash{}, GSHash{}, PSHash{};
		//hash the states
		uint32_t RenderStateHash{};
		uint32_t PrimTypeHash{};
		uint32_t ShadingRateStateHash{};
		//hash the bindings
		uint32_t BindingsHash[nvrhi::c_MaxBindingLayouts]{ {}, };
		//no need for the input layout as everyone uses the same due to the instance pass
	}obj;
	//hashing all the shaders debug name
	if (desc.VS)
		obj.VSHash = HashString(desc.VS->getDesc().debugName);
	if (desc.HS)
		obj.HSHash = HashString(desc.HS->getDesc().debugName);
	if (desc.DS)
		obj.DSHash = HashString(desc.DS->getDesc().debugName);
	if (desc.GS)
		obj.GSHash = HashString(desc.GS->getDesc().debugName);
	if (desc.PS)
		obj.PSHash = HashString(desc.PS->getDesc().debugName);
	//hash the states
	obj.RenderStateHash = HashBytes(&desc.renderState);
	obj.PrimTypeHash = HashBytes(&desc.primType);
	obj.ShadingRateStateHash = HashBytes(&desc.shadingRateState);
	//hash the bindings
	for (int i = 0; i < nvrhi::c_MaxBindingLayouts; ++i) {
		obj.BindingsHash[i] = HashBytes(&desc.bindingLayouts);
	}
	return HashBytes(&obj);
}

uint32_t Hash(const nvrhi::ComputePipelineDesc& desc) {
	struct ComputePipelineAbstraction {
		//shader
		uint32_t CSHash{};
		//binding hash
		uint32_t BindingsHash[nvrhi::c_MaxBindingLayouts]{ {}, };
	}obj;
	if (desc.CS)
		obj.CSHash = HashString(desc.CS->getDesc().debugName);
	for (int i = 0; i < nvrhi::c_MaxBindingLayouts; ++i) {
		obj.BindingsHash[i] = HashBytes(&desc.bindingLayouts);
	}
	return HashBytes(&obj);
}

nvrhi::GraphicsPipelineHandle AssetManager::GetGraphicsPipeline(const nvrhi::GraphicsPipelineDesc& desc, const nvrhi::FramebufferHandle& fb)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	uint32_t hash = Hash(desc);
	if (GPSOs.contains(hash))
		return GPSOs.at(hash);
	RD_CORE_INFO("GPSO created");
	return GPSOs[hash] = DirectXDevice::GetNativeDevice()->createGraphicsPipeline(desc, fb);
}

nvrhi::ComputePipelineHandle AssetManager::GetComputePipeline(const nvrhi::ComputePipelineDesc& desc)
{
	uint32_t hash = Hash(desc);
	if (CPSOs.contains(hash))
		return CPSOs.at(hash);
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);
		RD_CORE_INFO("CPSO created");
		return CPSOs[hash] = DirectXDevice::GetNativeDevice()->createComputePipeline(desc);
	}
}

nvrhi::BindingLayoutHandle AssetManager::GetBindingLayout(const nvrhi::BindingSetDesc& desc)
{
	RD_SCOPE(Asset, GetBindingLayout);
	static auto ConvertSetToLayout = [](const nvrhi::BindingSetItemArray& setDesc, nvrhi::BindingLayoutItemArray& layoutDesc)
		{
			for (auto& item : setDesc)
			{
				nvrhi::BindingLayoutItem layoutItem{};
				layoutItem.slot = item.slot;
				layoutItem.type = item.type;
				if (item.type == nvrhi::ResourceType::PushConstants)
					layoutItem.size = uint32_t(item.range.byteSize);
				layoutDesc.push_back(layoutItem);
			}
		};
	nvrhi::BindingLayoutDesc layoutDesc;
	layoutDesc.visibility = nvrhi::ShaderType::All;
	ConvertSetToLayout(desc.bindings, layoutDesc.bindings);
	uint32_t hash = HashBytes(&layoutDesc);
	if (BindingLayouts.contains(hash))
		return BindingLayouts.at(hash);
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);
		RD_CORE_INFO("Binding Layout created");
		return BindingLayouts[hash] = DirectXDevice::GetNativeDevice()->createBindingLayout(layoutDesc);
	}
}

void AssetManager::RecompileShaders()
{
	std::filesystem::path Path = FileManagerRef->GetRoot().parent_path() / "Tools";
	system(("call " + Path.string() + "\\compileShader.bat < NUL").c_str());
	Shaders.clear();
	GPSOs.clear();
	CPSOs.clear();
}

void AssetManager::Init(std::shared_ptr<ragdoll::FileManager> fm)
{
	FileManagerRef = fm;
	nvrhi::DeviceHandle Device = DirectXDevice::GetNativeDevice();
	//set up the command list
	CommandList = Device->createCommandList();
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
	InstancedVertexAttributes = {
		vPositionAttrib,
		vNormalAttrib,
		vTangentAttrib,
		vTexcoordAttrib,
	};
	InstancedInputLayoutHandle = Device->createInputLayout(InstancedVertexAttributes.data(), InstancedVertexAttributes.size(), nullptr);

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
	AssetManager::GetInstance()->DefaultTex = Device->createTexture(textureDesc);
	uint8_t color[4][4] = { 
		{255, 255, 255, 255},
		{255, 255, 255, 255},
		{255, 255, 255, 255},
		{255, 255, 255, 255},
	};
	CommandList->writeTexture(AssetManager::GetInstance()->DefaultTex, 0, 0, &color, 8);

	textureDesc.debugName = "Error Texture";
	AssetManager::GetInstance()->ErrorTex = Device->createTexture(textureDesc);
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
	Samplers[(int)SamplerTypes::Point_Clamp] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	Samplers[(int)SamplerTypes::Point_Wrap] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Point_Repeat] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Mirror;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Mirror;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Mirror;
	Samplers[(int)SamplerTypes::Point_Mirror] = Device->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	Samplers[(int)SamplerTypes::Linear_Clamp] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	Samplers[(int)SamplerTypes::Linear_Wrap] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Linear_Repeat] = Device->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	Samplers[(int)SamplerTypes::Trilinear_Clamp] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	Samplers[(int)SamplerTypes::Trilinear_Wrap] = Device->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Trilinear_Repeat] = Device->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.borderColor = 0.f;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Border;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Border;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Border;
	samplerDesc.reductionType = nvrhi::SamplerReductionType::Comparison;
	ShadowSampler = Device->createSampler(samplerDesc);
#pragma endregion

	//create the bindless descriptor table
	nvrhi::BindlessLayoutDesc bindlessDesc;

	bindlessDesc.visibility = nvrhi::ShaderType::Pixel;
	bindlessDesc.maxCapacity = 1024;
	bindlessDesc.firstSlot = 0;
	bindlessDesc.registerSpaces = {
		nvrhi::BindingLayoutItem::Texture_SRV(1)
	};

	BindlessLayoutHandle = Device->createBindlessLayout(bindlessDesc);
	DescriptorTable = Device->createDescriptorTable(BindlessLayoutHandle);
	//TODO: not sure why i am forced to resize
	Device->resizeDescriptorTable(DescriptorTable, bindlessDesc.maxCapacity, true);

	CommandList->close();
	Device->executeCommandList(CommandList);
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
	MICROPROFILE_GPU_SET_CONTEXT(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer, MicroProfileGetGlobalGpuThreadLog());
	{
		MICROPROFILE_SCOPEI("Render", "Create VBO IBO", MP_BLUEVIOLET);
		//MICROPROFILE_SCOPEGPUI("Create VBO IBO", MP_LIGHTYELLOW1);
		CommandList->beginMarker("Update global buffer");

		nvrhi::BufferDesc vertexBufDesc;
		vertexBufDesc.byteSize = Vertices.size() * sizeof(Vertex);	//the offset is already the size of the vb
		vertexBufDesc.isVertexBuffer = true;
		vertexBufDesc.debugName = "Global vertex buffer";
		vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
		//smth smth syncrhonization need to be this state to be written

		VBO = DirectXDevice::GetInstance()->m_NvrhiDevice->createBuffer(vertexBufDesc);
		//copy data over
		CommandList->beginTrackingBufferState(VBO, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
		CommandList->writeBuffer(VBO, Vertices.data(), vertexBufDesc.byteSize);
		CommandList->setPermanentBufferState(VBO, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

		nvrhi::BufferDesc indexBufDesc;
		indexBufDesc.byteSize = Indices.size() * sizeof(uint32_t);
		indexBufDesc.isIndexBuffer = true;
		indexBufDesc.debugName = "Global index buffer";
		indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

		IBO = DirectXDevice::GetInstance()->m_NvrhiDevice->createBuffer(indexBufDesc);
		CommandList->beginTrackingBufferState(IBO, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(IBO, Indices.data(), indexBufDesc.byteSize);
		CommandList->setPermanentBufferState(IBO, nvrhi::ResourceStates::IndexBuffer);

		CommandList->endMarker();
	}
	CommandList->close();
	DirectXDevice::GetInstance()->m_NvrhiDevice->executeCommandList(CommandList);
	MicroProfileFlip(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer);
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
	else if (shaderFilename.find(".cs.") != std::string::npos) {
		type = nvrhi::ShaderType::Compute;
	}
	uint32_t size{};
	const uint8_t* data = FileManagerRef->ImmediateLoad("cso/" + shaderFilename, size);
	nvrhi::ShaderDesc desc;
	desc.shaderType = type;
	desc.debugName = shaderFilename;
	nvrhi::ShaderHandle shader = DirectXDevice::GetInstance()->m_NvrhiDevice->createShader(
		desc,
		data, size);
	Shaders[shaderFilename] = shader;
	return Shaders.at(shaderFilename);
}