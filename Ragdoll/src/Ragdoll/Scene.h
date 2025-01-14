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
	class FGPUScene;

	struct Proxy {
		Matrix ModelToWorld;
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
		Matrix PrevWorldMatrix;

		Vector4 Color = Vector4::One;
		uint32_t MeshIndex;
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
		bool bFreezeFrustumCulling{ false };
		Matrix FrozenProjection;
		Matrix FrozenView;
		Vector3 FrozenCameraPosition;
		bool bShowFrustum{ false };
		uint32_t TotalProxyCount;
		uint32_t PassedFrustumCullCount{};
		uint32_t PassedOcclusion1CullCount{};
		uint32_t PassedOcclusion2CullCount{};
	};

	struct SceneConfig {
		bool bIsThereCustomMeshes{ false };
		bool bDrawOctree{ false };
		bool bDrawBoxes{ false };
		bool bInitDLSS{ false };
		int32_t DrawOctreeLevelMin{ 0 };
		int32_t DrawOctreeLevelMax{ 0 };
	};

	struct CascadeInfo {
		Vector3 center;
		Matrix view, proj, viewProj;
		float width, height, nearZ{ -10.f }, farZ{ 10.f };
	};

	struct SceneCamera
	{
		std::string Name;
		Vector3 Position;
		Vector3 Rotation;
	};

	struct SceneInformation {
		std::vector<SceneCamera> Cameras;
		int32_t ActiveCameraIndex{ -1 };
		Matrix MainCameraViewProj;
		Matrix MainCameraViewProjWithAA;
		Matrix PrevMainCameraViewProj;
		Matrix MainCameraProj;
		Matrix MainCameraView;
		Matrix PrevMainCameraProj;
		Matrix PrevMainCameraView;
		CascadeInfo CascadeInfos[4];
		Vector3 MainCameraPosition;
		DirectX::BoundingBox SceneBounds;
		Vector4 LightDiffuseColor = { 1.f, 1.f, 1.f, 1.f };
		Vector4 SceneAmbientColor = { 0.2f, 0.2f, 0.2f, 1.f };
		Vector3 LightDirection = { 1.f, -1.f, 1.f };
		float LightIntensity = 1.f;
		float CameraFov;
		float CameraNear;
		float CameraFar;
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
		bool bIsCameraDirty{ true };
		bool bEnableDLSS{ true };
		bool bEnableIntelTAA{ false };
		bool bEnableFSR{ false };
		bool bResetAccumulation{ true };
		bool bEnableJitter{ true };
		bool bEnableXeGTAONoise{ true };
		uint32_t RenderWidth = 1600;
		uint32_t RenderHeight = 900;
		uint32_t TargetWidth;
		uint32_t TargetHeight;
		float JitterX;
		float JitterY;
		bool bIsResolutionDirty{ false };
	};

	struct SceneRenderTargets
	{
		//render targets
		nvrhi::TextureHandle SceneColor;
		nvrhi::TextureHandle CurrDepthBuffer;
		nvrhi::TextureHandle PrevDepthBuffer;
		nvrhi::TextureHandle SceneDepthZ0;
		nvrhi::TextureHandle GBufferAlbedo;
		nvrhi::TextureHandle GBufferNormal;
		nvrhi::TextureHandle GBufferRM;
		nvrhi::TextureHandle VelocityBuffer;
		nvrhi::TextureHandle HZBMips;
		//TAA / fsr
		nvrhi::TextureHandle TemporalColor0;	//temporal buffer
		nvrhi::TextureHandle TemporalColor1;	//temporal buffer
		nvrhi::TextureHandle SceneDepthZ1;	//temporal buffer
		nvrhi::TextureHandle RecontDepth;
		nvrhi::TextureHandle DilatedDepth;
		nvrhi::TextureHandle FarthestDepth;
		nvrhi::TextureHandle DilatedMotionVectors;
		nvrhi::TextureHandle Luminance0;
		nvrhi::TextureHandle Luminance1;
		nvrhi::TextureHandle CurrLuminance;
		nvrhi::TextureHandle PrevLuminance;
		nvrhi::TextureHandle LuminanceHistory0;
		nvrhi::TextureHandle LuminanceHistory1;
		nvrhi::TextureHandle CurrLuminanceHistory;
		nvrhi::TextureHandle PrevLuminanceHistory;
		nvrhi::TextureHandle LumaInstability;
		nvrhi::TextureHandle NewLocks;
		nvrhi::TextureHandle InputReactiveMask;
		nvrhi::TextureHandle InputTCMask;
		nvrhi::TextureHandle DilatedReactiveMask;
		nvrhi::TextureHandle Accumulation0;
		nvrhi::TextureHandle Accumulation1;
		nvrhi::TextureHandle CurrAccumulation;
		nvrhi::TextureHandle PrevAccumulation;
		nvrhi::TextureHandle FarthestDepthMip;
		nvrhi::TextureHandle FrameInfo;
		nvrhi::TextureHandle SpdMips;
		nvrhi::TextureHandle SpdAtomic;
		nvrhi::TextureHandle ShadingChange;
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
		nvrhi::TextureHandle DepthMip;
		nvrhi::TextureHandle AOTerm;
		nvrhi::TextureHandle EdgeMap;
		nvrhi::TextureHandle FinalAOTerm;
		nvrhi::TextureHandle AOTermAccumulation;
		nvrhi::TextureHandle AONormalized;
		//final color
		nvrhi::TextureHandle FinalColor;
		nvrhi::TextureHandle UpscaledBuffer0;
		nvrhi::TextureHandle UpscaledBuffer1;
		nvrhi::TextureHandle CurrUpscaledBuffer;
		nvrhi::TextureHandle PrevUpscaledBuffer;
		nvrhi::TextureHandle PresentationBuffer;
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
		std::shared_ptr<FGPUScene> GPUScene;
		SceneConfig Config;
		DebugInfo DebugInfo;

		std::vector<double> JitterOffsetsX;
		std::vector<double> JitterOffsetsY;
		uint32_t PhaseIndex{}, TotalPhaseCount{};

		SceneRenderTargets RenderTargets;
		//distance where the subfrusta are seperated
		const float SubfrustaFarPlanes[5] = { 0.001f, 10.f, 25.f, 50.f, 100.f };

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

		std::vector<InstanceData> StaticDebugInstanceDatas;	//all the debug cubes
		nvrhi::BufferHandle StaticInstanceDebugBufferHandle;	//contains all the aabb boxes to draw

		void PopulateStaticProxies();
		void BuildDebugInstances(std::vector<InstanceData>& instances);

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