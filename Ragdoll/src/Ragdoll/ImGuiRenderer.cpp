#include "ragdollpch.h"
#include "ImGuiRenderer.h"
#include "DirectXDevice.h"
#include "backends/imgui_impl_glfw.cpp"
#include "AssetManager.h"

#include "Profiler.h"

void ImguiRenderer::Init(DirectXDevice* dx)
{
	m_DirectXTest = dx;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	ImGui_ImplGlfw_InitForOther(m_DirectXTest->m_PrimaryWindow->GetGlfwWindow(), true);

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	CommandList = m_DirectXTest->m_NvrhiDevice->createCommandList(nvrhi::CommandListParameters().setEnableImmediateExecution(false));
	CommandList->open();

	nvrhi::ShaderHandle imGuiVS = AssetManager::GetInstance()->GetShader("imgui.vs.cso");
	nvrhi::ShaderHandle imGuiPS = AssetManager::GetInstance()->GetShader("imgui.ps.cso");
	// create attribute layout object
	nvrhi::VertexAttributeDesc vertexAttribLayout[] = {
		{ "POSITION", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,pos), sizeof(ImDrawVert), false },
		{ "TEXCOORD", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,uv),  sizeof(ImDrawVert), false },
		{ "COLOR",    nvrhi::Format::RGBA8_UNORM, 1, 0, offsetof(ImDrawVert,col), sizeof(ImDrawVert), false },
	};
	//creating the layout for shader
	ShaderAttribLayout = m_DirectXTest->m_NvrhiDevice->createInputLayout(vertexAttribLayout, _countof(vertexAttribLayout), imGuiVS);

	io.Fonts->AddFontDefault();

	CreateFontTexture();

	//create the imgui pipeline state
	nvrhi::BlendState blendState;
	blendState.targets[0].blendEnable = true;
	blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
	blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
	blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
	blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::Zero;

	nvrhi::RasterState rasterState = nvrhi::RasterState();
	rasterState.fillMode = nvrhi::RasterFillMode::Solid;
	rasterState.cullMode = nvrhi::RasterCullMode::None;
	rasterState.scissorEnable = true;
	rasterState.depthClipEnable = true;

	nvrhi::DepthStencilState depthStencilState = nvrhi::DepthStencilState();
	depthStencilState.depthTestEnable = false;
	depthStencilState.depthWriteEnable = true;
	depthStencilState.stencilEnable = false;
	depthStencilState.depthFunc = nvrhi::ComparisonFunc::Always;

	nvrhi::RenderState renderState = nvrhi::RenderState();
	renderState.blendState = blendState;
	renderState.rasterState = rasterState;
	renderState.depthStencilState = depthStencilState;
	//why do i keep default constructing when it is already implicit
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	//specify the resource is avail on all shader process
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		//bind slot 0 with 2 floats
		nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 2),
		//binds slot 0 with shader resource view, srv is used to read data from textures
		nvrhi::BindingLayoutItem::Texture_SRV(0),
		//binds slot 0 with sampler, controls how the texture is sampled
		nvrhi::BindingLayoutItem::Sampler(0),
	};
	BindingLayout = m_DirectXTest->m_NvrhiDevice->createBindingLayout(layoutDesc);

	BasePSODesc.primType = nvrhi::PrimitiveType::TriangleList;
	BasePSODesc.inputLayout = ShaderAttribLayout;
	BasePSODesc.VS = imGuiVS;
	BasePSODesc.PS = imGuiPS;
	BasePSODesc.renderState = renderState;
	BasePSODesc.bindingLayouts = { BindingLayout };
	CommandList->close();

	m_DirectXTest->m_NvrhiDevice->executeCommandList(CommandList);
}

