#include "ragdollpch.h"

#include "nvrhi/utils.h"
#include "Profiler.h"

#include "AssetManager.h"
#include "DirectXDevice.h"
#include "stb_image.h"

AssetManager* AssetManager::GetInstance()
{
	if(!s_Instance)
	{
		s_Instance = std::make_unique<AssetManager>();
	}
	return s_Instance.get();
}

size_t AssetManager::AddImage(Image img)
{
	DirectXDevice::GetInstance()->m_NvrhiDevice->writeDescriptorTable(DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(static_cast<uint32_t>(Images.size()), img.TextureHandle));
	Images.emplace_back(img);
	return Images.size() - 1;
}

size_t HashString(const std::string& str) {
	return hash(std::make_pair(str.c_str(), str.length()));
}

size_t Hash(const nvrhi::GraphicsPipelineDesc& desc) {
	struct GraphicsPipelineAbstraction
	{
		//5 shaders debug name hashes
		size_t VSHash{}, HSHash{}, DSHash{}, GSHash{}, PSHash{};
		//hash the states
		size_t RenderStateHash{};
		size_t PrimTypeHash{};
		size_t ShadingRateStateHash{};
		//hash the bindings
		size_t BindingsHash[nvrhi::c_MaxBindingLayouts]{ {}, };
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

size_t Hash(const nvrhi::ComputePipelineDesc& desc) {
	struct ComputePipelineAbstraction {
		//shader
		size_t CSHash{};
		//binding hash
		size_t BindingsHash[nvrhi::c_MaxBindingLayouts]{ {}, };
	}obj;
	if (desc.CS)
		obj.CSHash = HashString(desc.CS->getDesc().debugName);
	for (int i = 0; i < nvrhi::c_MaxBindingLayouts; ++i) {
		obj.BindingsHash[i] = HashBytes(&desc.bindingLayouts);
	}
	return HashBytes(&obj);
}

size_t Hash(const nvrhi::rt::PipelineDesc& desc) {
	struct RaytracePipelineAbstraction {
		//shader
		size_t ShaderHashes[16];
		//binding hash
		size_t BindingsHash[nvrhi::c_MaxBindingLayouts]{ {}, };
		//values
		uint32_t MaxPayloadSize;
		uint32_t MaxAttributeSize; // typical case: float2 uv;
		uint32_t MaxRecursionDepth;
	}obj;
	RD_ASSERT(desc.shaders.size() > 16, "Exceed max number of shaders in a rt pipeline, consider increase size");
	for (int i = 0; i < desc.shaders.size(); ++i) {
		obj.ShaderHashes[i] = HashString(desc.shaders[i].exportName);
	}
	for (int i = 0; i < nvrhi::c_MaxBindingLayouts; ++i) {
		obj.BindingsHash[i] = HashBytes(&desc.globalBindingLayouts);
	}
	obj.MaxPayloadSize = HashBytes(&desc.maxPayloadSize);
	obj.MaxAttributeSize = HashBytes(&desc.maxAttributeSize);
	obj.MaxRecursionDepth = HashBytes(&desc.maxRecursionDepth);
	return HashBytes(&obj);
}

nvrhi::GraphicsPipelineHandle AssetManager::GetGraphicsPipeline(const nvrhi::GraphicsPipelineDesc& desc, const nvrhi::FramebufferHandle& fb)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	size_t hash = Hash(desc);
	if (GPSOs.contains(hash))
		return GPSOs.at(hash);
	RD_CORE_INFO("GPSO created");
	return GPSOs[hash] = DirectXDevice::GetNativeDevice()->createGraphicsPipeline(desc, fb);
}

nvrhi::ComputePipelineHandle AssetManager::GetComputePipeline(const nvrhi::ComputePipelineDesc& desc)
{
	size_t hash = Hash(desc);
	if (CPSOs.contains(hash))
		return CPSOs.at(hash);
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);
		RD_CORE_INFO("CPSO created");
		return CPSOs[hash] = DirectXDevice::GetNativeDevice()->createComputePipeline(desc);
	}
}

