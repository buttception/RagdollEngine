#include "ragdollpch.h"

#include "ForwardRenderer.h"
#include "DirectXDevice.h"
#include <nvrhi/utils.h>

#include "AssetManager.h"
#include "Ragdoll/Entity/EntityManager.h"
#include "Ragdoll/Components/TransformComp.h"
#include "Ragdoll/Components/RenderableComp.h"
#include "Ragdoll/Components/MaterialComp.h"

void ForwardRenderer::Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em)
{
	PrimaryWindow = win;
	FileManager = fm;
	EntityManager = em;
	//TODO: create device spec in the future
	Device = DirectXDevice::Create({}, win, fm);
	CreateResource();
}

void ForwardRenderer::Draw()
{
	Device->BeginFrame();
	Device->m_NvrhiDevice->runGarbageCollection();
	nvrhi::TextureSubresourceSet subSet;
	CommandList->open();
	nvrhi::TextureHandle tex = Device->GetCurrentBackbuffer();

	auto bgCol = PrimaryWindow->GetBackgroundColor();
	nvrhi::Color col = nvrhi::Color(bgCol.x, bgCol.y, bgCol.z, bgCol.w);
	CommandList->clearTextureFloat(tex, subSet, col);
	CommandList->clearDepthStencilTexture(DepthBuffer, subSet, true, 1.f, false, 0);

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
		float cameraFar = 100.f;
		float cameraAspect = 16.f / 9.f;
		float cameraSpeed = 2.5f;
		float cameraRotationSpeed = 5.f;
		Color dirLightColor = { 1.f,1.f,1.f,1.f };
		Color ambientLight = { 0.2f, 0.2f, 0.2f, 1.f };
		float ambientIntensity = 0.2f;
		Vector2 azimuthAndElevation = { 300.f, 45.f };
	};
	static Data data;
	ImGui::Begin("Camera Manipulate");
	ImGui::SliderFloat("Camera FOV (Degrees)", &data.cameraFov, 60.f, 120.f);
	ImGui::SliderFloat("Camera Near", &data.cameraNear, 0.01f, 1.f);
	ImGui::SliderFloat("Camera Far", &data.cameraFar, 10.f, 10000.f);
	ImGui::SliderFloat("Camera Aspect Ratio", &data.cameraAspect, 0.01f, 5.f);
	ImGui::SliderFloat("Camera Speed", &data.cameraSpeed, 0.01f, 5.f);
	ImGui::SliderFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 5.f, 100.f);
	ImGui::ColorEdit3("Light Diffuse", &data.dirLightColor.x);
	ImGui::ColorEdit3("Ambient Light Diffuse", &data.ambientLight.x);
	ImGui::SliderFloat("Azimuth (Degrees)", &data.azimuthAndElevation.x, 0.f, 360.f);
	ImGui::SliderFloat("Elevation (Degrees)", &data.azimuthAndElevation.y, -90.f, 90.f);
	ImGui::End();
	ImGui::Begin("Object Manipulate");
	ImGui::DragFloat3("Translation", &data.translate.x, 0.01f);
	ImGui::DragFloat3("Scale", &data.scale.x, 0.01f);
	ImGui::DragFloat3("Rotate", &data.rotate.x, 0.01f);
	ImGui::End();

	CBuffer cbuf;
	Matrix world = Matrix::CreateScale(data.scale);
	world *= Matrix::CreateFromQuaternion(Quaternion::CreateFromYawPitchRoll(data.rotate));
	world *= Matrix::CreateTranslation(data.translate);
	cbuf.world = world;
	cbuf.invWorldMatrix = world.Invert();
	Matrix proj = Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(data.cameraFov), data.cameraAspect, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));
	Matrix view = Matrix::CreateLookAt(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	cbuf.viewProj = view * proj;
	cbuf.sceneAmbientColor = data.ambientLight;
	cbuf.lightDiffuseColor = data.dirLightColor;
	Vector2 azimuthElevationRad = {
		DirectX::XMConvertToRadians(data.azimuthAndElevation.x),
		DirectX::XMConvertToRadians(data.azimuthAndElevation.y) };
	cbuf.lightDirection = Vector3(
		sinf(azimuthElevationRad.y)*cosf(azimuthElevationRad.x),
		cosf(azimuthElevationRad.y)*cosf(azimuthElevationRad.x),
		sinf(azimuthElevationRad.x));
	cbuf.cameraPosition = data.cameraPos;

	//hardcoded handling of movement now
	if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
			data.cameraPos += cameraDir * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		else if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
			data.cameraPos -= cameraDir * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		Vector3 cameraRight = -cameraDir.Cross(Vector3(0.f, 1.f, 0.f));
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
			data.cameraPos += cameraRight * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		else if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
			data.cameraPos -= cameraRight * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			auto& io = ImGui::GetIO();
			data.cameraYaw -= io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
			data.cameraPitch += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
		}
	}
	cbuf.lightDiffuseColor = data.dirLightColor;

	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex)
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());

	entt::registry& reg = EntityManager->GetRegistry();
	auto ecsView = reg.view<RenderableComp, MaterialComp, TransformComp>();
	for (entt::entity ent : ecsView) {
		MaterialComp* matComp = EntityManager->GetComponent<MaterialComp>(ent);
		RenderableComp* renderableComp = EntityManager->GetComponent<RenderableComp>(ent);
		//bind the test mesh
		const Mesh& mesh = AssetManager::GetInstance()->Meshes[renderableComp->meshIndex];
		state.indexBuffer = { mesh.IndexBufferHandle, nvrhi::Format::R32_UINT, 0 };
		state.vertexBuffers = {
			{mesh.VertexBufferHandle, 0, offsetof(Vertex, position)},	//POSITION
			{mesh.VertexBufferHandle, 1, offsetof(Vertex, color)},		//COLOR
			{mesh.VertexBufferHandle, 2, offsetof(Vertex, normal)},		//NORMAL
			{mesh.VertexBufferHandle, 3, offsetof(Vertex, tangent)},	//TANGENT
			{mesh.VertexBufferHandle, 4, offsetof(Vertex, binormal)},	//BINORMAL
			{mesh.VertexBufferHandle, 5, offsetof(Vertex, texcoord)}	//TEXCOORD
		};
		cbuf.albedoFactor = matComp->Color;
		cbuf.metallic = matComp->Metallic;
		cbuf.roughness = matComp->Roughness;
		cbuf.isLit = matComp->bIsLit;

		nvrhi::ITexture* albedoTex, * normalTex, * roughnessMetallicTex;
		nvrhi::SamplerHandle albedoSampler, normalSampler, roughnessMetallicSampler;
		albedoTex = normalTex = roughnessMetallicTex = AssetManager::GetInstance()->DefaultTex;
		albedoSampler = normalSampler = roughnessMetallicSampler = AssetManager::GetInstance()->DefaultSampler;
		if (matComp->AlbedoIndex >= 0) {
			const Texture& albedo = AssetManager::GetInstance()->Textures[matComp->AlbedoIndex];
			albedoTex = AssetManager::GetInstance()->Images[albedo.ImageIndex].TextureHandle;
			albedoSampler = albedo.SamplerHandle;
			cbuf.useAlbedo = true;
		}
		if (matComp->NormalIndex >= 0) {
			const Texture& normal = AssetManager::GetInstance()->Textures[matComp->NormalIndex];
			normalTex = AssetManager::GetInstance()->Images[normal.ImageIndex].TextureHandle;
			normalSampler = normal.SamplerHandle;
			cbuf.useNormalMap = true;
		}
		if (matComp->MetallicRoughnessIndex >= 0) {
			const Texture& metalRough = AssetManager::GetInstance()->Textures[matComp->MetallicRoughnessIndex];
			roughnessMetallicTex = AssetManager::GetInstance()->Images[metalRough.ImageIndex].TextureHandle;
			roughnessMetallicSampler = metalRough.SamplerHandle;
			cbuf.useMetallicRoughnessMap = true;
		}
		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, albedoTex),
			nvrhi::BindingSetItem::Sampler(0, albedoSampler),
			nvrhi::BindingSetItem::Texture_SRV(1, normalTex),
			nvrhi::BindingSetItem::Sampler(1, normalSampler),
			nvrhi::BindingSetItem::Texture_SRV(2, roughnessMetallicTex),
			nvrhi::BindingSetItem::Sampler(2, roughnessMetallicSampler),
		};
		BindingSetHandle = Device->m_NvrhiDevice->createBindingSet(bindingSetDesc, BindingLayoutHandle);
		state.addBindingSet(BindingSetHandle);

		//upload the constant buffer
		CommandList->writeBuffer(ConstantBuffer, &cbuf, sizeof(CBuffer));

		CommandList->setGraphicsState(state);
		nvrhi::DrawArguments args;
		args.vertexCount = mesh.VertexCount;
		CommandList->drawIndexed(args);
	}
	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);
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
		//binds slot 0 with shader resource view, srv is used to read data from textures
		nvrhi::BindingLayoutItem::Texture_SRV(0),
		//binds slot 0 with sampler, controls how the texture is sampled
		nvrhi::BindingLayoutItem::Sampler(0),
		nvrhi::BindingLayoutItem::Texture_SRV(1),
		nvrhi::BindingLayoutItem::Sampler(1),
		nvrhi::BindingLayoutItem::Texture_SRV(2),
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

	CommandList->close();
	//remember to execute the list
	Device->m_NvrhiDevice->executeCommandList(CommandList);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();
	pipelineDesc.addBindingLayout(BindingLayoutHandle);
	pipelineDesc.setVertexShader(ForwardVertexShader);
	pipelineDesc.setFragmentShader(ForwardPixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::Front;	//does nothing?
	pipelineDesc.inputLayout = InputLayoutHandle;

	GraphicsPipeline = Device->m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, fb);
}
