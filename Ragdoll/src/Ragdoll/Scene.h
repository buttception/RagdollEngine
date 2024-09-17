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
		ENTT_ID_TYPE EnttId;
		int32_t BufferIndex = -1;
		int32_t MaterialIndex = -1;
		DirectX::BoundingBox BoundingBox;
	};

	struct InstanceData {
		Matrix ModelToWorld;
		Matrix InvModelToWorld;

		Vector4 Color = Vector4::One;
		float Roughness = 0.f;
		float Metallic = 0.f;

		int AlbedoIndex = -1;
		int AlbedoSamplerIndex = 0;
		int NormalIndex = -1;
		int NormalSamplerIndex = 0;
		int RoughnessMetallicIndex = -1;
		int RoughnessMetallicSamplerIndex = 0;
		int bIsLit = 1;
	};

	struct InstanceGroupInfo {
		uint32_t InstanceOffset = 0;
		uint32_t InstanceCount = 0;
		int32_t VertexBufferIndex;
	};

	struct SceneConfig {
		bool bIsThereCustomMeshes{ false };
	};

	class Scene {
		ImguiRenderer ImguiInterface;

		//Transforms
		std::shared_ptr<EntityManager> EntityManager;
		std::stack<Matrix> m_ModelStack;
		//the root details
		Guid m_RootEntity;
		Guid m_RootSibling;
		Guid m_FurthestSibling;	//cache for better performance
		//state
		bool m_DirtyOnwards{ false };

		//Rendering
		CBuffer CBuffer;
		std::vector<Proxy> StaticProxies;
		std::vector<DirectX::BoundingBox> ProxyBoundingBoxes;
		std::vector<InstanceData> StaticInstanceDatas;
		std::vector<InstanceGroupInfo> StaticInstanceGroupInfos;
		nvrhi::BufferHandle StaticInstanceBufferHandle;

	public:
		ForwardRenderer Renderer;
		SceneConfig Config;

		Scene(Application*);

		void Init();
		void Update(float _dt);
		void Shutdown();

		//Transforms
		void UpdateTransforms();
		void ResetTransformDirtyFlags();
		void AddEntityAtRootLevel(Guid entityId);

		//Renderable
		void PopulateStaticProxies();
		void UpdateStaticProxies();
		void BuildStaticInstances();

	private:
		//Transforms
		bool HasRoot() { return m_RootEntity.m_RawId != 0; }
		void DebugPrintHierarchy();
		void TraverseTreeAndUpdateTransforms();
		void TraverseNode(const Guid& guid);
		Matrix GetLocalModelMatrix(const TransformComp& trans);
		void UpdateTransform(TransformComp& comp, const Guid& id);
	};
}