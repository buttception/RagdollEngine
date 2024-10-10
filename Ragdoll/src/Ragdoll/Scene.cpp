#include "ragdollpch.h"

#include "Scene.h"

#include <nvrhi/utils.h>
#include <microprofile.h>
#include "Application.h"
#include "Entity/EntityManager.h"
#include "AssetManager.h"
#include "DirectXDevice.h"
#include "GeometryBuilder.h"

ragdoll::Scene::Scene(Application* app)
{
	EntityManagerRef = app->m_EntityManager;
	PrimaryWindowRef = app->m_PrimaryWindow;

	CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	CreateRenderTargets();

	DeferredRenderer = std::make_shared<class Renderer>();
	DeferredRenderer->Init(app->m_PrimaryWindow, this);
	if (app->Config.bCreateCustomMeshes)
	{
		Config.bIsThereCustomMeshes = true;
		CreateCustomMeshes();
	}
	Config.bDrawBoxes = app->Config.bDrawDebugBoundingBoxes;
	Config.bDrawOctree = app->Config.bDrawDebugOctree;
	ImguiInterface = std::make_shared<ImguiRenderer>();
	ImguiInterface->Init(DirectXDevice::GetInstance());
}

void ragdoll::Scene::Update(float _dt)
{
	ImguiInterface->BeginFrame();
	UpdateControls(_dt);

	//spawn more shits temp
	const static Vector3 t_range{ 50.f, 50.f, 50.f };
	const static Vector3 t_min = { -25.f, -25.f, -25.f };
	const static Vector3 s_range{ 0.2f, 0.2f, 0.2f };
	const static Vector3 s_min{ 0.3f, 0.3f, 0.3f };
	static int32_t currentGeomCount{};

	if (Config.bIsThereCustomMeshes)
	{
		ImGui::Begin("Spawn");
		ImGui::Text("Current Geom Count: %d", currentGeomCount);
		if (ImGui::Button("Spawn 500 geometry")) {
			for (int i = 0; i < 500; ++i) {
				MICROPROFILE_SCOPEI("Creation", "Entity Create", MP_GREEN);
				currentGeomCount++;
				Vector3 pos{
					t_min.x + (std::rand() / (float)RAND_MAX) * t_range.x,
					t_min.y + (std::rand() / (float)RAND_MAX) * t_range.y,
					t_min.z + (std::rand() / (float)RAND_MAX) * t_range.z,
				};
				Vector3 scale{
					s_min.x + (std::rand() / (float)RAND_MAX) * s_range.x,
					s_min.y + (std::rand() / (float)RAND_MAX) * s_range.y,
					s_min.z + (std::rand() / (float)RAND_MAX) * s_range.z,
				};
				Vector3 eulerRotate{
					std::rand() / (float)RAND_MAX * DirectX::XM_2PI,
					std::rand() / (float)RAND_MAX * DirectX::XM_2PI,
					std::rand() / (float)RAND_MAX * DirectX::XM_2PI
				};
				entt::entity ent;
				{
					MICROPROFILE_SCOPEI("Creation", "Creating Entity", MP_DARKGREEN);
					ent = EntityManagerRef->CreateEntity();
				}
				{
					MICROPROFILE_SCOPEI("Creation", "Setting Transform", MP_GREENYELLOW);
					auto tcomp = EntityManagerRef->AddComponent<TransformComp>(ent);
					tcomp->m_LocalPosition = pos;
					tcomp->m_LocalScale = scale;
					tcomp->m_LocalRotation = Quaternion::CreateFromYawPitchRoll(eulerRotate.y, eulerRotate.x, eulerRotate.z);
					{
						MICROPROFILE_SCOPEI("Creation", "Adding at Root", MP_DARKOLIVEGREEN);
						AddEntityAtRootLevel(EntityManagerRef->GetGuid(ent));
					}
				}
				{
					MICROPROFILE_SCOPEI("Creation", "Setting Renderable", MP_FORESTGREEN);
					auto rcomp = EntityManagerRef->AddComponent<RenderableComp>(ent);
					rcomp->meshIndex = std::rand() / (float)RAND_MAX * 25;
				}
			}
			{
				MICROPROFILE_SCOPEI("Creation", "Entity Update", MP_DARKSEAGREEN);
				UpdateTransforms();			//update all the dirty transforms
				PopulateStaticProxies();	//create all the proxies to iterate
				ResetTransformDirtyFlags();	//reset the dirty flags
				bIsCameraDirty = true;
			}
			//MicroProfileDumpFileImmediately("test.html", nullptr, nullptr);
		}
		ImGui::End();
	}

	ImGui::Begin("Debug");
	if (ImGui::Button("Reload Shaders")) {
		//need to call bat file to recompile
		AssetManager::GetInstance()->RecompileShaders();
	}
	ImGui::SliderFloat("Filter Radius", &SceneInfo.FilterRadius, 0.001f, 1.f);
	ImGui::SliderFloat("Bloom Intensity", &SceneInfo.BloomIntensity, 0.f, 1.f);
	ImGui::SliderFloat("Gamma", &SceneInfo.Gamma, 0.5f, 3.f);
	ImGui::Checkbox("UseFixedExposure", &SceneInfo.UseFixedExposure);
	if (SceneInfo.UseFixedExposure)
		ImGui::SliderFloat("Exposure", &SceneInfo.Exposure, 0.f, 2.f);
	else
		ImGui::Text("Adapted Luminance: %f", DeferredRenderer->AdaptedLuminance);
	ImGui::SliderFloat("Sky Dimmer e-6", &SceneInfo.SkyDimmer, 0.f, 1.f);
	if (ImGui::Checkbox("Freeze Culling Matrix", &bFreezeFrustumCulling))
		bIsCameraDirty = true;
	if (ImGui::Checkbox("Show Octree", &Config.bDrawOctree))
		bIsCameraDirty = true;
	if (Config.bDrawOctree) {
		if (ImGui::DragIntRange2("Octree Level", &Config.DrawOctreeLevelMin, &Config.DrawOctreeLevelMax, 0.1f, 0, Octree::MaxDepth))
			BuildDebugInstances(StaticDebugInstanceDatas);
	}
	if (ImGui::Checkbox("Show Boxes", &Config.bDrawBoxes))
		bIsCameraDirty = true;
	if (ImGui::SliderInt("Show Cascades", &SceneInfo.EnableCascadeDebug, 0, 4))
		BuildDebugInstances(StaticDebugInstanceDatas);
	if (SceneInfo.EnableCascadeDebug > 0) {
		if (ImGui::TreeNode("Cascade Info"))
		{
			for (int i = 0; i < 4; ++i) {
				if (ImGui::TreeNode(("Cascade" + std::to_string(i)).c_str(), "Casecade %d", i))
				{
					ImGui::Text("Position: %1.f, %1.f, %1.f", SceneInfo.CascadeInfo[i].center.x, SceneInfo.CascadeInfo[i].center.y, SceneInfo.CascadeInfo[i].center.z);
					ImGui::Text("Dimension: %.2f, %.2f", SceneInfo.CascadeInfo[i].width, SceneInfo.CascadeInfo[i].height);
					ImGui::Text("NearFar: %.2f, %.2f", SceneInfo.CascadeInfo[i].nearZ, SceneInfo.CascadeInfo[i].farZ);
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}
	ImGui::Text("%d entities count", EntityManagerRef->GetRegistry().view<entt::entity>().size_hint());
	ImGui::Text("%d instance count", StaticInstanceDatas.size());
	ImGui::Text("%d octants culled", DebugInfo.CulledOctantsCount);
	ImGui::Text("%d proxies in octree", Octree::TotalProxies);
	ImGui::End();

	if (bIsCameraDirty)
	{
		DebugInfo.CulledOctantsCount = 0;
		UpdateShadowCascadesExtents();
		BuildStaticCascadeMapInstances();
		UpdateShadowLightMatrices();
		BuildStaticInstances(CameraProjection, CameraView, StaticInstanceDatas, StaticInstanceGroupInfos);
		BuildDebugInstances(StaticDebugInstanceDatas);
	}

	DeferredRenderer->Render(this, _dt);

	ImguiInterface->Render();
	DirectXDevice::GetInstance()->Present();

	bIsCameraDirty = false;
}

void ragdoll::Scene::Shutdown()
{
	ImguiInterface->Shutdown();
	ImguiInterface = nullptr;
	DeferredRenderer->Shutdown();
	DeferredRenderer = nullptr;
}

void ragdoll::Scene::UpdateControls(float _dt)
{
	//manipulate the cube and camera
	struct Data {
		Vector3 cameraPos = { 0.f, 1.f, 5.f };
		float cameraYaw = DirectX::XM_PI;
		float cameraPitch = 0.f;
		float cameraFov = 90.f;
		float cameraNear = 0.01f;
		float cameraFar = 1000.f;
		float cameraWidth = 16.f;
		float cameraHeight = 9.f;
		float cameraSpeed = 5.f;
		float cameraRotationSpeed = 15.f;
		Color dirLightColor = { 1.f,1.f,1.f,1.f };
		Color ambientLight = { 0.2f, 0.2f, 0.2f, 1.f };
		float lightIntensity = 1.f;
		Vector2 azimuthAndElevation = { 0.f, 90.f };
	};
	static Data data;
	ImGui::Begin("Camera Manipulate");
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera FOV (Degrees)", &data.cameraFov, 60.f, 120.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Near", &data.cameraNear, 0.01f, 1.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Far", &data.cameraFar, 10.f, 10000.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Width", &data.cameraWidth, 0.01f, 30.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Height", &data.cameraHeight, 0.01f, 20.f) : true;
	ImGui::SliderFloat("Camera Speed", &data.cameraSpeed, 0.01f, 30.f);
	ImGui::SliderFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 5.f, 100.f);
	ImGui::ColorEdit3("Light Diffuse", &data.dirLightColor.x);
	ImGui::SliderFloat("Light Intensity", &data.lightIntensity, 0.1f, 10.f);
	ImGui::ColorEdit3("Ambient Light Diffuse", &data.ambientLight.x);
	if (ImGui::SliderFloat("Azimuth (Degrees)", &data.azimuthAndElevation.x, 0.f, 360.f))
	{
		UpdateShadowCascadesExtents();
		BuildStaticCascadeMapInstances();
		UpdateShadowLightMatrices();
		BuildDebugInstances(StaticDebugInstanceDatas);
	}
	if(ImGui::SliderFloat("Elevation (Degrees)", &data.azimuthAndElevation.y, 0.01f, 90.f))
	{
		UpdateShadowCascadesExtents();
		BuildStaticCascadeMapInstances();
		UpdateShadowLightMatrices();
		BuildDebugInstances(StaticDebugInstanceDatas);
	}
	ImGui::End();

	SceneInfo.CameraFov = data.cameraFov;
	SceneInfo.CameraAspect = data.cameraWidth / data.cameraHeight;
	SceneInfo.CameraNear = data.cameraNear;

	//make a infinite z inverse projection matrix
	float e = 1 / tanf(DirectX::XMConvertToRadians(data.cameraFov) / 2.f);
	SceneInfo.InfiniteReverseZProj._11 = e;
	SceneInfo.InfiniteReverseZProj._22 = e * (data.cameraWidth / data.cameraHeight);
	SceneInfo.InfiniteReverseZProj._33 = 0.f;
	SceneInfo.InfiniteReverseZProj._44 = 0.f;
	SceneInfo.InfiniteReverseZProj._43 = data.cameraNear;
	SceneInfo.InfiniteReverseZProj._34 = 1.f;
	if (!bFreezeFrustumCulling)
		CameraProjection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(data.cameraFov), data.cameraWidth / data.cameraHeight, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));

	//hardcoded handling of movement now
	if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
		{
			bIsCameraDirty = true;
			data.cameraPos += cameraDir * data.cameraSpeed * PrimaryWindowRef->GetFrameTime();
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
		{
			bIsCameraDirty = true;
			data.cameraPos -= cameraDir * data.cameraSpeed * PrimaryWindowRef->GetFrameTime();
		}
		Vector3 cameraRight = cameraDir.Cross(Vector3(0.f, 1.f, 0.f));
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
		{
			bIsCameraDirty = true;
			data.cameraPos += cameraRight * data.cameraSpeed * PrimaryWindowRef->GetFrameTime();
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
		{
			bIsCameraDirty = true;
			data.cameraPos -= cameraRight * data.cameraSpeed * PrimaryWindowRef->GetFrameTime();
		}
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			bIsCameraDirty = true;
			auto& io = ImGui::GetIO();
			data.cameraYaw += io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindowRef->GetFrameTime();
			data.cameraPitch += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindowRef->GetFrameTime();
			data.cameraPitch = data.cameraPitch > DirectX::XM_PIDIV2 - 0.1f ? DirectX::XM_PIDIV2 - 0.1f : data.cameraPitch;
			data.cameraPitch = data.cameraPitch < -DirectX::XM_PIDIV2 + 0.1f ? -DirectX::XM_PIDIV2 + 0.1f : data.cameraPitch;
		}
	}
	cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));

	SceneInfo.MainCameraView = DirectX::XMMatrixLookAtLH(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	if (!bFreezeFrustumCulling)
		CameraView = SceneInfo.MainCameraView;
	SceneInfo.MainCameraViewProj = SceneInfo.MainCameraView * SceneInfo.InfiniteReverseZProj;
	if (!bFreezeFrustumCulling)
		CameraViewProjection = SceneInfo.MainCameraViewProj;
	SceneInfo.SceneAmbientColor = data.ambientLight;
	SceneInfo.LightDiffuseColor = data.dirLightColor;
	SceneInfo.LightIntensity = data.lightIntensity;
	Vector2 azimuthElevationRad = {
		DirectX::XMConvertToRadians(data.azimuthAndElevation.x),
		DirectX::XMConvertToRadians(data.azimuthAndElevation.y) };
	SceneInfo.LightDirection = Vector3(
		cosf(azimuthElevationRad.y) * sinf(azimuthElevationRad.x),
		sinf(azimuthElevationRad.y),
		cosf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x)
	);
	SceneInfo.LightDirection.Normalize();
	SceneInfo.MainCameraPosition = data.cameraPos;
}

