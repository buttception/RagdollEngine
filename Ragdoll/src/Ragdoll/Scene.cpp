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
	Renderer.BeginFrame(CBuffer);
	Renderer.DrawAllInstances(StaticInstanceBufferHandle, StaticInstanceGroupInfos, CBuffer);

	//spawn more shits temp
	const static Vector3 t_range{ 10.f, 10.f, 10.f };
	const static Vector3 t_min = { -5.f, -5.f, -5.f };
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
				UpdateTransforms();
				BuildStaticInstances();
			}
			MicroProfileDumpFileImmediately("test.html", nullptr, nullptr);
		}
		ImGui::End();
	}
	

	ImguiInterface.Render();
	Renderer.Device->Present();
}

void ragdoll::Scene::Shutdown()
{
	Renderer.Shutdown();
}

void ragdoll::Scene::UpdateTransforms()
{
	TraverseTreeAndUpdateTransforms();
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

void ragdoll::Scene::BuildStaticInstances()
{
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
			Proxies.push_back(Proxy);
		}
	}
	if(Proxies.empty())
		return;
	//sort the proxies
	std::sort(Proxies.begin(), Proxies.end(), [](const Proxy& lhs, const Proxy& rhs) {
		return lhs.BufferIndex < rhs.BufferIndex;
		});
	//build the structured buffer
	int32_t CurrBufferIndex{ -1 }, Start{ 0 };
	if (Proxies.size() != 0)
		CurrBufferIndex = Proxies[0].BufferIndex;
	//clear the old information
	StaticInstanceGroupInfos.clear();
	StaticInstanceDatas.clear();

	for (int i = 0; i < Proxies.size(); ++i) {
		//iterate till i get a different buffer id, meaning is a diff mesh
		if (Proxies[i].BufferIndex != CurrBufferIndex || i == Proxies.size() - 1)
		{
			if (i == 0)
				i = 1;
			//add this as instance group
			InstanceGroupInfo info;
			info.InstanceCount = i - Start;
			info.InstanceOffset = StaticInstanceDatas.size();
			info.VertexBufferIndex = CurrBufferIndex;
			StaticInstanceGroupInfos.emplace_back(info);
			//populate the buffer data vector
			for (int j = Start; j < i; ++j) {	//maybe should do in outer loop, then gather all the materials too
				TransformComp* tComp = EntityManager->GetComponent<TransformComp>((entt::entity)Proxies[j].EnttId);
				RenderableComp* rComp = EntityManager->GetComponent<RenderableComp>((entt::entity)Proxies[j].EnttId);
				InstanceData Data;

				const Material& mat = AssetManager::GetInstance()->Materials[Proxies[j].MaterialIndex];
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
			}
			if(i < Proxies.size())
				CurrBufferIndex = Proxies[i].BufferIndex;
			Start = i;
		}
	}
	{
		//create the instance buffer handle
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * StaticInstanceDatas.size();
		InstanceBufferDesc.debugName = "Global instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceBufferHandle = Renderer.Device->m_NvrhiDevice->createBuffer(InstanceBufferDesc);

		//copy data over
		Renderer.CommandList->open();
		Renderer.CommandList->beginTrackingBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::CopyDest);
		Renderer.CommandList->writeBuffer(StaticInstanceBufferHandle, StaticInstanceDatas.data(), sizeof(InstanceData) * StaticInstanceDatas.size());
		Renderer.CommandList->setPermanentBufferState(StaticInstanceBufferHandle, nvrhi::ResourceStates::ShaderResource);
		Renderer.CommandList->close();
		Renderer.Device->m_NvrhiDevice->executeCommandList(Renderer.CommandList);
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
	auto transform = EntityManager->GetComponent<TransformComp>(guid);
	if (transform)
	{
		{
			//check if state is dirty
			bool dirtyFromHere = false;
			if (!m_DirtyOnwards)
			{
				if (transform->m_Dirty)
				{
					m_DirtyOnwards = true;
					dirtyFromHere = true;
				}
			}
			if (m_DirtyOnwards)
			{
				//get local matrix
				auto localModel = GetLocalModelMatrix(*transform);
				//add to model stack
				if (m_ModelStack.empty())	//first matrix will be this guy local
					m_ModelStack.push(localModel);
				else
					m_ModelStack.push(localModel * m_ModelStack.top());	//otherwise concatenate matrix in stacks
				transform->m_ModelToWorld = m_ModelStack.top();	//set model matrix to the top of the stack
			}
			else
				m_ModelStack.push(transform->m_ModelToWorld);	//if not dirty, just push the current model matrix
			//traverse children
			if (transform->m_Child)
				TraverseNode(transform->m_Child);

			//once done, pop the stack
			m_ModelStack.pop();
			//resets the dirty flags
			if (dirtyFromHere)
				m_DirtyOnwards = false;
			transform->m_Dirty = false;
		}

		//traverse siblings with a for loop
		while (transform->m_Sibling.m_RawId != 0) {
			transform = EntityManager->GetComponent<TransformComp>(transform->m_Sibling);
			//check if state is dirty
			bool dirtyFromHere = false;
			if (!m_DirtyOnwards)
			{
				if (transform->m_Dirty)
				{
					m_DirtyOnwards = true;
					dirtyFromHere = true;
				}
			}
			if (m_DirtyOnwards)
			{
				//get local matrix
				auto localModel = GetLocalModelMatrix(*transform);
				//add to model stack
				if (m_ModelStack.empty())	//first matrix will be this guy local
					m_ModelStack.push(localModel);
				else
					m_ModelStack.push(localModel * m_ModelStack.top());	//otherwise concatenate matrix in stacks
				transform->m_ModelToWorld = m_ModelStack.top();	//set model matrix to the top of the stack
			}
			else
				m_ModelStack.push(transform->m_ModelToWorld);	//if not dirty, just push the current model matrix
			//traverse children
			if (transform->m_Child)
				TraverseNode(transform->m_Child);

			//once done, pop the stack
			m_ModelStack.pop();
			//resets the dirty flags
			if (dirtyFromHere)
				m_DirtyOnwards = false;
			transform->m_Dirty = false;
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
