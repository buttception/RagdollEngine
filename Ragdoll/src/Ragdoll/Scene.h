#pragma once
#include "ImGuiRenderer.h"
#include "ForwardRenderer.h"

#include "Components/TransformComp.h"
#include "Components/RenderableComp.h"

namespace ragdoll {
	class EntityManager;
	class TransformSystem;
	class Application;

	class Scene {
		ForwardRenderer Renderer;
		ImguiRenderer ImguiInterface;

		//Transforms
		std::shared_ptr<EntityManager> EntityManager;
		std::stack<Matrix> m_ModelStack;
		//the root details
		Guid m_RootEntity;
		Guid m_RootSibling;
		//state
		bool m_DirtyOnwards{ false };

	public:
		Scene(Application*);

		void Init();
		void Update(float _dt);
		void Shutdown();

		//Transforms
		void AddEntityAtRootLevel(Guid entityId);

	private:
		//Transforms
		void UpdateTransforms();
		bool HasRoot() { return m_RootEntity.m_RawId != 0; }
		void DebugPrintHierarchy();
		void TraverseTreeAndUpdateTransforms();
		void TraverseNode(const Guid& guid);
		Matrix GetLocalModelMatrix(const TransformComp& trans);

		//Renderable

	};
}