void ImguiRenderer::BeginFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = (float)m_DirectXTest->m_PrimaryWindow->GetFrameTime();
	io.MouseDrawCursor = false;
	io.DisplaySize.x = (float)m_DirectXTest->m_PrimaryWindow->GetBufferWidth();
	io.DisplaySize.y = (float)m_DirectXTest->m_PrimaryWindow->GetBufferHeight();
	
	ImGui::NewFrame();
}

bool ImguiRenderer::DrawSpawn(ragdoll::DebugInfo& DebugInfo, ragdoll::SceneInformation& SceneInfo, ragdoll::SceneConfig& Config)
{

	bool ret = false;
	if (Config.bIsThereCustomMeshes)
	{
		ImGui::Begin("Spawn");
		if (ImGui::Button("Spawn 500 geometry")) {
			ret = true;
		}
		ImGui::End();
	}
	return ret;
}

int32_t ImguiRenderer::DrawFBViewer()
{
	ImGui::Begin("FB View");
	//view textures
	static int selectedItem = 0;
	const char* items[] = {
		"None",
		"NormalMap",
		"ORM",
		"Velocity",
		"AO",
		"Temporal Color",
		"Shadow Mask",
	};
	ImGui::Combo("Texture View", &selectedItem, items, IM_ARRAYSIZE(items));
	ImGui::End();
	return selectedItem;
}