nvrhi::rt::PipelineHandle AssetManager::GetRaytracePipeline(const nvrhi::rt::PipelineDesc& desc)
{
	size_t hash = Hash(desc);
	if (RTSOs.contains(hash))
		return RTSOs.at(hash);
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);
		RD_CORE_INFO("RTSO created");
		return RTSOs[hash] = DirectXDevice::GetNativeDevice()->createRayTracingPipeline(desc);
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
	size_t hash = HashBytes(&layoutDesc);
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
	std::filesystem::path Path = "\"" + (FileManagerRef->GetRoot().parent_path() / "Tools\\compileShader.bat\" < NUL").string();
	system(("call " + Path.string()).c_str());
	Shaders.clear();
	ShaderLibraries.clear();
	GPSOs.clear();
	CPSOs.clear();
	RTSOs.clear();
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
	//second buffer for the vertex shader for instance id
	nvrhi::VertexAttributeDesc InstanceIdAttrib;
	InstanceIdAttrib.bufferIndex = 1;
	InstanceIdAttrib.name = "INSTANCEID";
	InstanceIdAttrib.offset = 0;
	InstanceIdAttrib.elementStride = sizeof(int32_t);
	InstanceIdAttrib.format = nvrhi::Format::R32_SINT;
	InstanceIdAttrib.isInstanced = true;
	InstancedVertexAttributes = {
		vPositionAttrib,
		vNormalAttrib,
		vTangentAttrib,
		vTexcoordAttrib,
		InstanceIdAttrib,
	};
	InstancedInputLayoutHandle = Device->createInputLayout(InstancedVertexAttributes.data(), static_cast<uint32_t>(InstancedVertexAttributes.size()), nullptr);

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

	std::string NoisePath = "stbn_unitvec2_2Dx1D_128x128x64_";
	for (int i = 0; i < 64; ++i)
	{
		RD_SCOPE(Load, Load File);
		std::filesystem::path modelPath = FileManagerRef->GetRoot() / "bluenoise" / NoisePath;
		modelPath += std::to_string(i) + ".png";
		//load raw bytes, do not use stbi load
		std::ifstream file(modelPath, std::ios::binary | std::ios::ate);
		RD_ASSERT(!file, "Unable to open file {}:{}", strerror(errno), modelPath.string());
		uint32_t size = static_cast<int32_t>(file.tellg());
		file.seekg(0, std::ios::beg);
		std::vector<uint8_t> data(size);
		RD_ASSERT(!file.read((char*)data.data(), size), "Failed to read file {}", modelPath.string());
		//use stbi_info_from_memory to check header on info for how to load
		int w = -1, h = -1, comp = -1, req_comp = 0;
		RD_ASSERT(!stbi_info_from_memory(data.data(), size, &w, &h, &comp), "stb unable to read image {}", modelPath.string());
		if (comp == 3)
			req_comp = 4;
		//dont support hdr images
		//use stbi_load_from_memory to load the image data, set up all the desc with that info
		uint8_t* raw = stbi_load_from_memory(data.data(), size, &w, &h, &comp, req_comp);
		RD_ASSERT(!raw, "Issue loading {}", modelPath.string());

		if (!BlueNoise2D)
		{
			nvrhi::TextureDesc texDesc;
			texDesc.width = w;
			texDesc.height = h;
			texDesc.arraySize = 64;
			texDesc.dimension = nvrhi::TextureDimension::Texture2DArray;
			switch (comp)
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
			texDesc.debugName = "stbn_unitvec2_2Dx1D_128x128x64";
			texDesc.initialState = nvrhi::ResourceStates::ShaderResource;
			texDesc.isRenderTarget = false;
			texDesc.keepInitialState = true;
			BlueNoise2D = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
		}
		//upload the texture data
		CommandList->writeTexture(BlueNoise2D, i, 0, raw, w * (comp == 3 ? 4 : comp));

		stbi_image_free(raw);
	}

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
	samplerDesc.reductionType = nvrhi::SamplerReductionType::Minimum;
	Samplers[(int)SamplerTypes::Point_Clamp_Reduction] = Device->createSampler(samplerDesc);
	samplerDesc.reductionType = nvrhi::SamplerReductionType::Standard;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Point_Repeat] = Device->createSampler(samplerDesc);
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

	samplerDesc.maxAnisotropy = 16;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	Samplers[(int)SamplerTypes::Anisotropic_Repeat] = Device->createSampler(samplerDesc);