void ragdoll::Scene::CreateCustomMeshes()
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
		nvrhi::TextureHandle texHandle = DirectXDevice::GetNativeDevice()->createTexture(textureDesc);
		CommandList->writeTexture(texHandle, 0, 0, &colors[i], 4);
		img.TextureHandle = texHandle;
		AssetManager::GetInstance()->AddImage(img);

		Texture texture;
		texture.ImageIndex = i;
		texture.SamplerIndex = i;
		AssetManager::GetInstance()->Textures.emplace_back(texture);

	}
	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);

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
	geomBuilder.Init(DirectXDevice::GetNativeDevice());
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
	AssetManager::GetInstance()->UpdateVBOIBO();
}

void ragdoll::Scene::CreateRenderTargets()
{
	//check if the depth buffer i want is already in the asset manager
	nvrhi::TextureDesc depthBufferDesc;
	depthBufferDesc.width = PrimaryWindowRef->GetBufferWidth();
	depthBufferDesc.height = PrimaryWindowRef->GetBufferHeight();
	depthBufferDesc.initialState = nvrhi::ResourceStates::DepthWrite;
	depthBufferDesc.isRenderTarget = true;
	depthBufferDesc.sampleCount = 1;
	depthBufferDesc.dimension = nvrhi::TextureDimension::Texture2D;
	depthBufferDesc.keepInitialState = true;
	depthBufferDesc.mipLevels = 1;
	depthBufferDesc.format = nvrhi::Format::D32;
	depthBufferDesc.isTypeless = true;
	if (!SceneDepthZ) {
		//create a depth buffer
		depthBufferDesc.debugName = "SceneDepthZ";
		SceneDepthZ = DirectXDevice::GetNativeDevice()->createTexture(depthBufferDesc);
	}
	if (!ShadowMap[0]) {
		depthBufferDesc.width = 2000;
		depthBufferDesc.height = 2000;
		for (int i = 0; i < 4; ++i)
		{
			depthBufferDesc.debugName = "ShadowMap" + std::to_string(i);
			ShadowMap[i] = DirectXDevice::GetNativeDevice()->createTexture(depthBufferDesc);
		}
	}

	//create the gbuffer stuff
	nvrhi::TextureDesc texDesc;
	texDesc.width = PrimaryWindowRef->GetBufferWidth();
	texDesc.height = PrimaryWindowRef->GetBufferHeight();
	texDesc.initialState = nvrhi::ResourceStates::RenderTarget;
	texDesc.clearValue = 0.f;
	texDesc.useClearValue = true;
	texDesc.isRenderTarget = true;
	texDesc.sampleCount = 1;
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.keepInitialState = true;
	texDesc.mipLevels = 1;
	texDesc.isTypeless = false;

	if (!SceneColor) {
		texDesc.format = nvrhi::Format::R11G11B10_FLOAT;
		texDesc.debugName = "SceneColor";
		SceneColor = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	if (!GBufferAlbedo) {
		texDesc.format = nvrhi::Format::RGBA8_UNORM;
		texDesc.debugName = "GBufferAlbedo";
		GBufferAlbedo = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	if (!GBufferNormal) {
		texDesc.format = nvrhi::Format::RG16_UNORM;
		texDesc.debugName = "GBufferNormal";
		GBufferNormal = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	if (!GBufferORM) {
		texDesc.format = nvrhi::Format::RGBA8_UNORM;
		texDesc.debugName = "GBufferORM";
		GBufferORM = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	if (!ShadowMask) {
		texDesc.format = nvrhi::Format::RGBA8_UNORM;
		texDesc.debugName = "ShadowMask";
		ShadowMask = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	if (!SkyThetaGammaTable) {
		texDesc.width = 64;
		texDesc.height = 2;
		texDesc.format = nvrhi::Format::RGBA8_UNORM;
		texDesc.debugName = "SkyThetaGammaTable";
		SkyThetaGammaTable = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	if (!SkyTexture) {
		texDesc.width = texDesc.height = 2000;
		texDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
		texDesc.isUAV = true;
		texDesc.format = nvrhi::Format::R11G11B10_FLOAT;
		texDesc.debugName = "SkyTexture";
		SkyTexture = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	}
	uint32_t width = PrimaryWindowRef->GetWidth();
	uint32_t height = PrimaryWindowRef->GetHeight();
	for (int i = 0; i < MipCount; ++i) {
		BloomMip& mip = DownsampledImages.emplace_back();
		mip.Width = width; mip.Height = height;
		nvrhi::TextureDesc desc;
		desc.width = mip.Width;
		desc.height = mip.Height;
		desc.format = nvrhi::Format::R11G11B10_FLOAT;
		desc.initialState = nvrhi::ResourceStates::RenderTarget;
		desc.keepInitialState = true;
		desc.isRenderTarget = true;
		desc.debugName = "DownSample" + std::to_string(mip.Width) + "x" + std::to_string(mip.Height);
		mip.Image = DirectXDevice::GetNativeDevice()->createTexture(desc);
		width /= 2; height /= 2;
	}
	texDesc = nvrhi::TextureDesc();
	texDesc.width = PrimaryWindowRef->GetWidth() / 2 + PrimaryWindowRef->GetWidth() % 2;
	texDesc.height = PrimaryWindowRef->GetHeight() / 2 + PrimaryWindowRef->GetHeight() % 2;
	texDesc.format = nvrhi::Format::R16_FLOAT;
	texDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	texDesc.isUAV = true;
	texDesc.keepInitialState = true;
	texDesc.debugName = "DeinterleavedDepth";
	texDesc.dimension = nvrhi::TextureDimension::Texture2DArray;
	texDesc.arraySize = 4;
	texDesc.mipLevels = 4;
	DeinterleavedDepth = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA8_SNORM;
	texDesc.debugName = "DeinterleavedNormals";
	texDesc.mipLevels = 1;
	DeinterleavedNormals = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RG8_UNORM;
	texDesc.debugName = "SSAOBufferPong";
	SSAOBufferPong = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "SSAOBufferPing";
	SSAOBufferPing = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "ImportanceMap";
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.arraySize = 1;
	texDesc.width = PrimaryWindowRef->GetWidth() / 4 + PrimaryWindowRef->GetWidth() % 4;
	texDesc.height = PrimaryWindowRef->GetHeight() / 4 + PrimaryWindowRef->GetHeight() % 4;
	ImportanceMap = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "ImportanceMapPong";
	ImportanceMapPong = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R32_UINT;
	texDesc.debugName = "LoadCounter";
	texDesc.width = texDesc.height = 1;
	texDesc.dimension = nvrhi::TextureDimension::Texture1D;
	LoadCounter = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
}

void ragdoll::Scene::UpdateTransforms()
{
	TraverseTreeAndUpdateTransforms();
}

void ragdoll::Scene::ResetTransformDirtyFlags()
{
	auto EcsView = EntityManagerRef->GetRegistry().view<TransformComp>();
	for (const entt::entity& ent : EcsView) {
		TransformComp* comp = EntityManagerRef->GetComponent<TransformComp>(ent);
		comp->m_Dirty = false;
	}
}

void ragdoll::Scene::AddEntityAtRootLevel(Guid entityId)
{
	if (m_RootEntity.m_RawId == 0)
		m_RootEntity = entityId;
	else {
		if (m_RootSibling.m_RawId == 0)
		{
			m_RootSibling = entityId;
			m_FurthestSibling = entityId;
		}
		else
		{
			TransformComp* furthestTrans = EntityManagerRef->GetComponent<TransformComp>(m_FurthestSibling);
			furthestTrans->m_Sibling = entityId;
			m_FurthestSibling = entityId;
		}
	}
}

void ragdoll::Scene::PopulateStaticProxies()
{
	//assuming the world transform has already been updated
	StaticOctree.Clear();
	//iterate through all the transforms and renderable
	auto EcsView = EntityManagerRef->GetRegistry().view<RenderableComp, TransformComp>();
	for (const entt::entity& ent : EcsView) {
		TransformComp* tComp = EntityManagerRef->GetComponent<TransformComp>(ent);
		RenderableComp* rComp = EntityManagerRef->GetComponent<RenderableComp>(ent);
		Mesh mesh = AssetManager::GetInstance()->Meshes[rComp->meshIndex];
		for (const Submesh& submesh : mesh.Submeshes)
		{
			StaticProxies.emplace_back();
			Proxy& Proxy = StaticProxies.back();
			const Material& mat = AssetManager::GetInstance()->Materials[submesh.MaterialIndex];
			if (mat.AlbedoTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.AlbedoTextureIndex];
				Proxy.AlbedoIndex = tex.ImageIndex;
				Proxy.AlbedoSamplerIndex = tex.SamplerIndex;
			}
			if (mat.NormalTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.NormalTextureIndex];
				Proxy.NormalIndex = tex.ImageIndex;
				Proxy.NormalSamplerIndex = tex.SamplerIndex;
			}
			if (mat.RoughnessMetallicTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.RoughnessMetallicTextureIndex];
				Proxy.ORMIndex = tex.ImageIndex;
				Proxy.ORMSamplerIndex = tex.SamplerIndex;
			}
			Proxy.Color = mat.Color;
			Proxy.Metallic = mat.Metallic;
			Proxy.Roughness = mat.Roughness;
			Proxy.bIsLit = mat.bIsLit;
			Proxy.ModelToWorld = tComp->m_ModelToWorld;
			Proxy.InvModelToWorld = tComp->m_ModelToWorld.Invert();
			Proxy.BufferIndex = submesh.VertexBufferIndex;
			Proxy.MaterialIndex = submesh.MaterialIndex;
			AssetManager::GetInstance()->VertexBufferInfos[Proxy.BufferIndex].BestFitBox.Transform(Proxy.BoundingBox, tComp->m_ModelToWorld);
			StaticOctree.AddProxy(Proxy.BoundingBox, StaticProxies.size() - 1);
		}
	}
}

void ragdoll::Scene::BuildStaticInstances(const Matrix& cameraProjection, const Matrix& cameraView, std::vector<InstanceData>& instances, std::vector<InstanceGroupInfo>& instancesGrpInfo)
{
	MICROPROFILE_SCOPEI("Scene", "Build Static", MP_RED);
	//clear the old information
	instances.clear();
	instancesGrpInfo.clear();

	//cull the octree
	std::vector<uint32_t> result;
	{
		MICROPROFILE_SCOPEI("Scene", "Culling Octree", MP_VIOLETRED1);
		//build the camera frustum
		DirectX::BoundingFrustum frustum;
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, cameraProjection);
		frustum.Transform(frustum, cameraView.Invert());
		CullOctant(StaticOctree.Octant, frustum, result);
	}
	if (result.empty())	//all culled
		return;
	//sort the results
	std::sort(result.begin(), result.end(), [&](const uint32_t& lhs, const uint32_t& rhs) {
		return StaticProxies[lhs].BufferIndex != StaticProxies[rhs].BufferIndex ? StaticProxies[lhs].BufferIndex < StaticProxies[rhs].BufferIndex : StaticProxies[lhs].MaterialIndex < StaticProxies[rhs].MaterialIndex;
		});
	//build the structured buffer
	int32_t CurrBufferIndex{ -1 };
	CurrBufferIndex = StaticProxies[result[0]].BufferIndex;

	InstanceGroupInfo info;
	info.VertexBufferIndex = CurrBufferIndex;
	info.InstanceCount = 0;
	{
		MICROPROFILE_SCOPEI("Scene", "Building instance buffer", MP_PALEVIOLETRED);
		for (int i = 0; i < result.size(); ++i) {
			const Proxy& proxy = StaticProxies[result[i]];
			//iterate till i get a different buffer id, meaning is a diff mesh
			if (proxy.BufferIndex != CurrBufferIndex)
			{
				//add the current info
				instancesGrpInfo.emplace_back(info);
				//reset count
				info.InstanceCount = 0;
				//set new buffer index
				info.VertexBufferIndex = CurrBufferIndex = proxy.BufferIndex;
			}
			//since already sorted by mesh, just add everything in
			//set the instance data
			instances.emplace_back();
			instances.back() = proxy;
			info.InstanceCount++;
		}
	}
	//last info because i reached the end
	instancesGrpInfo.emplace_back(info);

	//if nothing was added because all instances failed the culling test
	if (instances.empty())
	{
		instancesGrpInfo.clear();
	}
	else
	{
		MICROPROFILE_SCOPEI("Scene", "Building Global Instance Buffer", MP_ORANGERED);
		CommandList->open();
		//create the instance buffer handle
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * instances.size();
		InstanceBufferDesc.debugName = "Global instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);

		//copy data over
		CommandList->beginTrackingBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(StaticInstanceBufferHandle, instances.data(), sizeof(InstanceData) * instances.size());
		CommandList->setPermanentBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::ShaderResource);
		CommandList->close();
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	}
}

void ragdoll::Scene::BuildStaticCascadeMapInstances()
{
	//build instance buffers for all 4
	for (int cascadeIndex = 0; cascadeIndex < 4; ++cascadeIndex)
	{
		StaticCascadeInstanceDatas[cascadeIndex].clear();
		StaticCascadeInstanceInfos[cascadeIndex].clear();

		std::vector<uint32_t> result;
		float back{ FLT_MAX }, front{ -FLT_MAX };
		{
			MICROPROFILE_SCOPEI("Scene", "Culling Octree Cascades", MP_VIOLETRED1);
			DirectX::BoundingOrientedBox box;
			Vector3 scale = { SceneInfo.CascadeInfo[cascadeIndex].width, SceneInfo.CascadeInfo[cascadeIndex].height, 1000.f };
			Matrix matrix = Matrix::CreateScale(scale);
			Vector3 forward = -SceneInfo.LightDirection;
			Vector3 worldUp = { 0.f, 1.f, 0.f };
			if (fabsf(forward.Dot(worldUp)) > 0.99f) {
				worldUp = { 1.f, 0.f, 0.f };
			}
			Vector3 right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, forward));
			Vector3 up = DirectX::XMVector3Cross(forward, right);
			Matrix rotationMatrix = DirectX::XMMATRIX(
				right,         // Right vector (x-axis)
				up,            // Up vector (y-axis)
				forward,       // Forward vector (z-axis)
				DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f)
			);
			matrix *= rotationMatrix;
			matrix *= Matrix::CreateTranslation(SceneInfo.CascadeInfo[cascadeIndex].center);
			box.Transform(box, matrix);
			CullOctantForCascade(StaticOctree.Octant, box, result, SceneInfo.CascadeInfo[cascadeIndex].center, -SceneInfo.LightDirection, back, front);
		}
		if (result.empty())	//all culled
			return;
		SceneInfo.CascadeInfo[cascadeIndex].nearZ = back;
		SceneInfo.CascadeInfo[cascadeIndex].farZ = front;
		//sort the results
		std::sort(result.begin(), result.end(), [&](const uint32_t& lhs, const uint32_t& rhs) {
			return StaticProxies[lhs].BufferIndex != StaticProxies[rhs].BufferIndex ? StaticProxies[lhs].BufferIndex < StaticProxies[rhs].BufferIndex : StaticProxies[lhs].MaterialIndex < StaticProxies[rhs].MaterialIndex;
			});
		//build the structured buffer
		int32_t CurrBufferIndex{ -1 };
		CurrBufferIndex = StaticProxies[result[0]].BufferIndex;

		InstanceGroupInfo info;
		info.VertexBufferIndex = CurrBufferIndex;
		info.InstanceCount = 0;
		{
			MICROPROFILE_SCOPEI("Scene", "Building instance buffer", MP_PALEVIOLETRED);
			for (int i = 0; i < result.size(); ++i) {
				const Proxy& proxy = StaticProxies[result[i]];
				//iterate till i get a different buffer id, meaning is a diff mesh
				if (proxy.BufferIndex != CurrBufferIndex)
				{
					//add the current info
					StaticCascadeInstanceInfos[cascadeIndex].emplace_back(info);
					//reset count
					info.InstanceCount = 0;
					//set new buffer index
					info.VertexBufferIndex = CurrBufferIndex = proxy.BufferIndex;
				}
				//since already sorted by mesh, just add everything in
				//set the instance data
				StaticCascadeInstanceDatas[cascadeIndex].emplace_back();
				StaticCascadeInstanceDatas[cascadeIndex].back() = proxy;
				info.InstanceCount++;
			}
		}
		//last info because i reached the end
		StaticCascadeInstanceInfos[cascadeIndex].emplace_back(info);

		//if nothing was added because all instances failed the culling test
		if (StaticCascadeInstanceDatas[cascadeIndex].empty())
		{
			StaticCascadeInstanceInfos[cascadeIndex].clear();
		}
		else
		{
			MICROPROFILE_SCOPEI("Scene", "Building Global Instance Buffer", MP_ORANGERED);
			CommandList->open();
			//create the instance buffer handle
			nvrhi::BufferDesc InstanceBufferDesc;
			InstanceBufferDesc.byteSize = sizeof(InstanceData) * StaticCascadeInstanceDatas[cascadeIndex].size();
			InstanceBufferDesc.debugName = "Global instance buffer";
			InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
			InstanceBufferDesc.structStride = sizeof(InstanceData);
			StaticCascadeInstanceBufferHandles[cascadeIndex] = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);

			//copy data over
			CommandList->beginTrackingBufferState(StaticCascadeInstanceBufferHandles[cascadeIndex], nvrhi::ResourceStates::CopyDest);
			CommandList->writeBuffer(StaticCascadeInstanceBufferHandles[cascadeIndex], StaticCascadeInstanceDatas[cascadeIndex].data(), sizeof(InstanceData) * StaticCascadeInstanceDatas[cascadeIndex].size());
			CommandList->setPermanentBufferState(StaticCascadeInstanceBufferHandles[cascadeIndex], nvrhi::ResourceStates::ShaderResource);
			CommandList->close();
			DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
		}
	}
}

