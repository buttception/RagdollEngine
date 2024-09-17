#include "ragdollpch.h"

#include "ForwardRenderer.h"
#include "DirectXDevice.h"
#include <nvrhi/utils.h>
#include <microprofile.h>

#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"
#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Scene.h"
#include "GeometryBuilder.h"
#include "nvrhi/d3d12-backend.h"

void ForwardRenderer::Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em)
{
	PrimaryWindow = win;
	FileManager = fm;
	EntityManager = em;
	//TODO: create device spec in the future
	Device = DirectXDevice::Create({}, win, fm);
	CreateResource();
}

void ForwardRenderer::Shutdown()
{
	//release nvrhi stuff
	DepthBuffer = nullptr;
	GraphicsPipeline = nullptr;
	ConstantBuffer = nullptr;
	BindingSetHandle = nullptr;
	BindingLayoutHandle = nullptr;
	Device->Shutdown();
	Device->~DirectXDevice();
}

void ForwardRenderer::CreateCustomMeshes()
{
	//create 5 random textures
	uint8_t colors[5][4] = {
		{255, 0, 0, 255},
		{255, 255, 0, 255},
		{0, 255, 255, 255},
		{0, 0, 255, 255},
		{0, 255, 0, 255},
	};
	std::string debugNames[5] = {
		"Custom Red",
		"Custom Yellow",
		"Custom Cyan",
		"Custom Blue",
		"Custom Green",
	};
	CommandList->open();
	for (int i = 0; i < 5; ++i)
	{
		Image img;
		nvrhi::TextureDesc textureDesc;
		textureDesc.width = 1;
		textureDesc.height = 1;
		textureDesc.format = nvrhi::Format::RGBA8_UNORM;
		textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
		textureDesc.isRenderTarget = false;
		textureDesc.isTypeless = false;
		textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
		textureDesc.keepInitialState = true;
		textureDesc.debugName = debugNames[i];
		nvrhi::TextureHandle texHandle = Device->m_NvrhiDevice->createTexture(textureDesc);
		CommandList->writeTexture(texHandle, 0, 0, &colors[i], 4);
		img.TextureHandle = texHandle;
		AssetManager::GetInstance()->Images.emplace_back(img);
		Texture texture;
		texture.ImageIndex = i;
		texture.SamplerIndex = i;
		AssetManager::GetInstance()->Textures.emplace_back(texture);

		AddTextureToTable(texHandle);
	}
	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);

	Material mat;
	mat.bIsLit = true;
	mat.Color = Vector4::One;
	mat.AlbedoTextureIndex = 0;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 1;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 2;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 3;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 4;
	AssetManager::GetInstance()->Materials.emplace_back(mat);

	//build primitives
	GeometryBuilder geomBuilder;
	geomBuilder.Init(Device->m_NvrhiDevice);
	int32_t id = geomBuilder.BuildCube(1.f);
	for (int i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildSphere(1.f, 16);
	for (int i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildCylinder(1.f, 1.f, 16);
	for (int i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildCone(1.f, 1.f, 16);
	for (int i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildIcosahedron(1.f);
	for (int i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	CommandList->open();
	AssetManager::GetInstance()->UpdateVBOIBO(this);
	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);
}

int32_t ForwardRenderer::AddTextureToTable(nvrhi::TextureHandle tex)
{
	Device->m_NvrhiDevice->writeDescriptorTable(DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(TextureCount++, tex));
	return TextureCount - 1;
}

void ForwardRenderer::BeginFrame()
{
	MICROPROFILE_SCOPEI("Render", "Begin Frame", MP_BLUE);
	Device->BeginFrame();
	Device->m_NvrhiDevice->runGarbageCollection();
	nvrhi::TextureHandle tex = Device->GetCurrentBackbuffer();
	{
		nvrhi::TextureSubresourceSet subSet;
		auto bgCol = PrimaryWindow->GetBackgroundColor();
		nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
		CommandList->open();
		CommandList->beginMarker("ClearBackBuffer");
		CommandList->clearTextureFloat(tex, subSet, col);
		CommandList->clearDepthStencilTexture(DepthBuffer, subSet, true, 1.f, false, 0);
		CommandList->endMarker();
		CommandList->close();
		Device->m_NvrhiDevice->executeCommandList(CommandList);
	}
}

void ForwardRenderer::DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, CBuffer& Cbuf)
{
	if (infos.empty())
		return;
	MICROPROFILE_SCOPEI("Render", "Draw All Instances", MP_BLUEVIOLET);
	//create and set the state
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(Device->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer),
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	BindingSetHandle = Device->m_NvrhiDevice->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO }
	};
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(DescriptorTable);

	CommandList->open();
	CommandList->beginMarker("Instance Draws");
	CommandList->writeBuffer(ConstantBuffer, &Cbuf, sizeof(CBuffer));
	CommandList->setGraphicsState(state);

	for (const ragdoll::InstanceGroupInfo& info : infos)
	{
		const VertexBufferInfo& buffer = AssetManager::GetInstance()->VertexBufferInfos[info.VertexBufferIndex];
		nvrhi::DrawArguments args;
		args.vertexCount = buffer.IndicesCount;
		args.startVertexLocation = buffer.VBOffset;
		args.startIndexLocation = buffer.IBOffset;
		args.instanceCount = info.InstanceCount;
		Cbuf.instanceOffset = info.InstanceOffset;
		CommandList->writeBuffer(ConstantBuffer, &Cbuf, sizeof(CBuffer));
		CommandList->drawIndexed(args);
	}

	CommandList->endMarker();
	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);
}

