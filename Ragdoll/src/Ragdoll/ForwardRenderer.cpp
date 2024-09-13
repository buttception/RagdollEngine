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

void ForwardRenderer::BeginFrame(CBuffer* Cbuf)
{
	Device->BeginFrame();
	Device->m_NvrhiDevice->runGarbageCollection();
	nvrhi::TextureHandle tex = Device->GetCurrentBackbuffer();
	{
		nvrhi::TextureSubresourceSet subSet;
		auto bgCol = PrimaryWindow->GetBackgroundColor();
		nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
		CommandList->open();
		CommandList->clearTextureFloat(tex, subSet, col);
		CommandList->clearDepthStencilTexture(DepthBuffer, subSet, true, 1.f, false, 0);
		CommandList->close();
		Device->m_NvrhiDevice->executeCommandList(CommandList);
	}

	//manipulate the cube and camera
	struct Data {
		Vector3 translate{ 0.f, 0.f, 0.f };
		Vector3 scale = { 1.f, 1.f, 1.f };
		Vector3 rotate = { 0.f, 0.f, 0.f };
		Vector3 cameraPos = { 0.f, 1.f, 5.f };
		float cameraYaw = DirectX::XM_PI;
		float cameraPitch = 0.f;
		float cameraFov = 60.f;
		float cameraNear = 0.01f;
		float cameraFar = 1000.f;
		float cameraAspect = 16.f / 9.f;
		float cameraSpeed = 5.f;
		float cameraRotationSpeed = 15.f;
		Color dirLightColor = { 1.f,1.f,1.f,1.f };
		Color ambientLight = { 0.2f, 0.2f, 0.2f, 1.f };
		float ambientIntensity = 0.2f;
		Vector2 azimuthAndElevation = { 0.f, 45.f };
	};
	static Data data;
	ImGui::Begin("Camera Manipulate");
	ImGui::SliderFloat("Camera FOV (Degrees)", &data.cameraFov, 60.f, 120.f);
	ImGui::SliderFloat("Camera Near", &data.cameraNear, 0.01f, 1.f);
	ImGui::SliderFloat("Camera Far", &data.cameraFar, 10.f, 10000.f);
	ImGui::SliderFloat("Camera Aspect Ratio", &data.cameraAspect, 0.01f, 5.f);
	ImGui::SliderFloat("Camera Speed", &data.cameraSpeed, 0.01f, 30.f);
	ImGui::SliderFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 5.f, 100.f);
	ImGui::ColorEdit3("Light Diffuse", &data.dirLightColor.x);
	ImGui::ColorEdit3("Ambient Light Diffuse", &data.ambientLight.x);
	ImGui::SliderFloat("Azimuth (Degrees)", &data.azimuthAndElevation.x, 0.f, 360.f);
	ImGui::SliderFloat("Elevation (Degrees)", &data.azimuthAndElevation.y, -90.f, 90.f);
	ImGui::End();

	Matrix world = Matrix::CreateScale(data.scale);
	world *= Matrix::CreateFromQuaternion(Quaternion::CreateFromYawPitchRoll(data.rotate));
	world *= Matrix::CreateTranslation(data.translate);
	Matrix proj = Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(data.cameraFov), data.cameraAspect, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));
	Matrix view = Matrix::CreateLookAt(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	Cbuf->viewProj = view * proj;
	Cbuf->sceneAmbientColor = data.ambientLight;
	Cbuf->lightDiffuseColor = data.dirLightColor;
	Vector2 azimuthElevationRad = {
		DirectX::XMConvertToRadians(data.azimuthAndElevation.x),
		DirectX::XMConvertToRadians(data.azimuthAndElevation.y) };
	Cbuf->lightDirection = Vector3(
		sinf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x),
		cosf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x),
		sinf(azimuthElevationRad.x));
	Cbuf->cameraPosition = data.cameraPos;

	//hardcoded handling of movement now
	if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
			data.cameraPos += cameraDir * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
			data.cameraPos -= cameraDir * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		Vector3 cameraRight = -cameraDir.Cross(Vector3(0.f, 1.f, 0.f));
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
			data.cameraPos += cameraRight * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
			data.cameraPos -= cameraRight * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			auto& io = ImGui::GetIO();
			data.cameraYaw -= io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
			data.cameraPitch += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
		}
	}
	Cbuf->lightDiffuseColor = data.dirLightColor;
}

void ForwardRenderer::DrawAllInstances(std::vector<ragdoll::InstanceBuffer>* InstanceBuffers, CBuffer* Cbuf)
{
	CommandList->open();
	for (auto& it : *InstanceBuffers)
		DrawInstanceBuffer(&it, Cbuf);

	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);
}