void ragdoll::Scene::BuildDebugInstances(std::vector<InstanceData>& instances)
{
	instances.clear();
	for (int i = 0; i < StaticProxies.size(); ++i) {
		if (Config.bDrawBoxes) {
			InstanceData debugData;
			Vector3 translate = StaticProxies[i].BoundingBox.Center;
			Vector3 scale = StaticProxies[i].BoundingBox.Extents;
			Matrix matrix = Matrix::CreateScale(scale);
			matrix *= Matrix::CreateTranslation(translate);
			debugData.ModelToWorld = matrix;
			debugData.bIsLit = false;
			debugData.Color = { 0.f, 1.f, 0.f, 1.f };
			instances.emplace_back(debugData);
		}
	}

	//adding the octants into the debug instance
	if (Config.bDrawOctree) {
		MICROPROFILE_SCOPEI("Scene", "Building octree debug instances", MP_RED4);
		AddOctantDebug(StaticOctree.Octant, 0);
	}

	if (SceneInfo.EnableCascadeDebug) {
		//draw 4 boxes
		static constexpr Vector4 colors[] = {
			{1.f, 0.f, 0.f, 1.f},
			{1.f, 1.f, 0.f, 1.f},
			{0.f, 1.f, 0.f, 1.f},
			{1.f, 0.f, 1.f, 1.f}
		};
		InstanceData debugData;
		Vector3 scale = { SceneInfo.CascadeInfo[SceneInfo.EnableCascadeDebug - 1].width,
			SceneInfo.CascadeInfo[SceneInfo.EnableCascadeDebug - 1].height,
			abs(SceneInfo.CascadeInfo[SceneInfo.EnableCascadeDebug - 1].farZ - SceneInfo.CascadeInfo[SceneInfo.EnableCascadeDebug - 1].nearZ) };
		Matrix matrix = Matrix::CreateScale(scale);
		DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(-SceneInfo.LightDirection);
		DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f);
		DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, forward));
		DirectX::XMVECTOR up = DirectX::XMVector3Cross(forward, right);
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMATRIX(
			right,         // Right vector (x-axis)
			up,            // Up vector (y-axis)
			forward,       // Forward vector (z-axis)
			DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f)
		);
		matrix *= rotationMatrix;
		matrix *= Matrix::CreateTranslation(SceneInfo.CascadeInfo[SceneInfo.EnableCascadeDebug - 1].center);
		debugData.ModelToWorld = matrix;
		debugData.bIsLit = false;
		debugData.Color = colors[SceneInfo.EnableCascadeDebug - 1];
		instances.emplace_back(debugData);
	}

	if (!instances.empty())
	{
		MICROPROFILE_SCOPEI("Scene", "Building Debug Instance Buffer", MP_DARKRED);
		//add one more cube at where the sun is
		InstanceData debugData;
		Vector3 translate = SceneInfo.LightDirection;
		Vector3 scale = Vector3::One;
		Matrix matrix = Matrix::CreateScale(scale);
		matrix *= Matrix::CreateTranslation(translate);
		debugData.ModelToWorld = matrix;
		debugData.bIsLit = false;
		debugData.Color = { 1.f, 1.f, 1.f, 1.f };
		instances.emplace_back(debugData);

		//create the debug instance buffer
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * instances.size();
		InstanceBufferDesc.debugName = "Debug instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceDebugBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);

		//copy data over
		CommandList->open();
		CommandList->beginTrackingBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(StaticInstanceDebugBufferHandle, instances.data(), sizeof(InstanceData) * instances.size());
		CommandList->setPermanentBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::ShaderResource);
		CommandList->close();
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	}
}

