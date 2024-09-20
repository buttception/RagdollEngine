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
	DeviceRef = app->Device;

	CommandList = DeviceRef->m_NvrhiDevice->createCommandList();
	//TODO: create the cmd args renderer
	ForwardRenderer = std::make_shared<class ForwardRenderer>();
	ForwardRenderer->Init(DeviceRef, app->m_PrimaryWindow);
	DeferredRenderer = std::make_shared<class DeferredRenderer>();
	DeferredRenderer->Init(DeviceRef, app->m_PrimaryWindow);
	if (app->Config.bCreateCustomMeshes)
	{
		Config.bIsThereCustomMeshes = true;
		CreateCustomMeshes();
	}
	Config.bDrawBoxes = app->Config.bDrawDebugBoundingBoxes;
	Config.bDrawOctree = app->Config.bDrawDebugOctree;
	ImguiInterface = std::make_shared<ImguiRenderer>();
	ImguiInterface->Init(DeviceRef.get());
	StaticOctree.Init();
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
	if (ImGui::Checkbox("Freeze Culling Matrix", &bFreezeFrustumCulling))
		bIsCameraDirty = true;
	if (ImGui::Checkbox("Show Octree", &Config.bDrawOctree))
		bIsCameraDirty = true;
	if (ImGui::Checkbox("Show Boxes", &Config.bDrawBoxes))
		bIsCameraDirty = true;
	ImGui::Text("%d entities count", EntityManagerRef->GetRegistry().view<entt::entity>().size_hint());
	ImGui::Text("%d proxies to draw", StaticProxiesToDraw.size());
	ImGui::Text("%d instance count", StaticInstanceDatas.size());
	ImGui::Text("%d octants culled", DebugInfo.CulledObjectCount);
	ImGui::Text("%d proxies in octree", Octree::TotalProxies);
	ImGui::End();

	if (bIsCameraDirty)
	{
		DebugInfo.CulledObjectCount = 0;
		BuildStaticInstances(CameraProjection, CameraView);
	}

	ForwardRenderer->Render(this);
	//DeferredRenderer->Render(this);

	ImguiInterface->Render();
	DeviceRef->Present();

	bIsCameraDirty = false;
}

void ragdoll::Scene::Shutdown()
{
	ImguiInterface->Shutdown();
	ImguiInterface = nullptr;
	ForwardRenderer->Shutdown();
	ForwardRenderer = nullptr;
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
		float ambientIntensity = 0.2f;
		Vector2 azimuthAndElevation = { 0.f, 45.f };
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
	ImGui::ColorEdit3("Ambient Light Diffuse", &data.ambientLight.x);
	ImGui::SliderFloat("Azimuth (Degrees)", &data.azimuthAndElevation.x, 0.f, 360.f);
	ImGui::SliderFloat("Elevation (Degrees)", &data.azimuthAndElevation.y, -90.f, 90.f);
	ImGui::End();

	//make a infinite z inverse projection matrix
	Matrix infiniteProj;
	float e = 1 / tanf(DirectX::XMConvertToRadians(data.cameraFov) / 2.f);
	infiniteProj._11 = e;
	infiniteProj._22 = e * (data.cameraWidth / data.cameraHeight);
	infiniteProj._33 = 0.f;
	infiniteProj._44 = 0.f;
	infiniteProj._43 = data.cameraNear;
	infiniteProj._34 = 1.f;
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

	Matrix view = DirectX::XMMatrixLookAtLH(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	if (!bFreezeFrustumCulling)
		CameraView = view;
	SceneInfo.MainCameraViewProj = view * infiniteProj;
	if (!bFreezeFrustumCulling)
		CameraViewProjection = SceneInfo.MainCameraViewProj;
	SceneInfo.SceneAmbientColor = data.ambientLight;
	SceneInfo.LightDiffuseColor = data.dirLightColor;
	Vector2 azimuthElevationRad = {
		DirectX::XMConvertToRadians(data.azimuthAndElevation.x),
		DirectX::XMConvertToRadians(data.azimuthAndElevation.y) };
	SceneInfo.LightDirection = Vector3(
		sinf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x),
		cosf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x),
		sinf(azimuthElevationRad.x));
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
		nvrhi::TextureHandle texHandle = DeviceRef->m_NvrhiDevice->createTexture(textureDesc);
		CommandList->writeTexture(texHandle, 0, 0, &colors[i], 4);
		img.TextureHandle = texHandle;
		AssetManager::GetInstance()->AddImage(img);

		Texture texture;
		texture.ImageIndex = i;
		texture.SamplerIndex = i;
		AssetManager::GetInstance()->Textures.emplace_back(texture);

	}
	CommandList->close();
	DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);

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
	geomBuilder.Init(DeviceRef->m_NvrhiDevice);
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
	StaticOctree.Clear();
	//iterate through all the transforms and renderable
	auto EcsView = EntityManagerRef->GetRegistry().view<RenderableComp, TransformComp>();
	for (const entt::entity& ent : EcsView) {
		TransformComp* tComp = EntityManagerRef->GetComponent<TransformComp>(ent);
		RenderableComp* rComp = EntityManagerRef->GetComponent<RenderableComp>(ent);
		Mesh mesh = AssetManager::GetInstance()->Meshes[rComp->meshIndex];
		for (const Submesh& submesh : mesh.Submeshes)
		{
			Proxy Proxy;
			Proxy.EnttId = (ENTT_ID_TYPE)ent;
			//temp only first submesh now
			Proxy.BufferIndex = submesh.VertexBufferIndex;
			Proxy.MaterialIndex = submesh.MaterialIndex;
			//bounding box will be empty for now
			AssetManager::GetInstance()->VertexBufferInfos[Proxy.BufferIndex].BestFitBox.Transform(Proxy.BoundingBox, tComp->m_ModelToWorld);
			StaticOctree.AddProxy(Proxy);
		}
	}
}