void ForwardRenderer::DrawInstanceBuffer(ragdoll::InstanceBuffer* InstanceBuffer, CBuffer* Cbuf)
{
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(Device->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	const VertexBufferObject& buffer = AssetManager::GetInstance()->VBOs[InstanceBuffer->VertexBufferIndex];

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	state.indexBuffer = { buffer.IndexBufferHandle, nvrhi::Format::R32_UINT, 0 };
	state.vertexBuffers = {
		{buffer.VertexBufferHandle}
	};

	//temp
	const Material& mat = AssetManager::GetInstance()->Materials[InstanceBuffer->MaterialIndices[0]];
	nvrhi::ITexture* albedoTex, * normalTex, * roughnessMetallicTex;
	nvrhi::SamplerHandle albedoSampler, normalSampler, roughnessMetallicSampler;
	albedoTex = normalTex = roughnessMetallicTex = AssetManager::GetInstance()->DefaultTex;
	albedoSampler = normalSampler = roughnessMetallicSampler = AssetManager::GetInstance()->DefaultSampler;
	if (mat.AlbedoIndex >= 0) {
		const Texture& albedo = AssetManager::GetInstance()->Textures[mat.AlbedoIndex];
		albedoTex = AssetManager::GetInstance()->Images[albedo.ImageIndex].TextureHandle;
		albedoSampler = AssetManager::GetInstance()->Samplers[albedo.SamplerIndex];
	}
	if (mat.NormalIndex >= 0) {
		const Texture& normal = AssetManager::GetInstance()->Textures[mat.NormalIndex];
		normalTex = AssetManager::GetInstance()->Images[normal.ImageIndex].TextureHandle;
		normalSampler = AssetManager::GetInstance()->Samplers[normal.SamplerIndex];
	}
	if (mat.MetallicRoughnessIndex >= 0) {
		const Texture& metalRough = AssetManager::GetInstance()->Textures[mat.MetallicRoughnessIndex];
		roughnessMetallicTex = AssetManager::GetInstance()->Images[metalRough.ImageIndex].TextureHandle;
		roughnessMetallicSampler = AssetManager::GetInstance()->Samplers[metalRough.SamplerIndex];
	}
	CommandList->writeBuffer(ConstantBuffer, Cbuf, sizeof(CBuffer));

	nvrhi::BindingSetDesc bindingSetDesc;
	bindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, InstanceBuffer->BufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(1, albedoTex),
		nvrhi::BindingSetItem::Sampler(0, albedoSampler),
		nvrhi::BindingSetItem::Texture_SRV(2, normalTex),
		nvrhi::BindingSetItem::Sampler(1, normalSampler),
		nvrhi::BindingSetItem::Texture_SRV(3, roughnessMetallicTex),
		nvrhi::BindingSetItem::Sampler(2, roughnessMetallicSampler),
	};
	BindingSetHandle = Device->m_NvrhiDevice->createBindingSet(bindingSetDesc, BindingLayoutHandle);
	state.addBindingSet(BindingSetHandle);
	CommandList->setGraphicsState(state);

	nvrhi::DrawArguments args;
	args.vertexCount = buffer.TriangleCount;
	args.instanceCount = InstanceBuffer->InstanceCount;
	CommandList->drawIndexed(args);
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
		nvrhi::BindingLayoutItem::Texture_SRV(1),
		nvrhi::BindingLayoutItem::Sampler(0),
		nvrhi::BindingLayoutItem::Texture_SRV(2),
		nvrhi::BindingLayoutItem::Sampler(1),
		nvrhi::BindingLayoutItem::Texture_SRV(3),
		nvrhi::BindingLayoutItem::Sampler(2),
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
	nvrhi::VertexAttributeDesc vColorAttrib;
	vColorAttrib.name = "COLOR";
	vColorAttrib.offset = offsetof(Vertex, color);
	vColorAttrib.elementStride = sizeof(Vertex);
	vColorAttrib.format = nvrhi::Format::RGBA32_FLOAT;
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
	nvrhi::VertexAttributeDesc vBinormalAttrib;
	vBinormalAttrib.name = "BINORMAL";
	vBinormalAttrib.offset = offsetof(Vertex, binormal);
	vBinormalAttrib.elementStride = sizeof(Vertex);
	vBinormalAttrib.format = nvrhi::Format::RGB32_FLOAT;
	nvrhi::VertexAttributeDesc vTexcoordAttrib;
	vTexcoordAttrib.name = "TEXCOORD";
	vTexcoordAttrib.offset = offsetof(Vertex, texcoord);
	vTexcoordAttrib.elementStride = sizeof(Vertex);
	vTexcoordAttrib.format = nvrhi::Format::RG32_FLOAT;
	VertexAttributes = {
		vPositionAttrib,
		vColorAttrib,
		vNormalAttrib,
		vTangentAttrib,
		vBinormalAttrib,
		vTexcoordAttrib,
	};
	InputLayoutHandle = Device->m_NvrhiDevice->createInputLayout(VertexAttributes.data(), VertexAttributes.size(), ForwardVertexShader);

	//create a default texture
	nvrhi::TextureDesc textureDesc;
	textureDesc.width = 1;
	textureDesc.height = 1;
	textureDesc.format = nvrhi::Format::RGBA8_UNORM;
	textureDesc.isRenderTarget = false;
	textureDesc.isTypeless = false;
	textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
	textureDesc.keepInitialState = true;

	AssetManager::GetInstance()->DefaultTex = Device->m_NvrhiDevice->createTexture(textureDesc);

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

	static bool testWithCustom{ true };
	if (testWithCustom) {
		//create 5 random textures
		//Image img;
		//nvrhi::TextureHandle tex = Device->m_NvrhiDevice->createTexture(textureDesc);
		//Vector4 color = { 1.f,1.f,1.f,1.f };
		//CommandList->writeTexture(tex, 0, 0, &color, 4);
		//img.TextureHandle = tex;
		//AssetManager::GetInstance()->Images.emplace_back(img);

		Material mat;
		mat.bIsLit = true;
		mat.Color = Vector4::One;
		AssetManager::GetInstance()->Materials.emplace_back(mat);
		AssetManager::GetInstance()->Materials.emplace_back(mat);
		AssetManager::GetInstance()->Materials.emplace_back(mat);
		AssetManager::GetInstance()->Materials.emplace_back(mat);
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
	}

	const static int32_t seed = 42;
	std::srand(seed);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();
	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(ForwardVertexShader);
	pipelineDesc.setFragmentShader(ForwardPixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Front;
	pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
	pipelineDesc.inputLayout = InputLayoutHandle;

	GraphicsPipeline = Device->m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, fb);
}
