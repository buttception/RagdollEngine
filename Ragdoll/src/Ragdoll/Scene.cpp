#include "ragdollpch.h"

#include "Scene.h"

#include "Application.h"
#include "Entity/EntityManager.h"
#include "AssetManager.h"
#include "DirectXDevice.h"
#include "nvrhi/utils.h"
#include "microprofile.h"

ragdoll::Scene::Scene(Application* app)
{
	EntityManager = app->m_EntityManager;
	PrimaryWindow = app->m_PrimaryWindow;
	Renderer.Init(app->m_PrimaryWindow, app->m_FileManager, app->m_EntityManager);
	if (app->Config.bCreateCustomMeshes)
	{
		Config.bIsThereCustomMeshes = true;
		Renderer.CreateCustomMeshes();
	}
	ImguiInterface.Init(Renderer.Device.get(), Renderer.ImguiVertexShader, Renderer.ImguiPixelShader);
}

void ragdoll::Scene::Update(float _dt)
{
	ImguiInterface.BeginFrame();
	Renderer.BeginFrame();
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
					ent = EntityManager->CreateEntity();
				}
				{
					MICROPROFILE_SCOPEI("Creation", "Setting Transform", MP_GREENYELLOW);
					auto tcomp = EntityManager->AddComponent<TransformComp>(ent);
					tcomp->m_LocalPosition = pos;
					tcomp->m_LocalScale = scale;
					tcomp->m_LocalRotation = Quaternion::CreateFromYawPitchRoll(eulerRotate.y, eulerRotate.x, eulerRotate.z);
					{
						MICROPROFILE_SCOPEI("Creation", "Adding at Root", MP_DARKOLIVEGREEN);
						AddEntityAtRootLevel(EntityManager->GetGuid(ent));
					}
				}
				{
					MICROPROFILE_SCOPEI("Creation", "Setting Renderable", MP_FORESTGREEN);
					auto rcomp = EntityManager->AddComponent<RenderableComp>(ent);
					rcomp->meshIndex = std::rand() / (float)RAND_MAX * 25;
				}
			}
			{
				MICROPROFILE_SCOPEI("Creation", "Entity Update", MP_DARKSEAGREEN);
				PopulateStaticProxies();	//create all the proxies to iterate
				UpdateTransforms();	//update all the dirty transforms
				UpdateStaticProxies();	//update all the bounding box in the proxies
				ResetTransformDirtyFlags();	//reset the dirty flags
				bIsCameraDirty = true;
			}
			MicroProfileDumpFileImmediately("test.html", nullptr, nullptr);
		}
		ImGui::End();
	}

	if (bIsCameraDirty)
		BuildStaticInstances(CameraViewProjection, CameraView);
	Renderer.DrawAllInstances(StaticInstanceBufferHandle, StaticInstanceGroupInfos, CBuffer);
	Renderer.DrawBoundingBoxes(StaticInstanceDebugBufferHandle, StaticDebugInstanceDatas.size(), CBuffer);

	ImGui::Begin("Debug");
	ImGui::Text("%d culled", DebugInfo.CulledObjectCount);
	ImGui::End();

	ImguiInterface.Render();
	Renderer.Device->Present();

	DebugInfo.CulledObjectCount = 0;
	bIsCameraDirty = false;
}

void ragdoll::Scene::Shutdown()
{
	Renderer.Shutdown();
}