void ForwardRenderer::CreateResource()
{
	CommandList = Device->m_NvrhiDevice->createCommandList();
	CommandList->open();
	nvrhi::InputLayoutHandle inputLayout;
	nvrhi::FramebufferHandle fb;

	//load outputted shaders objects
	uint32_t size{};
	const uint8_t* data = FileManager->ImmediateLoad("ForwardShader.vs.cso", size);
	ForwardVertexShader = Device->m_NvrhiDevice->createShader(
		nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex),
		data, size);
	data = FileManager->ImmediateLoad("ForwardShader.ps.cso", size);
	ForwardPixelShader = Device->m_NvrhiDevice->createShader(
		nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel),
		data, size);
	data = FileManager->ImmediateLoad("imgui.vs.cso", size);
	ImguiVertexShader = Device->m_NvrhiDevice->createShader(
		nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex),
		data, size);
	data = FileManager->ImmediateLoad("imgui.ps.cso", size);
	ImguiPixelShader = Device->m_NvrhiDevice->createShader(
		nvrhi::ShaderDesc(nvrhi::ShaderType::Pixel),
		data, size);

	//create a depth buffer
	nvrhi::TextureDesc depthBufferDesc;
	depthBufferDesc.width = PrimaryWindow->GetBufferWidth();
	depthBufferDesc.height = PrimaryWindow->GetBufferHeight();
	depthBufferDesc.initialState = nvrhi::ResourceStates::DepthWrite;
	depthBufferDesc.isRenderTarget = true;
	depthBufferDesc.sampleCount = 1;
	depthBufferDesc.dimension = nvrhi::TextureDimension::Texture2D;
	depthBufferDesc.keepInitialState = true;
	depthBufferDesc.mipLevels = 1;
	depthBufferDesc.format = nvrhi::Format::D32;
	depthBufferDesc.isTypeless = true;
	depthBufferDesc.debugName = "Depth";
	DepthBuffer = Device->m_NvrhiDevice->createTexture(depthBufferDesc);

	//create the fb for the graphics pipeline to draw on
	nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(Device->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);
	fb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	//can set the requirements
	//here is creating a push constant
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
		nvrhi::BindingLayoutItem::Sampler(0),
		nvrhi::BindingLayoutItem::Sampler(1),
		nvrhi::BindingLayoutItem::Sampler(2),
		nvrhi::BindingLayoutItem::Sampler(3),
		nvrhi::BindingLayoutItem::Sampler(4),
		nvrhi::BindingLayoutItem::Sampler(5),
		nvrhi::BindingLayoutItem::Sampler(6),
		nvrhi::BindingLayoutItem::Sampler(7),
		nvrhi::BindingLayoutItem::Sampler(8),
	};
	BindingLayoutHandle = Device->m_NvrhiDevice->createBindingLayout(layoutDesc);
	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(CBuffer), "CBuffer", 1);
	ConstantBuffer = Device->m_NvrhiDevice->createBuffer(cBufDesc);

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
	InputLayoutHandle = Device->m_NvrhiDevice->createInputLayout(VertexAttributes.data(), VertexAttributes.size(), ForwardVertexShader);

	//create a default texture
	nvrhi::TextureDesc textureDesc;
	textureDesc.width = 1;
	textureDesc.height = 1;
	textureDesc.format = nvrhi::Format::RGBA8_UNORM;
	textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
	textureDesc.isRenderTarget = false;
	textureDesc.isTypeless = false;
	textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
	textureDesc.keepInitialState = true;

	textureDesc.debugName = "DefaultTex";
	AssetManager::GetInstance()->DefaultTex = Device->m_NvrhiDevice->createTexture(textureDesc);
	uint8_t defaultColor[4] = { 255, 255, 255, 255 };
	CommandList->writeTexture(AssetManager::GetInstance()->DefaultTex, 0, 0, &defaultColor, 4);

	nvrhi::SamplerDesc samplerDesc;
	AssetManager::GetInstance()->DefaultSampler = Device->m_NvrhiDevice->createSampler(samplerDesc);

	AssetManager::GetInstance()->Samplers.resize((int)SamplerTypes::COUNT);
	//create all the samplers used
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp] = Device->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Wrap] = Device->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 0;
	samplerDesc.magFilter = 0;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Repeat] = Device->m_NvrhiDevice->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp] = Device->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Wrap] = Device->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 0;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Repeat] = Device->m_NvrhiDevice->createSampler(samplerDesc);

	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Clamp;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Trilinear_Clamp] = Device->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Wrap;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Trilinear_Wrap] = Device->m_NvrhiDevice->createSampler(samplerDesc);
	samplerDesc.minFilter = 1;
	samplerDesc.magFilter = 1;
	samplerDesc.mipFilter = 1;
	samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressV = nvrhi::SamplerAddressMode::Repeat;
	samplerDesc.addressW = nvrhi::SamplerAddressMode::Repeat;
	AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Trilinear_Repeat] = Device->m_NvrhiDevice->createSampler(samplerDesc);

	CommandList->close();
	//remember to execute the list
	Device->m_NvrhiDevice->executeCommandList(CommandList);

	//create the bindless descriptor table
	nvrhi::BindlessLayoutDesc bindlessDesc;

	bindlessDesc.visibility = nvrhi::ShaderType::Pixel;
	bindlessDesc.maxCapacity = 1024;
	bindlessDesc.firstSlot = 0;
	bindlessDesc.registerSpaces = {
		nvrhi::BindingLayoutItem::Texture_SRV(1)
	};

	BindlessLayoutHandle = Device->m_NvrhiDevice->createBindlessLayout(bindlessDesc);
	DescriptorTable = Device->m_NvrhiDevice->createDescriptorTable(BindlessLayoutHandle);
	//TODO: not sure why i am forced to resize
	Device->m_NvrhiDevice->resizeDescriptorTable(DescriptorTable, bindlessDesc.maxCapacity, true);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();

	const static int32_t seed = 42;
	std::srand(seed);

	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.addBindingLayout(BindlessLayoutHandle);
	pipelineDesc.setVertexShader(ForwardVertexShader);
	pipelineDesc.setFragmentShader(ForwardPixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Front;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	pipelineDesc.inputLayout = InputLayoutHandle;

	GraphicsPipeline = Device->m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, fb);

	pipelineDesc.renderState.rasterState.fillMode = nvrhi::RasterFillMode::Wireframe;

	WireframePipeline = Device->m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, fb);
}