void ImguiRenderer::DrawSettings(ragdoll::DebugInfo& DebugInfo, ragdoll::SceneInformation& SceneInfo, ragdoll::SceneConfig& Config, float _dt)
{
	static struct Data {
		Vector3 cameraPos = { 0.f, 1.f, 0.f };
		Vector3 cameraDir = { 0.f, 0.f, 1.f };
		float cameraYaw = 0.f;
		float cameraPitch = 0.f;
		float cameraFov = 90.f;
		float cameraNear = 0.01f;
		float cameraFar = 1000.f;
		float cameraWidth = 16.f;
		float cameraHeight = 9.f;
		float cameraSpeed = 5.f;
		float cameraRotationSpeed = 15.f;
		//this is for lights
		Vector2 azimuthAndElevation = { 0.f, 90.f };
	} data;
	if (SceneInfo.ActiveCameraIndex == -1)
	{
		if (SceneInfo.Cameras.size() > 0)
		{
			SceneInfo.ActiveCameraIndex = 0;
			data.cameraPos = SceneInfo.Cameras[0].Position;
			data.cameraYaw = SceneInfo.Cameras[0].Rotation.y;
			data.cameraPitch = SceneInfo.Cameras[0].Rotation.x;
		}
	}

	ImGui::Begin("Debug");
	if (ImGui::Button("Reload Shaders")) {
		//need to call bat file to recompile
		AssetManager::GetInstance()->RecompileShaders();
	}

	const char* resolutions[] = {
		{"960x540"},
		{"1280x720"},
		{"1600x900"},
		{"1920x1080"}
	};
	static int32_t selectedItem{3};
	if(ImGui::Combo("Resolution", &selectedItem, resolutions, 4))
	{
		switch (selectedItem)
		{
		case 0:
			SceneInfo.RenderWidth = 960;
			SceneInfo.RenderHeight = 540;
			break;
		case 1:
			SceneInfo.RenderWidth = 1280;
			SceneInfo.RenderHeight = 720;
			break;
		case 2:
			SceneInfo.RenderWidth = 1600;
			SceneInfo.RenderHeight = 900;
			break;
		case 3:
			SceneInfo.RenderWidth = 1920;
			SceneInfo.RenderHeight = 1080;
			break;
		}
		SceneInfo.bIsResolutionDirty = true;
	}

	static bool firstFrame = true;
	if(!firstFrame)
		SceneInfo.bResetAccumulation = false;
	firstFrame = false;
	if (ImGui::TreeNode("Anti Aliasing & Super Sampling"))
	{
		ImGui::Checkbox("Enable Jitter", &SceneInfo.bEnableJitter);
		if (Config.bInitDLSS)
			if (ImGui::Checkbox("Enable DLSS", &SceneInfo.bEnableDLSS))
			{
				if (SceneInfo.bEnableDLSS)
				{
					SceneInfo.bEnableIntelTAA = false;
					SceneInfo.bEnableFSR = false;
					SceneInfo.bResetAccumulation = true;
				}
			}
		if (ImGui::Checkbox("Enable Intel TAA", &SceneInfo.bEnableIntelTAA))
		{
			if (SceneInfo.bEnableIntelTAA)
			{
				SceneInfo.bEnableDLSS = false;
				SceneInfo.bEnableFSR = false;
				SceneInfo.bResetAccumulation = true;
			}
		}
		if (ImGui::Checkbox("Enable FSR", &SceneInfo.bEnableFSR))
		{
			if (SceneInfo.bEnableFSR)
			{
				SceneInfo.bEnableDLSS = false;
				SceneInfo.bEnableIntelTAA = false;
				SceneInfo.bResetAccumulation = true;
			}
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Ambient Occlusion"))
	{
		if (ImGui::Checkbox("CACAO", &SceneInfo.UseCACAO))
			SceneInfo.UseXeGTAO = SceneInfo.UseCACAO ? false : SceneInfo.UseXeGTAO;
		if (ImGui::Checkbox("XeGTAO", &SceneInfo.UseXeGTAO))
			SceneInfo.UseCACAO = SceneInfo.UseXeGTAO ? false : SceneInfo.UseXeGTAO;
		ImGui::Checkbox("Enable XeGTAO Noise", &SceneInfo.bEnableXeGTAONoise);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Bloom"))
	{
		ImGui::Checkbox("Bloom", &SceneInfo.bEnableBloom);
		ImGui::SliderFloat("Filter Radius", &SceneInfo.FilterRadius, 0.001f, 1.f);
		ImGui::SliderFloat("Bloom Intensity", &SceneInfo.BloomIntensity, 0.f, 1.f);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Tonemap"))
	{
		ImGui::Checkbox("UseFixedExposure", &SceneInfo.UseFixedExposure);
		if (SceneInfo.UseFixedExposure)
			ImGui::SliderFloat("Exposure", &SceneInfo.Exposure, 0.f, 2.f);
		else
			ImGui::Text("Adapted Luminance: %f", SceneInfo.Luminance);
		ImGui::SliderFloat("Gamma", &SceneInfo.Gamma, 0.5f, 3.f);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Lighting"))
	{
		ImGui::Checkbox("Raytrace Directional Light Shadows", &SceneInfo.bRaytraceDirectionalLight);
		if (SceneInfo.bRaytraceDirectionalLight)
		{
			ImGui::Checkbox("Inline Raytracing", &SceneInfo.bInlineRaytrace);
			ImGui::Checkbox("Raytrace Shadows Denoiser", &SceneInfo.bRaytraceShadowDenoiser);
			ImGui::SliderFloat("Sun Size", &SceneInfo.SunSize, 0.f, 1.f);
		}
		ImGui::SliderFloat("Sky Dimmer e-6", &SceneInfo.SkyDimmer, 0.f, 1.f);
		ImGui::ColorEdit3("Light Diffuse", &SceneInfo.LightDiffuseColor.x);
		ImGui::SliderFloat("Light Intensity", &SceneInfo.LightIntensity, 0.1f, 10.f);
		ImGui::ColorEdit3("Ambient Light Diffuse", &SceneInfo.SceneAmbientColor.x);
		if (ImGui::SliderFloat("Azimuth (Degrees)", &data.azimuthAndElevation.x, 0.f, 360.f))
		{
			SceneInfo.bIsCameraDirty = true;
		}
		if (ImGui::SliderFloat("Elevation (Degrees)", &data.azimuthAndElevation.y, 0.01f, 90.f))
		{
			SceneInfo.bIsCameraDirty = true;
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Camera"))
	{
		if (SceneInfo.Cameras.size() > 0)
		{
			ImGui::Text("Active Camera: %s", SceneInfo.Cameras[SceneInfo.ActiveCameraIndex].Name);
			if (ImGui::SliderInt("Camera Index", &SceneInfo.ActiveCameraIndex, 0, SceneInfo.Cameras.size() - 1))
			{
				if (SceneInfo.Cameras.size() > 0)
				{
					data.cameraYaw = SceneInfo.Cameras[SceneInfo.ActiveCameraIndex].Rotation.y;
					data.cameraPitch = SceneInfo.Cameras[SceneInfo.ActiveCameraIndex].Rotation.x;
					data.cameraPos = SceneInfo.Cameras[SceneInfo.ActiveCameraIndex].Position;
					SceneInfo.bIsCameraDirty = true;
				}
			}
		}
		else
			ImGui::Text("No Cameras in scene");
		
		SceneInfo.bIsCameraDirty = !SceneInfo.bIsCameraDirty ? ImGui::SliderFloat("Camera FOV (Degrees)", &data.cameraFov, 60.f, 120.f) : true;
		SceneInfo.bIsCameraDirty = !SceneInfo.bIsCameraDirty ? ImGui::SliderFloat("Camera Near", &data.cameraNear, 0.01f, 1.f) : true;
		SceneInfo.bIsCameraDirty = !SceneInfo.bIsCameraDirty ? ImGui::SliderFloat("Camera Far", &data.cameraFar, 10.f, 10000.f) : true;
		SceneInfo.bIsCameraDirty = !SceneInfo.bIsCameraDirty ? ImGui::SliderFloat("Camera Width", &data.cameraWidth, 0.01f, 30.f) : true;
		SceneInfo.bIsCameraDirty = !SceneInfo.bIsCameraDirty ? ImGui::SliderFloat("Camera Height", &data.cameraHeight, 0.01f, 20.f) : true;
		ImGui::SliderFloat("Camera Speed", &data.cameraSpeed, 0.01f, 30.f);
		ImGui::SliderFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 5.f, 100.f);

		data.cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Debug"))
	{
		if (ImGui::Checkbox("Freeze Culling Matrix", &DebugInfo.bFreezeFrustumCulling))
		{
			if (DebugInfo.bFreezeFrustumCulling)
			{
				DebugInfo.FrozenProjection = SceneInfo.MainCameraProj;
				DebugInfo.FrozenCameraPosition = SceneInfo.MainCameraPosition;
				DebugInfo.FrozenView = SceneInfo.MainCameraView;
			}
			SceneInfo.bIsCameraDirty = true;
		}
		ImGui::Checkbox("Enable Instance Frustum Culling", &SceneInfo.bEnableInstanceFrustumCull);
		if (ImGui::Checkbox("Enable Instance Occlusion Culling", &SceneInfo.bEnableOcclusionCull))
			SceneInfo.bIsCameraDirty = true;
		ImGui::Checkbox("Enable Mesh shading", &SceneInfo.bEnableMeshletShading);
		if (SceneInfo.bEnableMeshletShading)
		{
			ImGui::Checkbox("Enable Mesh Frustum culling", &SceneInfo.bEnableMeshletFrustumCulling);
			ImGui::Checkbox("Enable Mesh Cone culling", &SceneInfo.bEnableMeshletConeCulling);
			ImGui::Checkbox("Enable Mesh Occlusion culling", &SceneInfo.bEnableMeshletOcclusionCulling);
			if (ImGui::Checkbox("Enable Meshlet colors", &SceneInfo.bEnableMeshletColors))
			{
				if (SceneInfo.bEnableMeshletColors)
					SceneInfo.bEnableInstanceColors = false;
			}
		}
		if (ImGui::Checkbox("Enable Instance colors", &SceneInfo.bEnableInstanceColors)) 
		{
			if (SceneInfo.bEnableInstanceColors)
				SceneInfo.bEnableMeshletColors = false;
		}
		if (ImGui::Checkbox("Enable Light Grid", &DebugInfo.bEnableLightGrid))
			SceneInfo.bIsCameraDirty = true;
		if(ImGui::Checkbox("Show Frustum", &DebugInfo.bShowFrustum));
			SceneInfo.bIsCameraDirty = true;
		if (ImGui::Checkbox("Show Light Grid", &DebugInfo.bShowLightGrid));
			SceneInfo.bIsCameraDirty = true;
		if (DebugInfo.bShowLightGrid)
		{
			ImGui::SliderInt2("Light grid slice min max", DebugInfo.LightGridSliceStartEnd, 0, 15);
			DebugInfo.LightGridSliceStartEnd[0] = std::min(DebugInfo.LightGridSliceStartEnd[0], DebugInfo.LightGridSliceStartEnd[1]);
			DebugInfo.LightGridSliceStartEnd[1] = std::max(DebugInfo.LightGridSliceStartEnd[0], DebugInfo.LightGridSliceStartEnd[1]);
		}
		if (ImGui::Checkbox("Show Boxes", &Config.bDrawBoxes))
			SceneInfo.bIsCameraDirty = true;
		if (ImGui::SliderInt("Show Cascades", &SceneInfo.EnableCascadeDebug, 0, 4))
			SceneInfo.bIsCameraDirty = true;
		if (SceneInfo.EnableCascadeDebug > 0) {
			if (ImGui::TreeNode("Cascade Info"))
			{
				for (int i = 0; i < 4; ++i) {
					if (ImGui::TreeNode(("Cascade" + std::to_string(i)).c_str(), "Casecade %d", i))
					{
						ImGui::Text("Position: %1.f, %1.f, %1.f", SceneInfo.CascadeInfos[i].center.x, SceneInfo.CascadeInfos[i].center.y, SceneInfo.CascadeInfos[i].center.z);
						ImGui::Text("Dimension: %.2f, %.2f", SceneInfo.CascadeInfos[i].width, SceneInfo.CascadeInfos[i].height);
						ImGui::Text("NearFar: %.2f, %.2f", SceneInfo.CascadeInfos[i].nearZ, SceneInfo.CascadeInfos[i].farZ);
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}
		ImGui::Text("%d total proxies", DebugInfo.TotalProxyCount);
		ImGui::Text("%d proxies passed frustum test", DebugInfo.PassedFrustumCullCount);
		ImGui::Text("%d proxies passed occlusion 1 test", DebugInfo.PassedOcclusion1CullCount);
		ImGui::Text("%d proxies passed occlusion 2 test", DebugInfo.PassedOcclusion2CullCount);
		if (SceneInfo.bEnableMeshletShading)
		{
			//useless data now because i do not know how meshlets passed the instance test now
			//ImGui::Text("%d total meshlets", DebugInfo.MeshletCount);
			ImGui::Text("%d meshlet cone cull", DebugInfo.MeshletConeCullCount);
			ImGui::Text("%d meshlet frustum cull", DebugInfo.MeshletFrustumCullCount);
			ImGui::Text("%d meshlet phase 1 occlusion cull", DebugInfo.MeshletOcclusion1CullCount);
			ImGui::Text("%d meshlet phase 2 occlusion cull", DebugInfo.MeshletOcclusion2CullCount);
		}
		ImGui::TreePop();
	}
	ImGui::End();

	//hardcoded handling of movement now
	if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()) {
		float speed = data.cameraSpeed;
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftShift))
			speed *= 3.f;
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
		{
			SceneInfo.bIsCameraDirty = true;
			data.cameraPos += data.cameraDir * speed * _dt;
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
		{
			SceneInfo.bIsCameraDirty = true;
			data.cameraPos -= data.cameraDir * speed * _dt;
		}
		Vector3 cameraRight = data.cameraDir.Cross(Vector3(0.f, 1.f, 0.f));
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
		{
			SceneInfo.bIsCameraDirty = true;
			data.cameraPos -= cameraRight * speed * _dt;
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
		{
			SceneInfo.bIsCameraDirty = true;
			data.cameraPos += cameraRight * speed * _dt;
		}
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			SceneInfo.bIsCameraDirty = true;
			auto& io = ImGui::GetIO();
			data.cameraYaw -= io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * _dt;
			data.cameraPitch += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * _dt;
			data.cameraPitch = data.cameraPitch > DirectX::XM_PIDIV2 - 0.1f ? DirectX::XM_PIDIV2 - 0.1f : data.cameraPitch;
			data.cameraPitch = data.cameraPitch < -DirectX::XM_PIDIV2 + 0.1f ? -DirectX::XM_PIDIV2 + 0.1f : data.cameraPitch;
		}
		Vector3 cameraUp = cameraRight.Cross(data.cameraDir);
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_Space))
		{
			SceneInfo.bIsCameraDirty = true;
			data.cameraPos += cameraUp * speed * _dt;
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftCtrl))
		{
			SceneInfo.bIsCameraDirty = true;
			data.cameraPos -= cameraUp * speed * _dt;
		}
	}
	data.cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));

	Vector2 azimuthElevationRad = {
		DirectX::XMConvertToRadians(data.azimuthAndElevation.x),
		DirectX::XMConvertToRadians(data.azimuthAndElevation.y) };
	SceneInfo.LightDirection = Vector3(
		cosf(azimuthElevationRad.y) * sinf(azimuthElevationRad.x),
		sinf(azimuthElevationRad.y),
		cosf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x)
	);
	SceneInfo.LightDirection.Normalize();

	SceneInfo.PrevMainCameraViewProj = SceneInfo.MainCameraViewProj;
	SceneInfo.PrevMainCameraView = SceneInfo.MainCameraView;
	SceneInfo.PrevMainCameraProj = SceneInfo.MainCameraProj;
	if (SceneInfo.bIsCameraDirty)
	{
		SceneInfo.CameraFov = data.cameraFov;
		SceneInfo.CameraAspect = data.cameraWidth / data.cameraHeight;
		SceneInfo.CameraNear = data.cameraNear;
		SceneInfo.CameraFar = data.cameraFar;

		//make a infinite z inverse projection matrix
		float e = 1 / tanf(DirectX::XMConvertToRadians(data.cameraFov) / 2.f);
		SceneInfo.MainCameraProj._11 = e;
		SceneInfo.MainCameraProj._22 = e * (data.cameraWidth / data.cameraHeight);
		SceneInfo.MainCameraProj._33 = 0.f;
		SceneInfo.MainCameraProj._44 = 0.f;
		SceneInfo.MainCameraProj._43 = data.cameraNear;
		SceneInfo.MainCameraProj._34 = -1.f;

		//SceneInfo.MainCameraProj = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(data.cameraFov), data.cameraWidth / data.cameraHeight, data.cameraFar, data.cameraNear);

		CameraProjection = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(data.cameraFov), data.cameraWidth / data.cameraHeight, data.cameraNear, data.cameraFar);

		SceneInfo.MainCameraView = DirectX::XMMatrixLookAtRH(data.cameraPos, data.cameraPos + data.cameraDir, Vector3(0.f, 1.f, 0.f));
		CameraView = SceneInfo.MainCameraView;
		SceneInfo.MainCameraViewProj = SceneInfo.MainCameraView * SceneInfo.MainCameraProj;
		CameraViewProjection = SceneInfo.MainCameraViewProj;
		SceneInfo.MainCameraPosition = data.cameraPos;
	}
}

void ImguiRenderer::Render()
{
	RD_SCOPE(Render, ImGuiBuildCommandBuffer);
	RD_GPU_SCOPE("ImGuiDraw", CommandList);
	ImGui::Render();

	ImDrawData* drawData = ImGui::GetDrawData();
	const auto& io = ImGui::GetIO();

	CommandList->beginMarker("ImGUI");

	if (!UpdateGeometry(CommandList))
	{
		return;
	}

	// handle DPI scaling
	drawData->ScaleClipRects(io.DisplayFramebufferScale);

	float invDisplaySize[2] = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

	// set up graphics state
	nvrhi::GraphicsState drawState;

	nvrhi::TextureHandle tex = m_DirectXTest->m_RhiSwapChainBuffers[m_DirectXTest->m_SwapChain->GetCurrentBackBufferIndex()];
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex);
	nvrhi::FramebufferHandle pipelineFb = m_DirectXTest->m_NvrhiDevice->createFramebuffer(fbDesc);
	drawState.framebuffer = pipelineFb;
	assert(drawState.framebuffer);

	drawState.pipeline = GetPSO(drawState.framebuffer);

	drawState.viewport.viewports.push_back(nvrhi::Viewport(io.DisplaySize.x * io.DisplayFramebufferScale.x,
		io.DisplaySize.y * io.DisplayFramebufferScale.y));
	drawState.viewport.scissorRects.resize(1);  // updated below

	nvrhi::VertexBufferBinding vbufBinding;
	vbufBinding.buffer = VertexBufferHandle;
	vbufBinding.slot = 0;
	vbufBinding.offset = 0;
	drawState.vertexBuffers.push_back(vbufBinding);

	drawState.indexBuffer.buffer = IndexBufferHandle;
	drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
	drawState.indexBuffer.offset = 0;

	// render command lists
	int vtxOffset = 0;
	int idxOffset = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		for (int i = 0; i < cmdList->CmdBuffer.Size; i++)
		{
			const ImDrawCmd* pCmd = &cmdList->CmdBuffer[i];

			if (pCmd->UserCallback)
			{
				pCmd->UserCallback(cmdList, pCmd);
			}
			else {
				nvrhi::BindingSetDesc desc;
				desc.bindings = {
					nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
					nvrhi::BindingSetItem::Texture_SRV(0, (nvrhi::ITexture*)pCmd->TextureId),
					nvrhi::BindingSetItem::Sampler(0, FontSampler)
				};

				nvrhi::BindingSetHandle binding = m_DirectXTest->m_NvrhiDevice->createBindingSet(desc, BindingLayout);
				assert(binding);
				drawState.bindings = { binding };
				//drawState.bindings = { getBindingSet((nvrhi::ITexture*)pCmd->TextureId) };
				assert(drawState.bindings[0]);

				drawState.viewport.scissorRects[0] = nvrhi::Rect(int(pCmd->ClipRect.x),
					int(pCmd->ClipRect.z),
					int(pCmd->ClipRect.y),
					int(pCmd->ClipRect.w));

				nvrhi::DrawArguments drawArguments;
				drawArguments.vertexCount = pCmd->ElemCount;
				drawArguments.startIndexLocation = idxOffset;
				drawArguments.startVertexLocation = vtxOffset;

				CommandList->setGraphicsState(drawState);
				CommandList->setPushConstants(invDisplaySize, sizeof(invDisplaySize));
				CommandList->drawIndexed(drawArguments);
			}

			idxOffset += pCmd->ElemCount;
		}

		vtxOffset += cmdList->VtxBuffer.Size;
	}

	CommandList->endMarker();
}