void ragdoll::Scene::CullOctant(Octant& octant, const DirectX::BoundingFrustum& frustum, std::vector<uint32_t>& result)
{
	if (frustum.Contains(octant.Box) != DirectX::ContainmentType::DISJOINT)	//so if contains or intersects
	{
		octant.bIsCulled = false;
		//if no children, all proxies go into the result buffer
		if (octant.Octants.empty())
		{
			for (const Octant::Node& proxy : octant.Nodes) {
				result.emplace_back(proxy.Index);
			}
		}
		else {
			//add all its children first
			for (const Octant::Node& proxy : octant.Nodes) {
				result.emplace_back(proxy.Index);
			}
			for (Octant& itOctant : octant.Octants) {
				CullOctant(itOctant, frustum, result);
			}
		}
	}
	else {
		octant.bIsCulled = false;
		DebugInfo.CulledOctantsCount += 1;
	}
}

void ragdoll::Scene::CullOctantForCascade(const Octant& octant, const DirectX::BoundingOrientedBox& oob, std::vector<uint32_t>& result, const Vector3& center, const Vector3& normal, float& back, float& front)
{
	if (oob.Contains(octant.Box) != DirectX::ContainmentType::DISJOINT)	//so if contains or intersects
	{
		//only if this octant have nodes, then consider min max
		if (!octant.Nodes.empty())
		{
			//calculate the furthest
			Vector3 corners[8];
			octant.Box.GetCorners(corners);
			for (int i = 0; i < 8; ++i) {
				float d2 = (corners[i] - center).Dot(normal);
				front = std::max(d2, front);
				back = std::min(d2, back);
			}
		}
		//if no children, all proxies go into the result buffer
		if (octant.Octants.empty())
		{
			for (const Octant::Node& proxy : octant.Nodes) {
				result.emplace_back(proxy.Index);
			}
		}
		else {
			//add all its children first
			for (const Octant::Node& proxy : octant.Nodes) {
				result.emplace_back(proxy.Index);
			}
			for (const Octant& itOctant : octant.Octants) {
				CullOctantForCascade(itOctant, oob, result, center, normal, back, front);
			}
		}
	}
}