#pragma endregion

	//create the bindless descriptor table
	nvrhi::BindlessLayoutDesc bindlessDesc;

	bindlessDesc.visibility = nvrhi::ShaderType::All;
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

size_t AssetManager::AddVertices(const std::vector<Vertex>& newVertices, const std::vector<uint32_t>& newIndices)
{
	uint32_t vCurrOffset = (uint32_t)Vertices.size();
	uint32_t iCurrOffset = (uint32_t)Indices.size();
	Vertices.resize(Vertices.size() + newVertices.size());
	Indices.resize(Indices.size() + newIndices.size());
	memcpy(Vertices.data() + vCurrOffset, newVertices.data(), newVertices.size() * sizeof(Vertex));
	memcpy(Indices.data() + iCurrOffset, newIndices.data(), newIndices.size() * sizeof(uint32_t));
	//create the vertexbuffer info
	VertexBufferInfo info;
	info.VerticesOffset = vCurrOffset;
	info.IndicesOffset = iCurrOffset;
	info.VerticesCount = (uint32_t)newVertices.size();
	info.IndicesCount = (uint32_t)newIndices.size();
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
		vertexBufDesc.structStride = sizeof(Vertex);
		vertexBufDesc.debugName = "Global vertex buffer";
		vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
		vertexBufDesc.canHaveRawViews = true;
		vertexBufDesc.isAccelStructBuildInput = true;
		//smth smth syncrhonization need to be this state to be written

		VBO = DirectXDevice::GetInstance()->m_NvrhiDevice->createBuffer(vertexBufDesc);
		//copy data over
		CommandList->beginTrackingBufferState(VBO, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
		CommandList->writeBuffer(VBO, Vertices.data(), vertexBufDesc.byteSize);
		CommandList->setPermanentBufferState(VBO, nvrhi::ResourceStates::VertexBuffer | nvrhi::ResourceStates::AccelStructBuildInput | nvrhi::ResourceStates::ShaderResource);	//now its a vb

		nvrhi::BufferDesc indexBufDesc;
		indexBufDesc.byteSize = Indices.size() * sizeof(uint32_t);
		indexBufDesc.isIndexBuffer = true;
		indexBufDesc.debugName = "Global index buffer";
		indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;
		indexBufDesc.canHaveRawViews = true;
		indexBufDesc.isAccelStructBuildInput = true;

		IBO = DirectXDevice::GetInstance()->m_NvrhiDevice->createBuffer(indexBufDesc);
		CommandList->beginTrackingBufferState(IBO, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(IBO, Indices.data(), indexBufDesc.byteSize);
		CommandList->setPermanentBufferState(IBO, nvrhi::ResourceStates::IndexBuffer | nvrhi::ResourceStates::AccelStructBuildInput | nvrhi::ResourceStates::ShaderResource);

		CommandList->endMarker();
	}
	CommandList->close();
	DirectXDevice::GetInstance()->m_NvrhiDevice->executeCommandList(CommandList);
}

nvrhi::ShaderHandle AssetManager::GetShader(const std::string& shaderFilename)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
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
	return shader;
}

nvrhi::ShaderLibraryHandle AssetManager::GetShaderLibrary(const std::string& shaderFilename)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	if (ShaderLibraries.contains(shaderFilename)) {
		return ShaderLibraries.at(shaderFilename);
	}
	uint32_t Size{};
	const uint8_t* Data = FileManagerRef->ImmediateLoad("cso/" + shaderFilename, Size);
	nvrhi::ShaderLibraryHandle ShaderLib = DirectXDevice::GetInstance()->m_NvrhiDevice->createShaderLibrary(Data, Size);
	ShaderLibraries[shaderFilename] = ShaderLib;
	return ShaderLib;
}