void ImguiRenderer::BackbufferResizing()
{
	PSO = nullptr;
}

void ImguiRenderer::Shutdown()
{
	CommandList = nullptr;
	FontSampler = nullptr;
	FontTexture = nullptr;
	ShaderAttribLayout = nullptr;
	BindingLayout = nullptr;
	PSO = nullptr;
	VertexBufferHandle = nullptr;
	IndexBufferHandle = nullptr;
	VertexBufferRaw.clear();
	IndexBufferRaw.clear();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	m_DirectXTest = nullptr;
}

bool ImguiRenderer::ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
{
	if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
	{
		nvrhi::BufferDesc desc;
		desc.byteSize = uint32_t(reallocateSize);
		desc.structStride = 0;
		desc.debugName = isIndexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
		desc.canHaveUAVs = false;
		desc.isVertexBuffer = !isIndexBuffer;
		desc.isIndexBuffer = isIndexBuffer;
		desc.isDrawIndirectArgs = false;
		desc.isVolatile = false;
		desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
		desc.keepInitialState = true;

		buffer = m_DirectXTest->m_NvrhiDevice->createBuffer(desc);

		RD_ASSERT(buffer == nullptr, "Imgui buffer resize fail");
		if (!buffer)
		{

			return false;
		}
	}

	return true;
}