void ragdoll::Scene::UpdateShadowCascadesExtents()
{
	//for each subfrusta
	for (int i = 1; i < 5; ++i)
	{
		//create the subfrusta
		DirectX::BoundingFrustum frustum;
		//no need to be reverse z
		Matrix proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(SceneInfo.CameraFov), SceneInfo.CameraAspect, SubfrustaFarPlanes[i-1], SubfrustaFarPlanes[i]);
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, proj);
		//move the frustum to world space
		frustum.Transform(frustum, SceneInfo.MainCameraView.Invert());
		//get the corners
		Vector3 corners[8];
		frustum.GetCorners(corners);
		//get the new center
		Vector3 center;
		for (int j = 0; j < 8; ++j) {
			center += corners[j];
		}
		center /= 8.f;
		center.x = (int)center.x;
		center.y = (int)center.y;
		center.z = (int)center.z;
		//move all corners into a 1x1x1 cube lightspace with the directional light
		Matrix lightProj = DirectX::XMMatrixOrthographicLH(1.f, 1.f, -0.5f, 0.5f);	//should be a 1x1x1 cube?
		Vector3 lightDir = -SceneInfo.LightDirection;
		Vector3 up = { 0.f, 1.f, 0.f };
		if (fabsf(lightDir.Dot(up)) > 0.99f) {
			up = { 1.f, 0.f, 0.f };
		}
		SceneInfo.CascadeInfo[i - 1].view = DirectX::XMMatrixLookAtLH(center, center + lightDir, up);
		Matrix lightViewProj = SceneInfo.CascadeInfo[i - 1].view * lightProj;
		//get the furthest extents of the corners for the left right top and bottom values
		Vector3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
		Vector3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int j = 0; j < 8; ++j) {
			//bring into lightspace
			corners[j] = Vector3::Transform(corners[j], lightViewProj);
			min.x = std::min(min.x, corners[j].x); max.x = std::max(max.x, corners[j].x);
			min.y = std::min(min.y, corners[j].y); max.y = std::max(max.y, corners[j].y);
			min.z = std::min(min.z, corners[j].z); max.z = std::max(max.z, corners[j].z);
		}
		//get largest width or height to make ortho a square
		float x = max.x - min.x;
		float y = max.y - min.y;
		float length = x < y ? y : x;
		SceneInfo.CascadeInfo[i - 1].center = center;
		SceneInfo.CascadeInfo[i - 1].width = SceneInfo.CascadeInfo[i - 1].height = length;
	}
}