void ragdoll::Scene::UpdateControls(float _dt)
{
	//manipulate the cube and camera
	struct Data {
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
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera FOV (Degrees)", &data.cameraFov, 60.f, 120.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Near", &data.cameraNear, 0.01f, 1.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Far", &data.cameraFar, 10.f, 10000.f) : true;
	bIsCameraDirty = !bIsCameraDirty ? ImGui::SliderFloat("Camera Aspect Ratio", &data.cameraAspect, 0.01f, 5.f) : true;
	ImGui::SliderFloat("Camera Speed", &data.cameraSpeed, 0.01f, 30.f);
	ImGui::SliderFloat("Camera Rotation Speed (Degrees)", &data.cameraRotationSpeed, 5.f, 100.f);
	ImGui::ColorEdit3("Light Diffuse", &data.dirLightColor.x);
	ImGui::ColorEdit3("Ambient Light Diffuse", &data.ambientLight.x);
	ImGui::SliderFloat("Azimuth (Degrees)", &data.azimuthAndElevation.x, 0.f, 360.f);
	ImGui::SliderFloat("Elevation (Degrees)", &data.azimuthAndElevation.y, -90.f, 90.f);
	ImGui::End();

	Matrix proj = Matrix::CreatePerspectiveFieldOfView(DirectX::XMConvertToRadians(data.cameraFov), data.cameraAspect, data.cameraNear, data.cameraFar);
	Vector3 cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));

	//hardcoded handling of movement now
	if (!ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemActive()) {
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_W))
		{
			bIsCameraDirty = true;
			data.cameraPos += cameraDir * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_S))
		{
			bIsCameraDirty = true;
			data.cameraPos -= cameraDir * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		}
		Vector3 cameraRight = -cameraDir.Cross(Vector3(0.f, 1.f, 0.f));
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_A))
		{
			bIsCameraDirty = true;
			data.cameraPos += cameraRight * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		}
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_D))
		{
			bIsCameraDirty = true;
			data.cameraPos -= cameraRight * data.cameraSpeed * PrimaryWindow->GetFrameTime();
		}
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			bIsCameraDirty = true;
			auto& io = ImGui::GetIO();
			data.cameraYaw -= io.MouseDelta.x * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
			data.cameraPitch += io.MouseDelta.y * DirectX::XMConvertToRadians(data.cameraRotationSpeed) * PrimaryWindow->GetFrameTime();
		}
	}
	cameraDir = Vector3::Transform(Vector3(0.f, 0.f, 1.f), Quaternion::CreateFromYawPitchRoll(data.cameraYaw, data.cameraPitch, 0.f));

	CameraView = Matrix::CreateLookAt(data.cameraPos, data.cameraPos + cameraDir, Vector3(0.f, 1.f, 0.f));
	CBuffer.viewProj = CameraView * proj;
	CameraViewProjection = CBuffer.viewProj;
	CBuffer.sceneAmbientColor = data.ambientLight;
	CBuffer.lightDiffuseColor = data.dirLightColor;
	Vector2 azimuthElevationRad = {
		DirectX::XMConvertToRadians(data.azimuthAndElevation.x),
		DirectX::XMConvertToRadians(data.azimuthAndElevation.y) };
	CBuffer.lightDirection = Vector3(
		sinf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x),
		cosf(azimuthElevationRad.y) * cosf(azimuthElevationRad.x),
		sinf(azimuthElevationRad.x));
	CBuffer.cameraPosition = data.cameraPos;
	CBuffer.lightDiffuseColor = data.dirLightColor;
}

void ragdoll::Scene::UpdateTransforms()
{
	TraverseTreeAndUpdateTransforms();
}

void ragdoll::Scene::ResetTransformDirtyFlags()
{
	auto EcsView = EntityManager->GetRegistry().view<TransformComp>();
	for (const entt::entity& ent : EcsView) {
		TransformComp* comp = EntityManager->GetComponent<TransformComp>(ent);
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
			TransformComp* furthestTrans = EntityManager->GetComponent<TransformComp>(m_FurthestSibling);
			furthestTrans->m_Sibling = entityId;
			m_FurthestSibling = entityId;
		}
	}
}