nvrhi::IGraphicsPipeline* ImguiRenderer::GetPSO(nvrhi::IFramebuffer* fb)
{
	if (PSO)
		return PSO;

	PSO = m_DirectXTest->m_NvrhiDevice->createGraphicsPipeline(BasePSODesc, fb);
	assert(PSO);

	return PSO;
}

bool ImguiRenderer::UpdateGeometry(nvrhi::ICommandList* commandList)
{
	ImDrawData* drawData = ImGui::GetDrawData();

	// create/resize vertex and index buffers if needed
	if (!ReallocateBuffer(VertexBufferHandle,
		drawData->TotalVtxCount * sizeof(ImDrawVert),
		(drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert),
		false))
	{
		return false;
	}

	if (!ReallocateBuffer(IndexBufferHandle,
		drawData->TotalIdxCount * sizeof(ImDrawIdx),
		(drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
		true))
	{
		return false;
	}

	VertexBufferRaw.resize(VertexBufferHandle->getDesc().byteSize / sizeof(ImDrawVert));
	IndexBufferRaw.resize(IndexBufferHandle->getDesc().byteSize / sizeof(ImDrawIdx));

	// copy and convert all vertices into a single contiguous buffer
	ImDrawVert* vtxDst = &VertexBufferRaw[0];
	ImDrawIdx* idxDst = &IndexBufferRaw[0];

	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];

		memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

		vtxDst += cmdList->VtxBuffer.Size;
		idxDst += cmdList->IdxBuffer.Size;
	}

	commandList->writeBuffer(VertexBufferHandle, &VertexBufferRaw[0], VertexBufferHandle->getDesc().byteSize);
	commandList->writeBuffer(IndexBufferHandle, &IndexBufferRaw[0], IndexBufferHandle->getDesc().byteSize);

	return true;
}

