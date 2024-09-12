#include "ragdollpch.h"

#include "Scene.h"

#include "Application.h"
#include "Entity/EntityManager.h"
#include "AssetManager.h"
#include "DirectXDevice.h"

ragdoll::Scene::Scene(Application* app)
{
	EntityManager = app->m_EntityManager;
	Renderer.Init(app->m_PrimaryWindow, app->m_FileManager, app->m_EntityManager);
	ImguiInterface.Init(Renderer.Device.get(), Renderer.ImguiVertexShader, Renderer.ImguiPixelShader);
}

void ragdoll::Scene::Update(float _dt)
{
	UpdateTransforms();
	ImguiInterface.BeginFrame();
	Renderer.Draw();

	//spawn more shits temp
	const static Vector3 t_range{ 10.f, 10.f, 10.f };
	const static Vector3 t_min = { -5.f, -5.f, -5.f };
	const static Vector3 s_range{ 0.2f, 0.2f, 0.2f };
	const static Vector3 s_min{ 0.3f, 0.3f, 0.3f };
	static int32_t currentGeomCount{};

	ImGui::Begin("Spawn");
	ImGui::Text("Current Geom Count: %d", currentGeomCount);
	if (ImGui::Button("Spawn 100 geometry")) {
		for (int i = 0; i < 100; ++i) {
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
			entt::entity ent = EntityManager->CreateEntity();
			auto tcomp = EntityManager->AddComponent<TransformComp>(ent);
			tcomp->m_LocalPosition = pos;
			tcomp->m_LocalScale = scale;
			AddEntityAtRootLevel(EntityManager->GetGuid(ent));
			auto rcomp = EntityManager->AddComponent<RenderableComp>(ent);
			rcomp->meshIndex = std::rand() / (float)RAND_MAX * AssetManager::GetInstance()->Meshes.size();
		}
	}
	ImGui::End();

	ImguiInterface.Render();
	Renderer.Device->Present();
}

void ragdoll::Scene::Shutdown()
{
	//Renderer.Device->Shutdown();
}

void ragdoll::Scene::UpdateTransforms()
{
	TraverseTreeAndUpdateTransforms();
}

void AddNodeToFurthestSibling(ragdoll::Guid sibling, ragdoll::Guid node, std::shared_ptr<ragdoll::EntityManager> em)
{
	TransformComp* trans = em->GetComponent<TransformComp>(sibling);
	if (trans->m_Sibling.m_RawId == 0)
		trans->m_Sibling = node;
	else
		AddNodeToFurthestSibling(trans->m_Sibling, node, em);
}

void ragdoll::Scene::AddEntityAtRootLevel(Guid entityId)
{
	if (m_RootEntity.m_RawId == 0)
		m_RootEntity = entityId;
	else {
		if (m_RootSibling.m_RawId == 0)
			m_RootSibling = entityId;
		else
			AddNodeToFurthestSibling(m_RootSibling, entityId, EntityManager);
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
