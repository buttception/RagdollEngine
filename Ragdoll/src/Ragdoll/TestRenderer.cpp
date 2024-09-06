#include "ragdollpch.h"

#include "TestRenderer.h"
#include "DirectXDevice.h"

void TestRenderer::Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm)
{
	PrimaryWindow = win;
	FileManager = fm;
	//TODO: create device spec in the future
	Device = DirectXDevice::Create({}, win, fm);
	Loader.Init(FileManager->GetRoot(), this, fm);
	CreateResource();
}

void TestRenderer::Draw()
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
		Vector3 cubePosition = { 0.f, 0.f, 0.f };
		Vector3 cubeScale = { 1.f, 1.f, 1.f };
		Vector3 cubeEulers = { 0.f, 0.f, 0.f };
		Vector3 cameraPos = { 0.f, 0.f, -5.f };
		Vector2 cameraEulers = { 0.f, 0.f };
		float cameraFov = 60.f;
		float cameraNear = 0.01f;
		float cameraFar = 100.f;
		float cameraAspect = 16.f / 9.f;
		float cameraSpeed = 1.f;
		float cameraRotationSpeed = 5.f;
	};
	static Data data;
	ImGui::Begin("Triangle Manipulate");
	ImGui::DragFloat3("Translation", &data.cubePosition.x, 0.01f);
	ImGui::DragFloat3("Scale", &data.cubeScale.x, 0.01f);
	ImGui::DragFloat3("Rotate (Radians)", &data.cubeEulers.x, 0.01f);
	ImGui::DragFloat("Camera FOV (Degrees)", &data.cameraFov, 1.f, 60.f, 120.f);
	ImGui::DragFloat("Camera Near", &data.cameraNear, 0.01f, 0.01f, 1.f);
	ImGui::DragFloat("Camera Far", &data.cameraFar, 10.f, 10.f, 10000.f);
	ImGui::DragFloat("Camera Aspect Ratio", &data.cameraAspect, 0.01f, 0.01f, 5.f);
	ImGui::DragFloat("Camera Speed", &data.cameraSpeed, 0.01f, 0.01f, 5.f);
	ImGui::DragFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 1.f, 10.f, 100.f);
	ImGui::End();

	CBuffer cbuf;
	cbuf.world = Matrix::CreateTranslation(data.cubePosition);
	cbuf.world *= Matrix::CreateScale(data.cubeScale);
	cbuf.world *= Matrix::CreateFromYawPitchRoll(data.cubeEulers.x, data.cubeEulers.y, data.cubeEulers.z);
	Matrix proj = Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(data.cameraFov), data.cameraAspect, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraEulers.x, data.cameraEulers.y, 0.f));
	Matrix view = Matrix::CreateLookAt(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	cbuf.viewProj = view * proj;

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

	//draw the triangle
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex)
		.setDepthAttachment(DepthBuffer);
	nvrhi::FramebufferHandle pipelineFb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);
	nvrhi::GraphicsState state;
	state.pipeline = GraphicsPipeline;
	state.framebuffer = pipelineFb;
	state.viewport.addViewportAndScissorRect(pipelineFb->getFramebufferInfo().getViewport());
	//bind the test mesh
	const auto& mesh = Meshes["Mesh"];
	state.indexBuffer = { mesh.Buffers.IndexBuffer, mesh.Buffers.IndexFormat, 0 };
	state.vertexBuffers = {
		{mesh.Buffers.VertexBuffer, 0, mesh.Buffers.Attribs.AttribsDesc[0].offset},
		{mesh.Buffers.VertexBuffer, 1, mesh.Buffers.Attribs.AttribsDesc[1].offset}
	};
	//state.indexBuffer = { IndexBuffer, nvrhi::Format::R32_UINT, 0 };
	//state.vertexBuffers = { {VertexBuffer, 0, offsetof(Vertex, position)}, {VertexBuffer, 1, offsetof(Vertex, color)} };

	auto layoutDesc = nvrhi::BindingSetDesc();
	layoutDesc.bindings = {
		nvrhi::BindingSetItem::PushConstants(0, sizeof(CBuffer)),
	};
	auto binding = Device->m_NvrhiDevice->createBindingSet(layoutDesc, BindingLayout);
	state.bindings = { binding };
	assert(state.bindings[0]);

	CommandList->setGraphicsState(state);
	CommandList->setPushConstants(&cbuf, sizeof(CBuffer));
	nvrhi::DrawArguments args;
	args.vertexCount = 36;
	CommandList->drawIndexed(args);
	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);
}

void TestRenderer::Shutdown()
{
	//release nvrhi stuff
	DepthBuffer = nullptr;
	GraphicsPipeline = nullptr;
	Device->Shutdown();
	Device->~DirectXDevice();
}

void TestRenderer::CreateResource()
{
	CommandList = Device->m_NvrhiDevice->createCommandList();
	CommandList->open();
	nvrhi::InputLayoutHandle inputLayout;
	nvrhi::FramebufferHandle fb;

	//load outputted shaders objects
	uint32_t size{};
	const uint8_t* data = FileManager->ImmediateLoad("test.vs.cso", size);
	TestVertexShader = Device->m_NvrhiDevice->createShader(
		nvrhi::ShaderDesc(nvrhi::ShaderType::Vertex),
		data, size);
	data = FileManager->ImmediateLoad("test.ps.cso", size);
	TestPixelShader = Device->m_NvrhiDevice->createShader(
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
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(Device->GetCurrentBackbuffer())
		.setDepthAttachment(DepthBuffer);
	fb = Device->m_NvrhiDevice->createFramebuffer(fbDesc);

	//can set the requirements
	auto layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		//bind slot 0 with 2 floats
		nvrhi::BindingLayoutItem::PushConstants(0, sizeof(CBuffer)),
	};
	BindingLayout = Device->m_NvrhiDevice->createBindingLayout(layoutDesc);

	//load the gltf model
	//for now force load the box.gltf
	Loader.LoadAndCreateModel("Box.gltf", Meshes);

	CommandList->close();
	Device->m_NvrhiDevice->executeCommandList(CommandList);

	auto pipelineDesc = nvrhi::GraphicsPipelineDesc();
	pipelineDesc.addBindingLayout(BindingLayout);
	pipelineDesc.setVertexShader(TestVertexShader);
	pipelineDesc.setFragmentShader(TestPixelShader);

	pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
	pipelineDesc.renderState.depthStencilState.stencilEnable = false;
	pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
	pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	pipelineDesc.inputLayout = Meshes["Mesh"].Buffers.Attribs.InputLayoutHandle;

	GraphicsPipeline = Device->m_NvrhiDevice->createGraphicsPipeline(pipelineDesc, fb);
}