void ragdoll::Scene::UpdateShadowLightMatrices()
{
	for (int i = 0; i < 4; ++i) {
		//reverse z
		SceneInfo.CascadeInfo[i].proj = DirectX::XMMatrixOrthographicLH(SceneInfo.CascadeInfo[i].width, SceneInfo.CascadeInfo[i].height, SceneInfo.CascadeInfo[i].farZ, SceneInfo.CascadeInfo[i].nearZ);
		SceneInfo.CascadeInfo[i].viewProj = SceneInfo.CascadeInfo[i].view * SceneInfo.CascadeInfo[i].proj;
	}
}

void PrintRecursive(ragdoll::Guid id, int level, std::shared_ptr<ragdoll::EntityManager> em)
{
	TransformComp* trans = em->GetComponent<TransformComp>(id);

	std::string nodeString{};
	for (int i = 0; i < level; ++i) {
		nodeString += "\t";
	}
	RD_CORE_INFO("{}", nodeString + std::to_string((uint64_t)trans->glTFId));
	RD_CORE_INFO("{} Position: {}", nodeString, trans->m_LocalPosition);
	RD_CORE_INFO("{} Rotation: {}", nodeString, trans->m_LocalRotation.ToEuler() * 180.f / DirectX::XM_PI);
	RD_CORE_INFO("{} Scale: {}", nodeString, trans->m_LocalScale);

	if (trans->m_Child.m_RawId != 0) {
		PrintRecursive(trans->m_Child, level + 1, em);
	}
	while (trans->m_Sibling.m_RawId != 0)
	{
		trans = em->GetComponent<TransformComp>(trans->m_Sibling);
		RD_CORE_INFO("{}", nodeString + std::to_string((uint64_t)trans->glTFId));
		RD_CORE_INFO("{} Position: {}", nodeString, trans->m_LocalPosition);
		RD_CORE_INFO("{} Rotation: {}", nodeString, trans->m_LocalRotation.ToEuler() * 180.f / DirectX::XM_PI);
		RD_CORE_INFO("{} Scale: {}", nodeString, trans->m_LocalScale);
		if (trans->m_Child)
			PrintRecursive(trans->m_Child, level + 1, em);
	}
}

