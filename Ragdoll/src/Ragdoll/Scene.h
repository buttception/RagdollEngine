#pragma once
#include "ImGuiRenderer.h"
#include "DeferredRenderer.h"

#include "Components/TransformComp.h"
#include "Components/RenderableComp.h"
#include "RenderPasses/BloomPass.h"
#include "Entity/EntityManager.h"
#include "Octree.h"

namespace ragdoll {
	class EntityManager;
	class TransformSystem;
	class Application;

	struct Proxy {
		Matrix ModelToWorld;
		Matrix InvModelToWorld;
		DirectX::BoundingBox BoundingBox;

		Vector4 Color = Vector4::One;
		float Roughness = 0.f;
		float Metallic = 0.f;

		int32_t BufferIndex;
		int32_t MaterialIndex;
		int32_t AlbedoIndex = -1;
		int32_t AlbedoSamplerIndex = 0;
		int32_t NormalIndex = -1;
		int32_t NormalSamplerIndex = 0;
		int32_t ORMIndex = -1;
		int32_t ORMSamplerIndex = 0;
		int32_t bIsLit = 1;
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
		int ORMIndex = -1;
		int ORMSamplerIndex = 0;
		int bIsLit = 1;

		InstanceData& operator=(const Proxy& proxy) {
			ModelToWorld = proxy.ModelToWorld;
			InvModelToWorld = proxy.InvModelToWorld;

			Color = proxy.Color;
			Roughness = proxy.Roughness;
			Metallic = proxy.Metallic;
			
			AlbedoIndex = proxy.AlbedoIndex;
			AlbedoSamplerIndex = proxy.AlbedoSamplerIndex;
			NormalIndex = proxy.NormalIndex;
			NormalSamplerIndex = proxy.NormalSamplerIndex;
			ORMIndex = proxy.ORMIndex;
			ORMSamplerIndex = proxy.ORMSamplerIndex;
			bIsLit = proxy.bIsLit;
			return *this;
		}
	};

	struct InstanceGroupInfo {
		uint32_t InstanceCount = 0;
		int32_t VertexBufferIndex;
	};

	struct DebugInfo {
		uint32_t CulledOctantsCount{};
	};

	struct SceneConfig {
		bool bIsThereCustomMeshes{ false };
		bool bDrawOctree{ false };
		bool bDrawBoxes{ false };
		int32_t DrawOctreeLevelMin{ 0 };
		int32_t DrawOctreeLevelMax{ 0 };
	};

	struct CascadeInfo {
		Vector3 center;
		Matrix view, proj, viewProj;
		float width, height, nearZ{ -10.f }, farZ{ 10.f };
	};

	struct SceneInformation {
		Matrix MainCameraViewProj;
		Matrix InfiniteReverseZProj;
		Matrix MainCameraView;
		CascadeInfo CascadeInfo[4];
		Vector3 MainCameraPosition;
		Vector4 LightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
		Vector4 SceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
		Vector3 LightDirection = { 1.f, -1.f, 1.f };
		float LightIntensity = 1.f;
		float CameraFov;
		float CameraNear;
		float CameraAspect;
		int32_t EnableCascadeDebug{ 0 };
		float Gamma = 1.f;
		float Exposure = 1.f;
		float SkyDimmer = 0.f;
		bool UseFixedExposure = 0.f;
		float FilterRadius = 0.05f;
		float BloomIntensity = 0.04f;
	};

	class Scene {
		std::shared_ptr<EntityManager> EntityManagerRef;
		std::shared_ptr<Window> PrimaryWindowRef;

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

	public:
		std::shared_ptr<Renderer> DeferredRenderer;
		SceneConfig Config;
		DebugInfo DebugInfo;
		Octree StaticOctree;

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
		nvrhi::TextureHandle ShadowMask;
		//sky texture
		nvrhi::TextureHandle SkyTexture;
		nvrhi::TextureHandle SkyThetaGammaTable;
		//bloom
		std::vector<BloomMip> DownsampledImages;
		const uint32_t MipCount{ 5 };	//how many times to downsample
		//distance where the subfrusta are seperated
		const float SubfrustaFarPlanes[5] = { 0.001f, 5.f, 10.f, 15.f, 30.f };

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
		std::vector<Proxy> StaticProxies;

		std::vector<InstanceData> StaticInstanceDatas;	//all the instances to draw that was culled
		std::vector<InstanceGroupInfo> StaticInstanceGroupInfos;	//info on how to draw the instances

		std::vector<InstanceData> StaticCascadeInstanceDatas[4];	//all instances to draw for shadow map
		std::vector<InstanceGroupInfo> StaticCascadeInstanceInfos[4];	//info on how to draw the instances

		std::vector<InstanceData> StaticDebugInstanceDatas;	//all the debug cubes

		nvrhi::BufferHandle StaticInstanceBufferHandle;
		nvrhi::BufferHandle StaticCascadeInstanceBufferHandles[4];	//all the cascade instance handles
		nvrhi::BufferHandle StaticInstanceDebugBufferHandle;	//contains all the aabb boxes to draw

		void PopulateStaticProxies();	//add all proxies into the octree
		void BuildStaticInstances(const Matrix& cameraProjection, const Matrix& cameraView, std::vector<InstanceData>& instances, std::vector<InstanceGroupInfo>& instancesGrpInfo);
		void BuildStaticCascadeMapInstances();
		void BuildDebugInstances(std::vector<InstanceData>& instances);

		void CullOctant(Octant& octant, const DirectX::BoundingFrustum& frustum, std::vector<uint32_t>& result);
		void CullOctantForCascade(const Octant& octant, const DirectX::BoundingOrientedBox& oob, std::vector<uint32_t>& result, const Vector3& center, const Vector3& normal, float& back, float& front);

		void UpdateShadowCascadesExtents();
		void UpdateShadowLightMatrices();

	private:
		//Transforms
		bool HasRoot() { return m_RootEntity.m_RawId != 0; }
		void DebugPrintHierarchy();
		void TraverseTreeAndUpdateTransforms();
		void TraverseNode(const Guid& guid);
		Matrix GetLocalModelMatrix(const TransformComp& trans);
		void UpdateTransform(TransformComp& comp, const Guid& id);

		//Debug
		void AddOctantDebug(const Octant& octant, uint32_t level);
	};
}