void ImguiRenderer::CreateFontTexture()
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	{
		nvrhi::TextureDesc desc;
		desc.width = width;
		desc.height = height;
		desc.format = nvrhi::Format::RGBA8_UNORM;
		desc.debugName = "ImGui Font Texture";

		FontTexture = m_DirectXTest->m_NvrhiDevice->createTexture(desc);

		//specify the texture data state for the gpu?
		CommandList->beginTrackingTextureState(FontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);

		RD_ASSERT(FontTexture == nullptr, "Failed to create font texture for imgui");

		//draws to the new texture
		//nvhri already deal with allocation
		CommandList->writeTexture(FontTexture, 0, 0, pixels, width * 4);

		CommandList->setPermanentTextureState(FontTexture, nvrhi::ResourceStates::ShaderResource);
		CommandList->commitBarriers();

		io.Fonts->TexID = FontTexture;

		//create the font sampler
		nvrhi::SamplerDesc samplerDesc = nvrhi::SamplerDesc();
		samplerDesc.addressU = samplerDesc.addressV = samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
		samplerDesc.minFilter = samplerDesc.magFilter = samplerDesc.mipFilter = true;
		FontSampler = m_DirectXTest->m_NvrhiDevice->createSampler(samplerDesc);
		RD_ASSERT(FontSampler == nullptr, "Failed to create font sampler for imgui");
	}
}