void ragdoll::Scene::DebugPrintHierarchy()
{
	if (m_RootEntity.m_RawId)
		PrintRecursive(m_RootEntity, 0, EntityManagerRef);
	if (m_RootSibling.m_RawId)
		PrintRecursive(m_RootSibling, 0, EntityManagerRef);
}

void ragdoll::Scene::TraverseTreeAndUpdateTransforms()
{
	if (m_RootEntity)
	{
		TraverseNode(m_RootEntity);
		if (m_RootSibling)
			TraverseNode(m_RootSibling);
	}
}

void ragdoll::Scene::TraverseNode(const Guid& guid)
{
	TransformComp* transform = EntityManagerRef->GetComponent<TransformComp>(guid);
	if (transform)
	{
		UpdateTransform(*transform, guid);

		//traverse siblings with a for loop
		while (transform->m_Sibling.m_RawId != 0) {
			transform = EntityManagerRef->GetComponent<TransformComp>(transform->m_Sibling);
			UpdateTransform(*transform, guid);
		}
	}
	else
	{
		RD_CORE_ERROR("Entity {} is a child despite not being a transform object", guid);
	}
}

Matrix ragdoll::Scene::GetLocalModelMatrix(const TransformComp& trans)
{
	Matrix model = Matrix::Identity;
	model *= Matrix::CreateScale(trans.m_LocalScale);
	model *= Matrix::CreateFromQuaternion(trans.m_LocalRotation);
	model *= Matrix::CreateTranslation(trans.m_LocalPosition);
	return model;
}