void ragdoll::Scene::BuildStaticInstances(const Matrix& cameraProjection, const Matrix& cameraView)
{
	MICROPROFILE_SCOPEI("Scene", "Build Static", MP_RED);
	//clear the old information
	StaticInstanceGroupInfos.clear();
	StaticInstanceDatas.clear();
	StaticDebugInstanceDatas.clear();

	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, cameraProjection);
	frustum.Transform(frustum, cameraView.Invert());

	//cull the octree
	StaticProxiesToDraw.clear();
	{
		MICROPROFILE_SCOPEI("Scene", "Culling Octree", MP_VIOLETRED1);
		CullOctant(StaticOctree.Octant, frustum);
	}
	if (StaticProxiesToDraw.empty())	//if got nothing to draw just return
		return;
	//sort
	std::sort(StaticProxiesToDraw.begin(), StaticProxiesToDraw.end(), [](const Proxy& lhs, const Proxy& rhs) {
		return lhs.BufferIndex != rhs.BufferIndex ? lhs.BufferIndex < rhs.BufferIndex : lhs.MaterialIndex < rhs.MaterialIndex;
		});
	//build the structured buffer
	int32_t CurrBufferIndex{ -1 };
	CurrBufferIndex = StaticProxiesToDraw[0].BufferIndex;

	InstanceGroupInfo info;
	info.VertexBufferIndex = CurrBufferIndex;
	info.InstanceCount = 0;
	{
		MICROPROFILE_SCOPEI("Scene", "Building instance buffer", MP_PALEVIOLETRED);
		for (int i = 0; i < StaticProxiesToDraw.size(); ++i) {
			//iterate till i get a different buffer id, meaning is a diff mesh
			if (StaticProxiesToDraw[i].BufferIndex != CurrBufferIndex)
			{
				//add the current info
				StaticInstanceGroupInfos.emplace_back(info);
				//reset count
				info.InstanceCount = 0;
				//set new buffer index
				info.VertexBufferIndex = CurrBufferIndex = StaticProxiesToDraw[i].BufferIndex;
			}
			//since already sorted by mesh, just add everything in
			//set the instance data
			TransformComp* tComp = EntityManagerRef->GetComponent<TransformComp>((entt::entity)StaticProxiesToDraw[i].EnttId);
			InstanceData Data;

			const Material& mat = AssetManager::GetInstance()->Materials[StaticProxiesToDraw[i].MaterialIndex];
			if (mat.AlbedoTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.AlbedoTextureIndex];
				Data.AlbedoIndex = tex.ImageIndex;
				Data.AlbedoSamplerIndex = tex.SamplerIndex;
			}
			if (mat.NormalTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.NormalTextureIndex];
				Data.NormalIndex = tex.ImageIndex;
				Data.NormalSamplerIndex = tex.SamplerIndex;
			}
			if (mat.RoughnessMetallicTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.RoughnessMetallicTextureIndex];
				Data.RoughnessMetallicIndex = tex.ImageIndex;
				Data.RoughnessMetallicSamplerIndex = tex.SamplerIndex;
			}
			Data.Color = mat.Color;
			Data.Metallic = mat.Metallic;
			Data.Roughness = mat.Roughness;
			Data.bIsLit = mat.bIsLit;
			Data.ModelToWorld = tComp->m_ModelToWorld;
			Data.InvModelToWorld = tComp->m_ModelToWorld.Invert();
			StaticInstanceDatas.emplace_back(Data);
			info.InstanceCount++;

			if (Config.bDrawBoxes) {
				InstanceData debugData;
				Vector3 translate = StaticProxiesToDraw[i].BoundingBox.Center;
				Vector3 scale = StaticProxiesToDraw[i].BoundingBox.Extents * 2;
				Matrix matrix = Matrix::CreateScale(scale);
				matrix *= Matrix::CreateTranslation(translate);
				debugData.ModelToWorld = matrix;
				debugData.bIsLit = false;
				debugData.Color = { 0.f, 1.f, 0.f, 1.f };
				StaticDebugInstanceDatas.emplace_back(debugData);
			}
		}
	}
	//last info because i reached the end
	StaticInstanceGroupInfos.emplace_back(info);
	//adding the octants into the debug instance
	if(Config.bDrawOctree){
		MICROPROFILE_SCOPEI("Scene", "Building octree debug instances", MP_RED4);
		AddOctantDebug(StaticOctree.Octant, 0);
	}

	if (StaticInstanceDatas.empty())
	{
		StaticInstanceGroupInfos.clear();
	}
	else
	{
		MICROPROFILE_SCOPEI("Scene", "Building Global Instance Buffer", MP_ORANGERED);
		CommandList->open();
		//create the instance buffer handle
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * StaticInstanceDatas.size();
		InstanceBufferDesc.debugName = "Global instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceBufferHandle = DeviceRef->m_NvrhiDevice->createBuffer(InstanceBufferDesc);

		//copy data over
		CommandList->beginTrackingBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(StaticInstanceBufferHandle, StaticInstanceDatas.data(), sizeof(InstanceData) * StaticInstanceDatas.size());
		CommandList->setPermanentBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::ShaderResource);
		CommandList->close();
		DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	}
	if (!StaticDebugInstanceDatas.empty())
	{
		MICROPROFILE_SCOPEI("Scene", "Building Debug Instance Buffer", MP_DARKRED);
		//create the debug instance buffer
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * StaticDebugInstanceDatas.size();
		InstanceBufferDesc.debugName = "Debug instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceDebugBufferHandle = DeviceRef->m_NvrhiDevice->createBuffer(InstanceBufferDesc);

		//copy data over
		CommandList->open();
		CommandList->beginTrackingBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(StaticInstanceDebugBufferHandle, StaticDebugInstanceDatas.data(), sizeof(InstanceData) * StaticDebugInstanceDatas.size());
		CommandList->setPermanentBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::ShaderResource);
		CommandList->close();
		DeviceRef->m_NvrhiDevice->executeCommandList(CommandList);
	}
}

