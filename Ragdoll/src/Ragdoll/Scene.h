#pragma once

#include "Components/TransformComp.h"
#include "Components/RenderableComp.h"
#include "RenderPasses/BloomPass.h"
#include "Entity/EntityManager.h"
#include "Octree.h"

class Renderer;
class ImguiRenderer;
namespace ragdoll {
	class EntityManager;
	class TransformSystem;
	class Application;
	class Window;

	struct Proxy {
		Matrix ModelToWorld;
		Matrix InvModelToWorld;
		Matrix PrevWorldMatrix;
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
		Matrix PrevWorldMatrix;

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
			PrevWorldMatrix = proxy.PrevWorldMatrix;

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
		nvrhi::TextureHandle DbgTarget;
		uint32_t CompCount;
		Vector4 Add;
		Vector4 Mul;
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
		Matrix MainCameraViewProjWithAA;
		Matrix PrevMainCameraViewProj;
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
		float Luminance = 0.f;
		float FilterRadius = 0.05f;
		float BloomIntensity = 0.04f;
		bool UseCACAO = false;
		bool UseXeGTAO = true;
		float ModulationFactor = 0.9f;
		bool bIsCameraDirty{ true };
		bool bFreezeFrustumCulling{ false };
		bool bEnableDLSS{ true };
		bool bEnableJitter{ true };
		bool bEnableXeGTAONoise{ true };
		uint32_t RenderWidth = 960;
		uint32_t RenderHeight = 540;
		uint32_t TargetWidth;
		uint32_t TargetHeight;
		bool bIsResolutionDirty{ false };
	};

	struct SceneRenderTargets
	{
		//render targets
		nvrhi::TextureHandle SceneColor;
		nvrhi::TextureHandle SceneDepthZ;
		nvrhi::TextureHandle GBufferAlbedo;
		nvrhi::TextureHandle GBufferNormal;
		nvrhi::TextureHandle GBufferRM;
		nvrhi::TextureHandle VelocityBuffer;
		//shadows
		nvrhi::TextureHandle ShadowMap[4];
		nvrhi::TextureHandle ShadowMask;
		//sky texture
		nvrhi::TextureHandle SkyTexture;
		nvrhi::TextureHandle SkyThetaGammaTable;
		//bloom
		std::vector<BloomMip> DownsampledImages;
		const uint32_t MipCount{ 5 };	//how many times to downsample
		//cacao
		nvrhi::TextureHandle DeinterleavedDepth;
		nvrhi::TextureHandle DeinterleavedNormals;
		nvrhi::TextureHandle SSAOBufferPong;
		nvrhi::TextureHandle SSAOBufferPing;
		nvrhi::TextureHandle ImportanceMap;
		nvrhi::TextureHandle ImportanceMapPong;
		nvrhi::TextureHandle LoadCounter;
		//xegtao
		nvrhi::TextureHandle DepthMips;
		nvrhi::TextureHandle AOTerm;
		nvrhi::TextureHandle EdgeMap;
		nvrhi::TextureHandle FinalAOTerm;
		nvrhi::TextureHandle AOTermAccumulation;
		nvrhi::TextureHandle AONormalized;
		//final color
		nvrhi::TextureHandle FinalColor;
		nvrhi::TextureHandle UpscaledBuffer;
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

	public:
		std::shared_ptr<Renderer> DeferredRenderer;
		SceneConfig Config;
		DebugInfo DebugInfo;
		Octree StaticOctree;

		std::vector<double> JitterOffsetsX;
		std::vector<double> JitterOffsetsY;
		uint32_t PhaseIndex{}, TotalPhaseCount{};

		SceneRenderTargets RenderTargets;
		//distance where the subfrusta are seperated
		const float SubfrustaFarPlanes[5] = { 0.001f, 5.f, 10.f, 15.f, 30.f };

		Scene(Application*);

		void Update(float _dt);
		void Shutdown();

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

		// Halton Sequence
		void HaltonSequence(Vector2 RenderRes, Vector2 TargetRes);	//assuming aspect ratio is same

		//Debug
		void AddOctantDebug(const Octant& octant, int32_t level);
	};
}