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
		Vector3 cameraPos = { 0.f, 1.f, -5.f };
		Vector2 cameraEulers = { 0.f, 0.f};
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

	CBuffer cbuf;
	cbuf.world = Matrix::Identity;
	Matrix proj = Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(data.cameraFov), data.cameraAspect, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraEulers.x, data.cameraEulers.y, 0.f));
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
			data.cameraEulers.x -= io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
			data.cameraEulers.y += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
		}
	}
	cbuf.lightDiffuseColor = data.dirLightColor;
	//upload the constant buffer
	CommandList->writeBuffer(ConstantBuffer, &cbuf, sizeof(CBuffer));

	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex)
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());

	entt::registry& reg = EntityManager->GetRegistry();
	auto ecsView = reg.view<RenderableComp, MaterialComp>();
	for (entt::entity ent : ecsView) {
		MaterialComp* matComp = EntityManager->GetComponent<MaterialComp>(ent);
		RenderableComp* renderableComp = EntityManager->GetComponent<RenderableComp>(ent);
		//bind the test mesh
		const Mesh& mesh = AssetManager::GetInstance()->Meshes[renderableComp->meshIndex];
		const Texture& meshTex = AssetManager::GetInstance()->Textures[matComp->glTFMaterialRef->pbrMetallicRoughness.baseColorTexture.index];
		const Texture& meshNorms = AssetManager::GetInstance()->Textures[matComp->glTFMaterialRef->normalTexture.index];
		state.indexBuffer = { mesh.IndexBufferHandle, nvrhi::Format::R32_UINT, 0 };
		state.vertexBuffers = {
			{mesh.VertexBufferHandle, 0, offsetof(Vertex, position)},	//POSITION
			{mesh.VertexBufferHandle, 1, offsetof(Vertex, normal)},	//NORMAL
			{mesh.VertexBufferHandle, 2, offsetof(Vertex, texcoord)}	//TEXCOORD
		};
		//bind the buffer
		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
			nvrhi::BindingSetItem::Texture_SRV(0, AssetManager::GetInstance()->Images[meshTex.ImageIndex].TextureHandle),
			nvrhi::BindingSetItem::Sampler(0, meshTex.SamplerHandle),
			nvrhi::BindingSetItem::Texture_SRV(1, AssetManager::GetInstance()->Images[meshNorms.ImageIndex].TextureHandle),
			nvrhi::BindingSetItem::Sampler(1, meshNorms.SamplerHandle)
		};
		BindingSetHandle = Device->m_NvrhiDevice->createBindingSet(bindingSetDesc, BindingLayoutHandle);
		state.addBindingSet(BindingSetHandle);

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
		vNormalAttrib,
		vTangentAttrib,
		vBinormalAttrib,
		vTexcoordAttrib,
	};
	InputLayoutHandle = Device->m_NvrhiDevice->createInputLayout(VertexAttributes.data(), VertexAttributes.size(), ForwardVertexShader);
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
