#pragma once
#include "ImGuiRenderer.h"
#include "ForwardRenderer.h"
#include "DeferredRenderer.h"

#include "Components/TransformComp.h"
#include "Components/RenderableComp.h"
#include "Entity/EntityManager.h"
#include "Octree.h"

namespace ragdoll {
	class EntityManager;
	class TransformSystem;
	class Application;

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
		uint32_t InstanceCount = 0;
		int32_t VertexBufferIndex;
	};

	struct DebugInfo {
		uint32_t CulledObjectCount{};
	};

	struct SceneConfig {
		bool bIsThereCustomMeshes{ false };
		bool bDrawOctree{ false };
		bool bDrawBoxes{ false };
	};

	struct SceneInformation {
		Matrix MainCameraViewProj;
		Matrix InfiniteReverseZProj;
		Matrix MainCameraView;
		Matrix LightViewProj[4];
		Vector3 MainCameraPosition;
		Vector4 LightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
		Vector4 SceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
		Vector3 LightDirection = { 1.f, -1.f, 1.f };
		float LightIntensity = 1.f;
		float CameraFov;
		float CameraAspect;
		bool EnableCascadeDebug{ false };
	};

	class Scene {
		std::shared_ptr<EntityManager> EntityManagerRef;
		std::shared_ptr<Window> PrimaryWindowRef;
		std::shared_ptr<DirectXDevice> DeviceRef;

		std::shared_ptr<ImguiRenderer> ImguiInterface;

		//Transforms
		std::stack<Matrix> m_ModelStack;
		//the root details
		Guid m_RootEntity;
		Guid m_RootSibling;
		Guid m_FurthestSibling;	//cache for better performance
		//state
		bool m_DirtyOnwards{ false };

		//Rendering
		nvrhi::CommandListHandle CommandList;
		bool bIsCameraDirty{ true };
		bool bFreezeFrustumCulling{ false };
		Octree StaticOctree;
		std::vector<Proxy> StaticProxiesToDraw;

	public:
		std::shared_ptr<ForwardRenderer> ForwardRenderer;
		std::shared_ptr<DeferredRenderer> DeferredRenderer;
		SceneConfig Config;
		DebugInfo DebugInfo;

		Matrix CameraViewProjection;
		Matrix CameraProjection;
		Matrix CameraView;

		//render targets
		nvrhi::TextureHandle SceneColor;
		nvrhi::TextureHandle SceneDepthZ;
		nvrhi::TextureHandle GBufferAlbedo;
		nvrhi::TextureHandle GBufferNormal;
		nvrhi::TextureHandle GBufferORM;
		//shadows
		nvrhi::TextureHandle ShadowMap[4];
		//distance where the subfrusta are seperated
		//0 -> 5, 5 -> 10, 10 -> 20 and 20 -> 50
		const float SubfrustaFarPlanes[5] = { 0.001f, 5.f, 15.f, 30.f, 100.f };
		//the cascades
		Matrix DirectionalLightViewProjections[4];

		Scene(Application*);

		void Update(float _dt);
		void Shutdown();

		void UpdateControls(float _dt);
		void CreateCustomMeshes();
		void CreateRenderTargets();

		//Transforms
		void UpdateTransforms();
		void ResetTransformDirtyFlags();
		void AddEntityAtRootLevel(Guid entityId);

		//Renderable
		SceneInformation SceneInfo;
		std::vector<InstanceData> StaticInstanceDatas;
		std::vector<InstanceData> StaticDebugInstanceDatas;
		std::vector<InstanceGroupInfo> StaticInstanceGroupInfos;
		nvrhi::BufferHandle StaticInstanceBufferHandle;
		nvrhi::BufferHandle StaticInstanceDebugBufferHandle;	//contains all the aabb boxes to draw

		void PopulateStaticProxies();
		void BuildStaticInstances(const Matrix& cameraProjection, const Matrix& cameraView);
		void BuildDebugInstances();
		void CullOctant(const Octant& octant, const DirectX::BoundingFrustum& frustum);
		void UpdateShadowCascades();

	private:
		//Transforms
		bool HasRoot() { return m_RootEntity.m_RawId != 0; }
		void DebugPrintHierarchy();
		void TraverseTreeAndUpdateTransforms();
		void TraverseNode(const Guid& guid);
		Matrix GetLocalModelMatrix(const TransformComp& trans);
		void UpdateTransform(TransformComp& comp, const Guid& id);

		//Debug
		void AddOctantDebug(Octant octant, uint32_t level);
	};
}