void ragdoll::Scene::CullOctant(const Octant& octant, const DirectX::BoundingFrustum& frustum)
{
	if (frustum.Contains(octant.Box) != DirectX::ContainmentType::DISJOINT)	//so if contains or intersects
	{
		//if no children, all proxies go into the instance buffer
		if (octant.Octants.empty())
		{
			for (const Proxy& proxy : octant.Proxies) {
				StaticProxiesToDraw.emplace_back(proxy);
			}
		}
		else {
			//add all its children first
			for (const Proxy& proxy : octant.Proxies) {
				StaticProxiesToDraw.emplace_back(proxy);
			}
			for (const Octant& itOctant : octant.Octants) {
				CullOctant(itOctant, frustum);
			}
		}
	}
	else {
		DebugInfo.CulledObjectCount += 1;
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

void ragdoll::Scene::AddOctantDebug(Octant octant, uint32_t level)
{
	static constexpr Vector4 colors[32] = {
		 {1.0f, 0.0f, 0.0f, 1.0f},  // Red
		{0.0f, 1.0f, 0.0f, 1.0f},  // Green
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
	if (octant.Octants.empty())
	{
		//draw only if no children
		InstanceData debugData;
		Vector3 translate = octant.Box.Center;
		Vector3 scale = octant.Box.Extents * 2;
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