void ForwardRenderer::DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, uint32_t instanceCount, CBuffer& Cbuf)
{
	if (instanceCount == 0)
		return;
	//make it instanced next time
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(Device->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, instanceBuffer),
	};
	for (int i = 0; i < (int)SamplerTypes::COUNT; ++i)
	{
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(i, AssetManager::GetInstance()->Samplers[i]));
	}
	BindingSetHandle = Device->m_NvrhiDevice->createBindingSet(bindingSetDesc, BindingLayoutHandle);

	nvrhi::GraphicsState state;
	state.pipeline = WireframePipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { AssetManager::GetInstance()->IBO, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{ AssetManager::GetInstance()->VBO }
	};
	state.addBindingSet(BindingSetHandle);
	state.addBindingSet(DescriptorTable);

	CommandList->open();
	CommandList->beginMarker("Debug Draws");
	CommandList->writeBuffer(ConstantBuffer, &Cbuf, sizeof(CBuffer));
	CommandList->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = 36;
	args.startVertexLocation = 0;
	args.startIndexLocation = 0;
	args.instanceCount = instanceCount;
	Cbuf.instanceOffset = 0;
	CommandList->writeBuffer(ConstantBuffer, &Cbuf, sizeof(CBuffer));
	CommandList->drawIndexed(args);

	CommandList->endMarker();
	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);
}