void ragdoll::Scene::UpdateTransform(TransformComp& comp, const Guid& guid)
{
	//check if state is dirty
	bool dirtyFromHere = false;
	if (!m_DirtyOnwards)
	{
		if (comp.m_Dirty)
		{
			m_DirtyOnwards = true;
			dirtyFromHere = true;
		}
	}
	if (m_DirtyOnwards)
	{
		comp.m_Dirty = true;
		//get local matrix
		auto localModel = GetLocalModelMatrix(comp);
		//add to model stack
		if (m_ModelStack.empty())	//first matrix will be this guy local
			m_ModelStack.push(localModel);
		else
			m_ModelStack.push(localModel * m_ModelStack.top());	//otherwise concatenate matrix in stacks
		comp.m_ModelToWorld = m_ModelStack.top();	//set model matrix to the top of the stack
	}
	else
		m_ModelStack.push(comp.m_ModelToWorld);	//if not dirty, just push the current model matrix
	//traverse children
	if (comp.m_Child)
		TraverseNode(comp.m_Child);

	//once done, pop the stack
	m_ModelStack.pop();
	//resets the dirty flags
	if (dirtyFromHere)
		m_DirtyOnwards = false;
}

void ragdoll::Scene::AddOctantDebug(const Octant& octant, uint32_t level)
{
	if (octant.bIsCulled)
		return;
	static constexpr Vector4 colors[] = {
		 {1.0f, 0.0f, 0.0f, 1.0f},  // Red
		{0.0f, 0.0f, 1.0f, 1.0f},  // Blue
		{1.0f, 1.0f, 0.0f, 1.0f},  // Yellow
		{1.0f, 0.0f, 1.0f, 1.0f},  // Magenta
		{0.0f, 1.0f, 1.0f, 1.0f},  // Cyan
		{1.0f, 0.5f, 0.0f, 1.0f},  // Orange
		{0.5f, 0.0f, 0.5f, 1.0f},  // Purple
		{0.5f, 1.0f, 0.5f, 1.0f},  // Light Green
		{0.5f, 0.5f, 1.0f, 1.0f},  // Lavender
		{0.75f, 0.25f, 0.5f, 1.0f},// Pink
		{0.25f, 0.5f, 0.75f, 1.0f},// Steel Blue
		{0.8f, 0.7f, 0.6f, 1.0f},  // Beige
		{0.6f, 0.4f, 0.2f, 1.0f},  // Brown
		{0.9f, 0.9f, 0.9f, 1.0f},  // Light Grey
		{0.4f, 0.4f, 0.4f, 1.0f},  // Dark Grey
		{0.7f, 0.3f, 0.3f, 1.0f},  // Salmon
		{0.2f, 0.6f, 0.2f, 1.0f},  // Dark Green
		{0.3f, 0.7f, 0.8f, 1.0f},  // Sky Blue
		{0.9f, 0.4f, 0.2f, 1.0f},  // Coral
		{0.5f, 0.5f, 0.0f, 1.0f},  // Olive
		{0.6f, 0.2f, 0.8f, 1.0f},  // Violet
		{0.2f, 0.2f, 0.6f, 1.0f},  // Navy Blue
		{0.4f, 0.2f, 0.1f, 1.0f},  // Chocolate
		{0.7f, 0.7f, 0.0f, 1.0f},  // Mustard
		{0.2f, 0.7f, 0.5f, 1.0f},  // Teal
		{0.9f, 0.7f, 0.5f, 1.0f},  // Peach
		{0.7f, 0.4f, 0.4f, 1.0f},  // Light Brown
		{0.5f, 0.8f, 0.6f, 1.0f},  // Mint
		{0.6f, 0.6f, 1.0f, 1.0f},  // Light Blue
		{0.9f, 0.3f, 0.3f, 1.0f},  // Crimson
		{0.3f, 0.9f, 0.6f, 1.0f}   // Aquamarine
	};
	if (level >= Config.DrawOctreeLevelMin && level <= Config.DrawOctreeLevelMax)
	{
		InstanceData debugData;
		Vector3 translate = octant.Box.Center;
		Vector3 scale = octant.Box.Extents;
		Matrix matrix = Matrix::CreateScale(scale);
		matrix *= Matrix::CreateTranslation(translate);
		debugData.ModelToWorld = matrix;
		debugData.bIsLit = false;
		debugData.Color = colors[level];
		StaticDebugInstanceDatas.emplace_back(debugData);
	}
	for (const Octant& itOctant : octant.Octants)
	{
		AddOctantDebug(itOctant, level + 1);
	}
}
