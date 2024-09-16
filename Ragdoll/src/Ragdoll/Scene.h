#pragma once
#include "ImGuiRenderer.h"
#include "ForwardRenderer.h"

#include "Components/TransformComp.h"
#include "Components/RenderableComp.h"
#include "Entity/EntityManager.h"

namespace ragdoll {
	class EntityManager;
	class TransformSystem;
	class Application;

	struct Proxy {
		ENTT_ID_TYPE EnttId;	//can draw to a entity buffer next time
		int32_t BufferIndex = -1;	//does not exist for now
		int32_t MaterialIndex = -1;
		//texture index and stuff next time
	};

	struct InstanceData {
		Matrix ModelToWorld;
		Matrix InvModelToWorld;

		Vector4 Color = Vector4::One;
		float Roughness = 0.f;
		float Metallic = 0.f;

		int AlbedoIndex = -1;
		int NormalIndex = -1;
		int RoughnessMetallicIndex = -1;
		int bIsLit = 1;
	};

	struct InstanceBuffer {
		nvrhi::BufferHandle BufferHandle;
		uint32_t CurrentCapacity = StartCapacity;
		uint32_t InstanceCount = 0;
		std::vector<InstanceData> Data;

		int32_t VertexBufferIndex;
		std::vector<int32_t> MaterialIndices;
		
		static const uint32_t StartCapacity = 64;
	};

	class Scene {
		ImguiRenderer ImguiInterface;

		//Transforms
		std::shared_ptr<EntityManager> EntityManager;
		std::stack<Matrix> m_ModelStack;
		//the root details
		Guid m_RootEntity;
		Guid m_RootSibling;
		//state
		bool m_DirtyOnwards{ false };

		//Rendering
		CBuffer CBuffer;
		std::vector<Proxy> Proxies;
		bool bIsStaticProxiesBuilt{ false };
		std::vector<InstanceBuffer> StaticInstanceBuffers;

	public:
		ForwardRenderer Renderer;

		Scene(Application*);

		void Init();
		void Update(float _dt);
		void Shutdown();

		//Transforms
		void UpdateTransforms();
		void AddEntityAtRootLevel(Guid entityId);

		//Renderable
		void BuildStaticInstances();

	private:
		//Transforms
		bool HasRoot() { return m_RootEntity.m_RawId != 0; }
		void DebugPrintHierarchy();
		void TraverseTreeAndUpdateTransforms();
		void TraverseNode(const Guid& guid);
		Matrix GetLocalModelMatrix(const TransformComp& trans);
	};
}