void ragdoll::Scene::PopulateStaticProxies()
{
	StaticProxies.clear();
	//iterate through all the transforms and renderable
	auto EcsView = EntityManager->GetRegistry().view<RenderableComp, TransformComp>();
	for (const entt::entity& ent : EcsView) {
		RenderableComp* rComp = EntityManager->GetComponent<RenderableComp>(ent);
		Mesh mesh = AssetManager::GetInstance()->Meshes[rComp->meshIndex];
		for (const Submesh& submesh : mesh.Submeshes)
		{
			Proxy Proxy;
			Proxy.EnttId = (ENTT_ID_TYPE)ent;
			//temp only first submesh now
			Proxy.BufferIndex = submesh.VertexBufferIndex;
			Proxy.MaterialIndex = submesh.MaterialIndex;
			//bounding box will be empty for now
			StaticProxies.push_back(Proxy);
		}
	}
}

void ragdoll::Scene::UpdateStaticProxies()
{
	for (Proxy& proxy : StaticProxies)
	{
		//update all the bounding boxes of the static proxies
		TransformComp* tComp = EntityManager->GetComponent<TransformComp>((entt::entity)proxy.EnttId);
		proxy.BoundingBox = AssetManager::GetInstance()->VertexBufferInfos[proxy.BufferIndex].BestFitBox;
		proxy.BoundingBox.Center.x += tComp->m_ModelToWorld._41;
		proxy.BoundingBox.Center.y += tComp->m_ModelToWorld._42;
		proxy.BoundingBox.Center.z += tComp->m_ModelToWorld._43;

		//transforming the axis aligned vector instead
		Vector3 E = proxy.BoundingBox.Extents;
		proxy.BoundingBox.Extents.x =
			E.x * abs(tComp->m_ModelToWorld._11) +
			E.y * abs(tComp->m_ModelToWorld._21) +
			E.z * abs(tComp->m_ModelToWorld._31);
		proxy.BoundingBox.Extents.y =
			E.x * abs(tComp->m_ModelToWorld._12) +
			E.y * abs(tComp->m_ModelToWorld._22) +
			E.z * abs(tComp->m_ModelToWorld._32);
		proxy.BoundingBox.Extents.z =
			E.x * abs(tComp->m_ModelToWorld._13) +
			E.y * abs(tComp->m_ModelToWorld._23) +
			E.z * abs(tComp->m_ModelToWorld._33);
	}
}

void ragdoll::Scene::BuildStaticInstances(const Matrix& cameraViewProjection, const Matrix& cameraView)
{
	if(StaticProxies.empty())
		return;
	//sort the proxies
	std::sort(StaticProxies.begin(), StaticProxies.end(), [](const Proxy& lhs, const Proxy& rhs) {
		return lhs.BufferIndex < rhs.BufferIndex;
		});
	//build the structured buffer
	int32_t CurrBufferIndex{ -1 }, Start{ 0 };
	if (StaticProxies.size() != 0)
		CurrBufferIndex = StaticProxies[0].BufferIndex;
	//clear the old information
	StaticInstanceGroupInfos.clear();
	StaticInstanceDatas.clear();
	StaticDebugInstanceDatas.clear();

	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, cameraViewProjection, true);
	frustum.Transform(frustum, cameraView.Invert());
	for (int i = 0; i < StaticProxies.size(); ++i) {
		//iterate till i get a different buffer id, meaning is a diff mesh
		if (StaticProxies[i].BufferIndex != CurrBufferIndex || i == StaticProxies.size() - 1)
		{
			if (i == 0)
				i = 1;
			//add this as instance group
			InstanceGroupInfo info;
			info.InstanceOffset = StaticInstanceDatas.size();
			info.VertexBufferIndex = CurrBufferIndex;
			//populate the buffer data vector
			for (int j = Start; j < i; ++j) {
				//debug instances
				InstanceData debugData;
				Vector3 translate = StaticProxies[j].BoundingBox.Center;
				Vector3 scale = StaticProxies[j].BoundingBox.Extents;
				Matrix matrix = Matrix::CreateScale(scale);
				matrix *= Matrix::CreateTranslation(translate);
				debugData.ModelToWorld = matrix;
				debugData.bIsLit = false;
				debugData.Color = { 0.f, 1.f, 0.f, 1.f };
				StaticDebugInstanceDatas.emplace_back(debugData);
				//check if within the camera frustum
				DirectX::ContainmentType result = frustum.Contains(StaticProxies[j].BoundingBox);
				if (result == DirectX::ContainmentType::DISJOINT)
				{
					DebugInfo.CulledObjectCount++;
					continue;
				}
				TransformComp* tComp = EntityManager->GetComponent<TransformComp>((entt::entity)StaticProxies[j].EnttId);
				RenderableComp* rComp = EntityManager->GetComponent<RenderableComp>((entt::entity)StaticProxies[j].EnttId);
				InstanceData Data;

				const Material& mat = AssetManager::GetInstance()->Materials[StaticProxies[j].MaterialIndex];
				if(mat.AlbedoTextureIndex != -1)
				{
					const Texture& tex = AssetManager::GetInstance()->Textures[mat.AlbedoTextureIndex];
					Data.AlbedoIndex = tex.ImageIndex;
					Data.AlbedoSamplerIndex = tex.SamplerIndex;
				}
				if(mat.NormalTextureIndex != -1)
				{
					const Texture& tex = AssetManager::GetInstance()->Textures[mat.NormalTextureIndex];
					Data.NormalIndex = tex.ImageIndex;
					Data.NormalSamplerIndex = tex.SamplerIndex;
				}
				if(mat.RoughnessMetallicTextureIndex != -1)
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
			}
			StaticInstanceGroupInfos.emplace_back(info);
			if(i < StaticProxies.size())
				CurrBufferIndex = StaticProxies[i].BufferIndex;
			Start = i;
		}
	}

	if (StaticInstanceDatas.empty())
	{
		StaticInstanceGroupInfos.clear();
	}
	else
	{
		Renderer.CommandList->open();
		//create the instance buffer handle
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * StaticInstanceDatas.size();
		InstanceBufferDesc.debugName = "Global instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceBufferHandle = Renderer.Device->m_NvrhiDevice->createBuffer(InstanceBufferDesc);

		//copy data over
		Renderer.CommandList->beginTrackingBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::CopyDest);
		Renderer.CommandList->writeBuffer(StaticInstanceBufferHandle, StaticInstanceDatas.data(), sizeof(InstanceData) * StaticInstanceDatas.size());
		Renderer.CommandList->setPermanentBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::ShaderResource);
		Renderer.CommandList->close();
		Renderer.Device->m_NvrhiDevice->executeCommandList(Renderer.CommandList);
	}
	//create the debug instance buffer
	nvrhi::BufferDesc InstanceBufferDesc;
	InstanceBufferDesc.byteSize = sizeof(InstanceData) * StaticDebugInstanceDatas.size();
	InstanceBufferDesc.debugName = "Debug instance buffer";
	InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
	InstanceBufferDesc.structStride = sizeof(InstanceData);
	StaticInstanceDebugBufferHandle = Renderer.Device->m_NvrhiDevice->createBuffer(InstanceBufferDesc);

	//copy data over
	Renderer.CommandList->open();
	Renderer.CommandList->beginTrackingBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::CopyDest);
	Renderer.CommandList->writeBuffer(StaticInstanceDebugBufferHandle, StaticDebugInstanceDatas.data(), sizeof(InstanceData)* StaticDebugInstanceDatas.size());
	Renderer.CommandList->setPermanentBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::ShaderResource);
	Renderer.CommandList->close();
	Renderer.Device->m_NvrhiDevice->executeCommandList(Renderer.CommandList);
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
		PrintRecursive(m_RootEntity, 0, EntityManager);
	if (m_RootSibling.m_RawId)
		PrintRecursive(m_RootSibling, 0, EntityManager);
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
	TransformComp* transform = EntityManager->GetComponent<TransformComp>(guid);
	if (transform)
	{
		UpdateTransform(*transform, guid);

		//traverse siblings with a for loop
		while (transform->m_Sibling.m_RawId != 0) {
			transform = EntityManager->GetComponent<TransformComp>(transform->m_Sibling);
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
