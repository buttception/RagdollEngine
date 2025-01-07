#include "ragdollpch.h"

#include "Scene.h"

#include <nvrhi/utils.h>
#include "Profiler.h"
#include "Application.h"
#include "Entity/EntityManager.h"
#include "AssetManager.h"
#include "DirectXDevice.h"
#include "GeometryBuilder.h"
#include "ImGuiRenderer.h"
#include "DeferredRenderer.h"
#include "Graphics/Window/Window.h"
#include "NVSDK.h"
#include "GPUScene.h"

ragdoll::Scene::Scene(Application* app)
{
	MICROPROFILE_SCOPEI("Scene", "Scene::Init", MP_DARKRED);
	EntityManagerRef = app->m_EntityManager;
	PrimaryWindowRef = app->m_PrimaryWindow;

	SceneInfo.TargetWidth = PrimaryWindowRef->GetWidth();
	SceneInfo.TargetHeight = PrimaryWindowRef->GetHeight();

	{
		MICROPROFILE_SCOPEI("Render", "Create Render Target", MP_YELLOW);
		CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
		CreateRenderTargets();
	}

	{
		MICROPROFILE_SCOPEI("Render", "Create Renderer", MP_YELLOW);
		DeferredRenderer = std::make_shared<class Renderer>();
		DeferredRenderer->Init(app->m_PrimaryWindow, this);
		GPUScene = std::make_shared<FGPUScene>();
	}
	if (app->Config.bCreateCustomMeshes)
	{
		Config.bIsThereCustomMeshes = true;
		CreateCustomMeshes();
	}
	Config.bDrawBoxes = app->Config.bDrawDebugBoundingBoxes;
	Config.bDrawOctree = app->Config.bDrawDebugOctree;
	ImguiInterface = std::make_shared<ImguiRenderer>();
	ImguiInterface->Init(DirectXDevice::GetInstance());

	HaltonSequence(Vector2(SceneInfo.RenderWidth, SceneInfo.RenderHeight), Vector2(SceneInfo.TargetWidth, SceneInfo.TargetHeight));

	Config.bInitDLSS = app->Config.bInitDLSS;
	if (Config.bInitDLSS)
		NVSDK::Init(DirectXDevice::GetInstance()->m_Device12, Vector2(SceneInfo.RenderWidth, SceneInfo.RenderHeight), Vector2(SceneInfo.TargetWidth, SceneInfo.TargetHeight));
	else
		SceneInfo.bEnableDLSS = false;
}

void ragdoll::Scene::Update(float _dt)
{
	//no need to update transforms as the scene is static for now
	//UpdateTransforms();
	{
		RD_SCOPE(Render, ImGuiBuildData);
		ImguiInterface->BeginFrame();

		int item = ImguiInterface->DrawFBViewer();

		switch (item) {
		case 1:
			DebugInfo.CompCount = 2;
			DebugInfo.DbgTarget = RenderTargets.GBufferNormal;
			DebugInfo.Add = Vector4::Zero;
			DebugInfo.Mul = Vector4::One;
			break;
		case 2:
			DebugInfo.CompCount = 2;
			DebugInfo.DbgTarget = RenderTargets.GBufferRM;
			DebugInfo.Add = Vector4::Zero;
			DebugInfo.Mul = Vector4::One;
			break;
		case 3:
			DebugInfo.CompCount = 3;
			DebugInfo.DbgTarget = RenderTargets.VelocityBuffer;
			DebugInfo.Add = Vector4::Zero;
			DebugInfo.Mul = Vector4::One;
			break;
		case 4:
			DebugInfo.CompCount = 1;
			DebugInfo.DbgTarget = RenderTargets.AONormalized;
			DebugInfo.Add = Vector4::Zero;
			DebugInfo.Mul = Vector4::One;
			break;
		case 5:
			DebugInfo.CompCount = 4;
			DebugInfo.DbgTarget = DeferredRenderer->bIsOddFrame ? RenderTargets.TemporalColor0 : RenderTargets.TemporalColor1;;
			DebugInfo.Add = Vector4::Zero;
			DebugInfo.Mul = Vector4::One;
			break;
		case 0:
		default:
			DebugInfo.DbgTarget = nullptr;
		}
		SceneInfo.Luminance = DeferredRenderer->AdaptedLuminance;

		ImguiInterface->DrawSettings(DebugInfo, SceneInfo, Config, _dt);

		PhaseIndex = ++PhaseIndex == TotalPhaseCount ? 0 : PhaseIndex;
		//jitter the projection
		Matrix Proj = SceneInfo.InfiniteReverseZProj;
		if (SceneInfo.bEnableJitter)
		{
			Proj.m[2][0] += JitterOffsetsX[PhaseIndex] / (double)SceneInfo.RenderWidth;
			Proj.m[2][1] += JitterOffsetsY[PhaseIndex] / (double)SceneInfo.RenderHeight;
			SceneInfo.JitterX = JitterOffsetsX[PhaseIndex];
			SceneInfo.JitterY = JitterOffsetsY[PhaseIndex];
		}
		else
		{
			SceneInfo.JitterX = 0.f;
			SceneInfo.JitterY = 0.f;
		}
		SceneInfo.MainCameraViewProjWithAA = SceneInfo.MainCameraView * Proj;
	}

	if (SceneInfo.bIsCameraDirty)
	{
		DebugInfo.CulledOctantsCount = 0;
		UpdateShadowCascadesExtents();
		UpdateShadowLightMatrices();
		BuildDebugInstances(StaticDebugInstanceDatas);
	}

	if (SceneInfo.bIsResolutionDirty)
	{
		HaltonSequence(Vector2(SceneInfo.RenderWidth, SceneInfo.RenderHeight), Vector2(SceneInfo.TargetWidth, SceneInfo.TargetHeight));
		if(Config.bInitDLSS)
			NVSDK::Init(DirectXDevice::GetInstance()->m_Device12, Vector2(SceneInfo.RenderWidth, SceneInfo.RenderHeight), Vector2(SceneInfo.TargetWidth, SceneInfo.TargetHeight));
		CreateRenderTargets();
	}

	DeferredRenderer->Render(this, GPUScene.get(), _dt, ImguiInterface);

	DirectXDevice::GetInstance()->Present();

	SceneInfo.bIsResolutionDirty = false;
	SceneInfo.bIsCameraDirty = false;
}

void ragdoll::Scene::Shutdown()
{
	if(Config.bInitDLSS)
		NVSDK::Release();
	ImguiInterface->Shutdown();
	ImguiInterface = nullptr;
	DeferredRenderer->Shutdown();
	DeferredRenderer = nullptr;
}

void ragdoll::Scene::CreateCustomMeshes()
{
	//create 5 random textures
	uint8_t colors[5][4] = {
		{255, 0, 0, 255},
		{255, 255, 0, 255},
		{0, 255, 255, 255},
		{0, 0, 255, 255},
		{0, 255, 0, 255},
	};
	std::string debugNames[5] = {
		"Custom Red",
		"Custom Yellow",
		"Custom Cyan",
		"Custom Blue",
		"Custom Green",
	};
	CommandList->open();
	for (int i = 0; i < 5; ++i)
	{
		Image img;
		nvrhi::TextureDesc textureDesc;
		textureDesc.width = 1;
		textureDesc.height = 1;
		textureDesc.format = nvrhi::Format::RGBA8_UNORM;
		textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
		textureDesc.isRenderTarget = false;
		textureDesc.isTypeless = false;
		textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
		textureDesc.keepInitialState = true;
		textureDesc.debugName = debugNames[i];
		nvrhi::TextureHandle texHandle = DirectXDevice::GetNativeDevice()->createTexture(textureDesc);
		CommandList->writeTexture(texHandle, 0, 0, &colors[i], 4);
		img.TextureHandle = texHandle;
		AssetManager::GetInstance()->AddImage(img);

		Texture texture;
		texture.ImageIndex = i;
		texture.SamplerIndex = i;
		AssetManager::GetInstance()->Textures.emplace_back(texture);

	}
	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);

	Material mat;
	mat.bIsLit = true;
	mat.Color = Vector4::One;
	mat.AlbedoTextureIndex = 0;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 1;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 2;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 3;
	AssetManager::GetInstance()->Materials.emplace_back(mat);
	mat.AlbedoTextureIndex = 4;
	AssetManager::GetInstance()->Materials.emplace_back(mat);

	//build primitives
	GeometryBuilder geomBuilder;
	geomBuilder.Init(DirectXDevice::GetNativeDevice());
	size_t id = geomBuilder.BuildCube(1.f);
	for (size_t i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildSphere(1.f, 16);
	for (size_t i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildCylinder(1.f, 1.f, 16);
	for (size_t i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildCone(1.f, 1.f, 16);
	for (size_t i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	id = geomBuilder.BuildIcosahedron(1.f);
	for (size_t i = 0; i < 5; ++i) {
		Mesh mesh;
		mesh.Submeshes.push_back({ id, i });
		AssetManager::GetInstance()->Meshes.emplace_back(mesh);
	}
	AssetManager::GetInstance()->UpdateVBOIBO();
}

void ragdoll::Scene::CreateRenderTargets()
{
	MICROPROFILE_SCOPEI("Render", "Create Render Target", MP_YELLOW);
	nvrhi::TextureDesc depthBufferDesc;
	depthBufferDesc.width = SceneInfo.RenderWidth;
	depthBufferDesc.height = SceneInfo.RenderHeight;
	depthBufferDesc.initialState = nvrhi::ResourceStates::DepthWrite;
	depthBufferDesc.isRenderTarget = true;
	depthBufferDesc.sampleCount = 1;
	depthBufferDesc.dimension = nvrhi::TextureDimension::Texture2D;
	depthBufferDesc.keepInitialState = true;
	depthBufferDesc.mipLevels = 1;
	depthBufferDesc.format = nvrhi::Format::D32;
	depthBufferDesc.isTypeless = true;
	depthBufferDesc.debugName = "SceneDepthZ0";
	depthBufferDesc.clearValue = 0.f;
	depthBufferDesc.useClearValue = true;
	RenderTargets.SceneDepthZ0 = DirectXDevice::GetNativeDevice()->createTexture(depthBufferDesc);
	depthBufferDesc.debugName = "SceneDepthZ1";
	RenderTargets.SceneDepthZ1 = DirectXDevice::GetNativeDevice()->createTexture(depthBufferDesc);

	depthBufferDesc.width = 2000;
	depthBufferDesc.height = 2000;
	for (int i = 0; i < 4; ++i)
	{
		depthBufferDesc.debugName = "ShadowMap" + std::to_string(i);
		RenderTargets.ShadowMap[i] = DirectXDevice::GetNativeDevice()->createTexture(depthBufferDesc);
	}

	depthBufferDesc = nvrhi::TextureDesc();
	depthBufferDesc.width = depthBufferDesc.height = 1;
	int MaxIterations = 32, Iterations = 1;
	while (depthBufferDesc.width < SceneInfo.RenderWidth && Iterations++ < MaxIterations)
		depthBufferDesc.width <<= 1;
	Iterations = 0;
	while (depthBufferDesc.height < SceneInfo.RenderHeight && Iterations++ < MaxIterations)
		depthBufferDesc.height <<= 1;
	depthBufferDesc.width >>= 1;
	depthBufferDesc.height >>= 1;
	depthBufferDesc.format = nvrhi::Format::D32;
	depthBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	depthBufferDesc.isUAV = true;
	depthBufferDesc.keepInitialState = true;
	depthBufferDesc.debugName = "HZB";
	depthBufferDesc.dimension = nvrhi::TextureDimension::Texture2DArray;
	depthBufferDesc.arraySize = 1;
	depthBufferDesc.mipLevels = log2(std::max(depthBufferDesc.width, depthBufferDesc.height));
	RenderTargets.HZBMips = DirectXDevice::GetNativeDevice()->createTexture(depthBufferDesc);

	nvrhi::TextureDesc texDesc;
	texDesc.width = SceneInfo.RenderWidth;
	texDesc.height = SceneInfo.RenderHeight;
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.keepInitialState = true;
	texDesc.useClearValue = true;
	texDesc.initialState = nvrhi::ResourceStates::Common;
	texDesc.isUAV = true;
	texDesc.isRenderTarget = true;

	texDesc.clearValue = 1.f;
	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "AONormalized";
	RenderTargets.AONormalized = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.debugName = "Accumulation0";
	RenderTargets.Accumulation0 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "Accumulation1";
	RenderTargets.Accumulation1 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "InputReactivity";
	RenderTargets.InputReactiveMask = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "InputTC";
	RenderTargets.InputTCMask = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	
	texDesc.clearValue = 0.f;
	texDesc.format = nvrhi::Format::RG8_UNORM;
	texDesc.debugName = "GBufferRM";
	RenderTargets.GBufferRM = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA8_UNORM;
	texDesc.debugName = "FinalColor";
	RenderTargets.FinalColor = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA16_FLOAT;
	texDesc.debugName = "TemporalColor0";
	RenderTargets.TemporalColor0 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "TemporalColor1";
	RenderTargets.TemporalColor1 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA16_FLOAT;
	texDesc.debugName = "VelocityBuffer";
	RenderTargets.VelocityBuffer = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R32_UINT;
	texDesc.debugName = "RecontDepth";
	RenderTargets.RecontDepth = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R32_FLOAT;
	texDesc.debugName = "DilatedDepth";
	RenderTargets.DilatedDepth = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R16_FLOAT;
	texDesc.debugName = "FarthestDepth";
	RenderTargets.FarthestDepth = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RG16_FLOAT;
	texDesc.debugName = "DilatedMotionVectors";
	RenderTargets.DilatedMotionVectors = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA8_UNORM;
	texDesc.debugName = "DilatedReactiveMask";
	RenderTargets.DilatedReactiveMask = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R16_FLOAT;
	texDesc.debugName = "Luminance0";
	RenderTargets.Luminance0 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "Luminance1";
	RenderTargets.Luminance1 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "Luma Instability";
	RenderTargets.LumaInstability = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA16_FLOAT;
	texDesc.debugName = "Luminance History0";
	RenderTargets.LuminanceHistory0 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "Luminance History1";
	RenderTargets.LuminanceHistory1 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.width = SceneInfo.TargetWidth;
	texDesc.height = SceneInfo.TargetHeight;

	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "NewLock";
	RenderTargets.NewLocks = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.width = SceneInfo.RenderWidth / 2;
	texDesc.height = SceneInfo.RenderHeight / 2;

	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "ShadingChange";
	RenderTargets.ShadingChange = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R16_FLOAT;
	texDesc.debugName = "FarthestDepthMip";
	RenderTargets.FarthestDepthMip = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.sampleCount = 1;
	texDesc.width = SceneInfo.RenderWidth;
	texDesc.height = SceneInfo.RenderHeight;
	texDesc.isTypeless = false;
	texDesc.isUAV = false;
	texDesc.initialState = nvrhi::ResourceStates::RenderTarget;
	texDesc.format = nvrhi::Format::RGBA8_UNORM;
	texDesc.debugName = "GBufferAlbedo";
	RenderTargets.GBufferAlbedo = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R11G11B10_FLOAT;
	texDesc.debugName = "SceneColor";
	RenderTargets.SceneColor = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA16_UNORM;
	texDesc.debugName = "GBufferNormal";
	RenderTargets.GBufferNormal = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA8_UNORM;
	texDesc.debugName = "ShadowMask";
	RenderTargets.ShadowMask = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.width = 64;
	texDesc.height = 2;
	texDesc.format = nvrhi::Format::RGBA8_UNORM;
	texDesc.debugName = "SkyThetaGammaTable";
	RenderTargets.SkyThetaGammaTable = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.width = texDesc.height = 2048;
	texDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	texDesc.isUAV = true;
	texDesc.format = nvrhi::Format::R11G11B10_FLOAT;
	texDesc.debugName = "SkyTexture";
	RenderTargets.SkyTexture = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	uint32_t width = SceneInfo.RenderWidth;
	uint32_t height = SceneInfo.RenderHeight;
	RenderTargets.DownsampledImages.resize(RenderTargets.MipCount);
	for (size_t i = 0; i < RenderTargets.MipCount; ++i) {
		BloomMip& mip = RenderTargets.DownsampledImages[i];
		mip.Width = width; mip.Height = height;
		nvrhi::TextureDesc desc;
		desc.width = mip.Width;
		desc.height = mip.Height;
		desc.format = nvrhi::Format::R11G11B10_FLOAT;
		desc.initialState = nvrhi::ResourceStates::RenderTarget;
		desc.keepInitialState = true;
		desc.isRenderTarget = true;
		desc.debugName = "DownSample" + std::to_string(mip.Width) + "x" + std::to_string(mip.Height);
		mip.Image = DirectXDevice::GetNativeDevice()->createTexture(desc);
		width /= 2; height /= 2;
	}

	texDesc = nvrhi::TextureDesc();
	texDesc.width = SceneInfo.RenderWidth / 2 + SceneInfo.RenderWidth % 2;
	texDesc.height = SceneInfo.RenderHeight / 2 + SceneInfo.RenderHeight % 2;
	texDesc.format = nvrhi::Format::R16_FLOAT;
	texDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	texDesc.isUAV = true;
	texDesc.keepInitialState = true;
	texDesc.debugName = "DeinterleavedDepth";
	texDesc.dimension = nvrhi::TextureDimension::Texture2DArray;
	texDesc.arraySize = 4;
	texDesc.mipLevels = 5;
	RenderTargets.DeinterleavedDepth = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RGBA8_SNORM;
	texDesc.debugName = "DeinterleavedNormals";
	texDesc.mipLevels = 1;
	RenderTargets.DeinterleavedNormals = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::RG8_UNORM;
	texDesc.debugName = "SSAOBufferPong";
	RenderTargets.SSAOBufferPong = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "SSAOBufferPing";
	RenderTargets.SSAOBufferPing = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "ImportanceMap";
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.arraySize = 1;
	texDesc.width = SceneInfo.RenderWidth / 4 + SceneInfo.RenderWidth % 4;
	texDesc.height = SceneInfo.RenderHeight / 4 + SceneInfo.RenderHeight % 4;
	RenderTargets.ImportanceMap = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "ImportanceMapPong";
	RenderTargets.ImportanceMapPong = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R32_UINT;
	texDesc.debugName = "LoadCounter";
	texDesc.width = texDesc.height = 1;
	texDesc.dimension = nvrhi::TextureDimension::Texture1D;
	RenderTargets.LoadCounter = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc = nvrhi::TextureDesc();
	texDesc.width = SceneInfo.RenderWidth;
	texDesc.height = SceneInfo.RenderHeight;
	texDesc.format = nvrhi::Format::R16_FLOAT;
	texDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	texDesc.isUAV = true;
	texDesc.keepInitialState = true;
	texDesc.debugName = "DepthMip";
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.mipLevels = 5;
	RenderTargets.DepthMip = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.width = SceneInfo.RenderWidth;
	texDesc.height = SceneInfo.RenderHeight;

	texDesc.format = nvrhi::Format::R8_UINT;
	texDesc.debugName = "AOTerm";
	texDesc.mipLevels = 1;
	RenderTargets.AOTerm = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "FinalAOTerm";
	RenderTargets.FinalAOTerm = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "AOTermAccumulation";
	RenderTargets.AOTermAccumulation = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.format = nvrhi::Format::R8_UNORM;
	texDesc.debugName = "EdgeMap";
	RenderTargets.EdgeMap = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc.width = PrimaryWindowRef->GetWidth();
	texDesc.height = PrimaryWindowRef->GetHeight();
	texDesc.format = nvrhi::Format::RGBA8_UNORM;
	texDesc.debugName = "Presentation";
	RenderTargets.PresentationBuffer = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.format = nvrhi::Format::RGBA16_FLOAT;
	texDesc.debugName = "UpscaledBuffer0";
	RenderTargets.UpscaledBuffer0 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "UpscaledBuffer1";
	RenderTargets.UpscaledBuffer1 = DirectXDevice::GetNativeDevice()->createTexture(texDesc);

	texDesc = nvrhi::TextureDesc();
	texDesc.width = 1;
	texDesc.height = 1;
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.keepInitialState = true;
	texDesc.useClearValue = true;
	texDesc.initialState = nvrhi::ResourceStates::Common;
	texDesc.isUAV = true;
	texDesc.isRenderTarget = true;
	texDesc.format = nvrhi::Format::RGBA32_FLOAT;
	texDesc.debugName = "FrameInfo";
	RenderTargets.FrameInfo = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	texDesc.debugName = "SpdAtomic";
	texDesc.format = nvrhi::Format::R32_UINT;
	RenderTargets.SpdAtomic = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
	
	texDesc.width = SceneInfo.RenderWidth / 2;
	texDesc.height = SceneInfo.RenderHeight / 2;
	texDesc.dimension = nvrhi::TextureDimension::Texture2D;
	texDesc.debugName = "SpdMips";
	int levels = 0;
	{
		int32_t tempWidth = SceneInfo.RenderWidth;
		int32_t tempHeight = SceneInfo.RenderHeight;
		while (true) {
			tempWidth = std::max(1, tempWidth / 2);
			tempHeight = std::max(1, tempHeight / 2);
			levels++;
			if (tempWidth == 1 && tempHeight == 1)
				break;
		}
	}
	texDesc.mipLevels = levels;
	texDesc.format = nvrhi::Format::RG16_FLOAT;
	RenderTargets.SpdMips = DirectXDevice::GetNativeDevice()->createTexture(texDesc);
}

void ragdoll::Scene::UpdateTransforms()
{
	RD_SCOPE(Scene, UpdateTransforms);
	TraverseTreeAndUpdateTransforms();
	ResetTransformDirtyFlags();
}

void ragdoll::Scene::ResetTransformDirtyFlags()
{
	auto EcsView = EntityManagerRef->GetRegistry().view<TransformComp>();
	for (const entt::entity& ent : EcsView) {
		TransformComp* comp = EntityManagerRef->GetComponent<TransformComp>(ent);
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
			TransformComp* furthestTrans = EntityManagerRef->GetComponent<TransformComp>(m_FurthestSibling);
			furthestTrans->m_Sibling = entityId;
			m_FurthestSibling = entityId;
		}
	}
}

void ragdoll::Scene::PopulateStaticProxies()
{
	//clear the current proxies
	StaticProxies.clear();
	//iterate through all the transforms and renderable
	auto EcsView = EntityManagerRef->GetRegistry().view<RenderableComp, TransformComp>();
	for (const entt::entity& ent : EcsView) {
		TransformComp* tComp = EntityManagerRef->GetComponent<TransformComp>(ent);
		RenderableComp* rComp = EntityManagerRef->GetComponent<RenderableComp>(ent);
		Mesh mesh = AssetManager::GetInstance()->Meshes[rComp->meshIndex];
		for (const Submesh& submesh : mesh.Submeshes)
		{
			StaticProxies.emplace_back();
			Proxy& Proxy = StaticProxies.back();
			const Material& mat = AssetManager::GetInstance()->Materials[submesh.MaterialIndex];
			if (mat.AlbedoTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.AlbedoTextureIndex];
				Proxy.AlbedoIndex = tex.ImageIndex;
				Proxy.AlbedoSamplerIndex = tex.SamplerIndex;
			}
			if (mat.NormalTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.NormalTextureIndex];
				Proxy.NormalIndex = tex.ImageIndex;
				Proxy.NormalSamplerIndex = tex.SamplerIndex;
			}
			if (mat.RoughnessMetallicTextureIndex != -1)
			{
				const Texture& tex = AssetManager::GetInstance()->Textures[mat.RoughnessMetallicTextureIndex];
				Proxy.ORMIndex = tex.ImageIndex;
				Proxy.ORMSamplerIndex = tex.SamplerIndex;
			}
			Proxy.Color = mat.Color;
			Proxy.Metallic = mat.Metallic;
			Proxy.Roughness = mat.Roughness;
			Proxy.bIsLit = mat.bIsLit;
			Proxy.ModelToWorld = tComp->m_ModelToWorld;
			Proxy.PrevWorldMatrix = tComp->m_PrevModelToWorld;
			Proxy.BufferIndex = (int32_t)submesh.VertexBufferIndex;
			Proxy.MaterialIndex = (int32_t)submesh.MaterialIndex;
			AssetManager::GetInstance()->VertexBufferInfos[Proxy.BufferIndex].BestFitBox.Transform(Proxy.BoundingBox, tComp->m_ModelToWorld);
		}
	}
}

Matrix ComputePlaneTransform(const Vector4& plane, float width, float height)
{
	// Normalize the plane normal
	Vector3 normal = Vector3(plane.x, plane.y, plane.z);

	// Compute plane distance from origin
	float distance = plane.w;

	// Compute center of the plane
	Vector3 center = -distance * normal;

	// Generate orthonormal basis
	Vector3 up = Vector3(0, 1, 0);
	if (fabsf(up.Dot(normal)) > 0.99f)
	{
		up = Vector3(1, 0, 0); // Choose a different up vector if too parallel
	}
	Vector3 xAxis = up.Cross(normal);
	Vector3 yAxis = normal.Cross(xAxis);
	xAxis.Normalize();
	yAxis.Normalize();

	// Construct the rotation matrix
	Matrix rotation = DirectX::XMMATRIX(
		xAxis,         // Right vector (x-axis)
		yAxis,            // Up vector (y-axis)
		normal,       // Forward vector (z-axis)
		DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f)
	);

	// Construct the scaling matrix
	Matrix scale = Matrix::CreateScale(width, height, 0.01f); // Flattened along Z

	// Combine to create the final transform
	Matrix transform = scale * rotation;
	transform *= Matrix::CreateTranslation(center);

	return transform;
}

void ragdoll::Scene::BuildDebugInstances(std::vector<InstanceData>& instances)
{
	instances.clear();
	for (int i = 0; i < StaticProxies.size(); ++i) {
		if (Config.bDrawBoxes) {
			InstanceData debugData;
			//bounding box is already in world, create world matrix from it to transform a 2x2 cube in model space for debug renderer
			Vector3 translate = StaticProxies[i].BoundingBox.Center;
			Vector3 scale = StaticProxies[i].BoundingBox.Extents;
			Matrix matrix = Matrix::CreateScale(scale);
			matrix *= Matrix::CreateTranslation(translate);
			debugData.ModelToWorld = matrix;
			debugData.bIsLit = false;
			debugData.Color = { 0.f, 1.f, 0.f, 1.f };
			instances.emplace_back(debugData);
		}
	}

	if (SceneInfo.EnableCascadeDebug) {
		//draw 4 boxes
		static constexpr Vector4 colors[] = {
			{1.f, 0.f, 0.f, 1.f},
			{1.f, 1.f, 0.f, 1.f},
			{0.f, 1.f, 0.f, 1.f},
			{1.f, 0.f, 1.f, 1.f}
		};
		InstanceData debugData;
		Vector3 scale = { SceneInfo.CascadeInfos[SceneInfo.EnableCascadeDebug - 1].width,
			SceneInfo.CascadeInfos[SceneInfo.EnableCascadeDebug - 1].height,
			abs(SceneInfo.CascadeInfos[SceneInfo.EnableCascadeDebug - 1].farZ - SceneInfo.CascadeInfos[SceneInfo.EnableCascadeDebug - 1].nearZ) };
		Matrix matrix = Matrix::CreateScale(scale);
		DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(-SceneInfo.LightDirection);
		DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f);
		DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUp, forward));
		DirectX::XMVECTOR up = DirectX::XMVector3Cross(forward, right);
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMATRIX(
			right,         // Right vector (x-axis)
			up,            // Up vector (y-axis)
			forward,       // Forward vector (z-axis)
			DirectX::XMVectorSet(0.f, 0.f, 0.f, 1.f)
		);
		matrix *= rotationMatrix;
		matrix *= Matrix::CreateTranslation(SceneInfo.CascadeInfos[SceneInfo.EnableCascadeDebug - 1].center);
		debugData.ModelToWorld = matrix;
		debugData.bIsLit = false;
		debugData.Color = colors[SceneInfo.EnableCascadeDebug - 1];
		instances.emplace_back(debugData);
	}
	
	if (DebugInfo.bShowFrustum)
	{
		//add the 5 planes for the camera
		Vector4 Planes[6];
		if(DebugInfo.bFreezeFrustumCulling)
			GPUScene->ExtractFrustumPlanes(Planes, DebugInfo.FrozenProjection, DebugInfo.FrozenView);
		else
			GPUScene->ExtractFrustumPlanes(Planes, SceneInfo.InfiniteReverseZProj, SceneInfo.MainCameraView);
		for (int i = 0; i < 5; ++i)
		{
			InstanceData debugData;;
			debugData.ModelToWorld = ComputePlaneTransform(Planes[i], 10.f, 10.f);
			debugData.bIsLit = false;
			if(i < 2)
				debugData.Color = { 0.f, 0.f, 1.f, 1.f };
			else if(i < 4)
				debugData.Color = { 1.f, 1.f, 0.f, 1.f };
			else
				debugData.Color = { 0.f, 1.f, 0.f, 1.f };
			instances.emplace_back(debugData);
		}
	}

	if (!instances.empty())
	{
		MICROPROFILE_SCOPEI("Scene", "Building Debug Instance Buffer", MP_DARKRED);
		//add one more cube at where the sun is
		InstanceData debugData;
		Vector3 translate = SceneInfo.LightDirection;
		Vector3 scale = Vector3::One;
		Matrix matrix = Matrix::CreateScale(scale);
		matrix *= Matrix::CreateTranslation(translate);
		debugData.ModelToWorld = matrix;
		debugData.bIsLit = false;
		debugData.Color = { 1.f, 1.f, 1.f, 1.f };
		instances.emplace_back(debugData);

		//create the debug instance buffer
		nvrhi::BufferDesc InstanceBufferDesc;
		InstanceBufferDesc.byteSize = sizeof(InstanceData) * instances.size();
		InstanceBufferDesc.debugName = "Debug instance buffer";
		InstanceBufferDesc.initialState = nvrhi::ResourceStates::CopyDest;
		InstanceBufferDesc.structStride = sizeof(InstanceData);
		StaticInstanceDebugBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);

		//copy data over
		CommandList->open();
		CommandList->beginTrackingBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(StaticInstanceDebugBufferHandle, instances.data(), sizeof(InstanceData) * instances.size());
		CommandList->setPermanentBufferState(StaticInstanceDebugBufferHandle, nvrhi::ResourceStates::ShaderResource);
		CommandList->close();
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	}
}

void ragdoll::Scene::UpdateShadowCascadesExtents()
{
	//for each subfrusta
	for (int i = 1; i < 5; ++i)
	{
		//create the subfrusta in view space
		DirectX::BoundingFrustum frustum;
		Matrix proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(SceneInfo.CameraFov / 2.f), SceneInfo.CameraAspect, SubfrustaFarPlanes[i-1], SubfrustaFarPlanes[i]);
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, proj);
		//move the frustum to world space
		frustum.Transform(frustum, SceneInfo.MainCameraView.Invert());
		//get the corners
		Vector3 corners[8];
		frustum.GetCorners(corners);
		//get the new center
		Vector3 center;
		for (int j = 0; j < 8; ++j) {
			center += corners[j];
		}
		center /= 8.f;
		center.x = (float)(int32_t)center.x;
		center.y = (float)(int32_t)center.y;
		center.z = (float)(int32_t)center.z;
		//move all corners into a 1x1x1 cube lightspace with the directional light
		Matrix lightProj = DirectX::XMMatrixOrthographicLH(1.f, 1.f, -0.5f, 0.5f);	//should be a 1x1x1 cube?
		Vector3 lightDir = -SceneInfo.LightDirection;
		Vector3 up = { 0.f, 1.f, 0.f };
		if (fabsf(lightDir.Dot(up)) > 0.99f) {
			up = { 1.f, 0.f, 0.f };
		}
		SceneInfo.CascadeInfos[i - 1].view = DirectX::XMMatrixLookAtLH(center, center + lightDir, up);
		Matrix lightViewProj = SceneInfo.CascadeInfos[i - 1].view * lightProj;
		//get the furthest extents of the corners for the left right top and bottom values
		Vector3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
		Vector3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		//bring the frustum back from world to light space
		frustum.Transform(frustum, lightViewProj);
		frustum.GetCorners(corners);
		for (int j = 0; j < 8; ++j) {
			max = DirectX::XMVectorMax(max, corners[j]);
			min = DirectX::XMVectorMin(min, corners[j]);
		}
		//get largest width or height to make ortho a square
		float x = max.x - min.x;
		float y = max.y - min.y;
		float length = x < y ? y : x;
		SceneInfo.CascadeInfos[i - 1].center = center;
		SceneInfo.CascadeInfos[i - 1].width = SceneInfo.CascadeInfos[i - 1].height = length;

		//transform the scene bound into the light view space, to get the near and far planes
		DirectX::BoundingBox sceneBound = SceneInfo.SceneBounds;
		sceneBound.Transform(sceneBound, SceneInfo.CascadeInfos[i - 1].view);
		Vector3 sceneCorners[8];
		sceneBound.GetCorners(sceneCorners);
		min = { FLT_MAX, FLT_MAX, FLT_MAX };
		max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int j = 0; j < 8; ++j)
		{
			max = DirectX::XMVectorMax(max, sceneCorners[j]);
			min = DirectX::XMVectorMin(min, sceneCorners[j]);
		}
		SceneInfo.CascadeInfos[i - 1].farZ = max.z;
		SceneInfo.CascadeInfos[i - 1].nearZ = min.z;
	}
}

void ragdoll::Scene::UpdateShadowLightMatrices()
{
	for (int i = 0; i < 4; ++i) {
		//reverse z
		SceneInfo.CascadeInfos[i].proj = DirectX::XMMatrixOrthographicLH(SceneInfo.CascadeInfos[i].width, SceneInfo.CascadeInfos[i].height, SceneInfo.CascadeInfos[i].farZ, SceneInfo.CascadeInfos[i].nearZ);
		SceneInfo.CascadeInfos[i].viewProj = SceneInfo.CascadeInfos[i].view * SceneInfo.CascadeInfos[i].proj;
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
		PrintRecursive(m_RootEntity, 0, EntityManagerRef);
	if (m_RootSibling.m_RawId)
		PrintRecursive(m_RootSibling, 0, EntityManagerRef);
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
	TransformComp* transform = EntityManagerRef->GetComponent<TransformComp>(guid);
	if (transform)
	{
		UpdateTransform(*transform, guid);

		//traverse siblings with a for loop
		while (transform->m_Sibling.m_RawId != 0) {
			transform = EntityManagerRef->GetComponent<TransformComp>(transform->m_Sibling);
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
	//set the prev transform regardless of dirty
	comp.m_PrevModelToWorld = comp.m_ModelToWorld;
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

void ragdoll::Scene::HaltonSequence(Vector2 RenderRes, Vector2 TargetRes)
{
	constexpr uint32_t BasePhaseCount = 8;
	TotalPhaseCount = BasePhaseCount * powf((float)TargetRes.x / (float)RenderRes.x, 2);
	PhaseIndex = 0;
	constexpr int32_t BaseX = 2;
	constexpr int32_t BaseY = 3;
	JitterOffsetsX.resize(TotalPhaseCount);
	JitterOffsetsY.resize(TotalPhaseCount);
	for (int i = 1; i <= TotalPhaseCount; ++i)
	{
		double FractionX{ 1.0 }, ValueX{};
		double FractionY{ 1.0 }, ValueY{};
		uint32_t IndexX = i, IndexY = i;
		while (IndexX > 0)
		{
			FractionX /= (float)BaseX;
			ValueX += FractionX * (IndexX % BaseX);
			IndexX /= BaseX;
		}
		while (IndexY)
		{
			FractionY /= (float)BaseY;
			ValueY += FractionY * (IndexY % BaseY);
			IndexY /= BaseY;
		}
		JitterOffsetsX[i - 1] = (ValueX - 0.5); //range [-0.5, 0.5]
		JitterOffsetsY[i - 1] = (ValueY - 0.5); //range [-0.5, 0.5]
	}
}

void ragdoll::Scene::AddOctantDebug(const Octant& octant, int32_t level)
{
	if (octant.bIsCulled)
		return;
	static constexpr Vector4 colors[] = {
		 {1.0f, 0.0f, 0.0f, 1.0f},  // Red
		{0.0f, 0.0f, 1.0f, 1.0f},  // Blue
		{1.0f, 1.0f, 0.0f, 1.0f},  // Yellow
		{1.0f, 0.0f, 1.0f, 1.0f},  // Magenta
		{0.0f, 1.0f, 1.0f, 1.0f},  // Cyan
		{1.0f, 0.5f, 0.0f, 1.0f},  // Orange
		{0.5f, 0.0f, 0.5f, 1.0f},  // Purple
		{0.5f, 1.0f, 0.5f, 1.0f},  // Light Green
		{0.5f, 0.5f, 1.0f, 1.0f},  // Lavender
		{0.75f, 0.25f, 0.5f, 1.0f},// Pink
		{0.25f, 0.5f, 0.75f, 1.0f},// Steel Blue
		{0.8f, 0.7f, 0.6f, 1.0f},  // Beige
		{0.6f, 0.4f, 0.2f, 1.0f},  // Brown
		{0.9f, 0.9f, 0.9f, 1.0f},  // Light Grey
		{0.4f, 0.4f, 0.4f, 1.0f},  // Dark Grey
		{0.7f, 0.3f, 0.3f, 1.0f},  // Salmon
		{0.2f, 0.6f, 0.2f, 1.0f},  // Dark Green
		{0.3f, 0.7f, 0.8f, 1.0f},  // Sky Blue
		{0.9f, 0.4f, 0.2f, 1.0f},  // Coral
		{0.5f, 0.5f, 0.0f, 1.0f},  // Olive
		{0.6f, 0.2f, 0.8f, 1.0f},  // Violet
		{0.2f, 0.2f, 0.6f, 1.0f},  // Navy Blue
		{0.4f, 0.2f, 0.1f, 1.0f},  // Chocolate
		{0.7f, 0.7f, 0.0f, 1.0f},  // Mustard
		{0.2f, 0.7f, 0.5f, 1.0f},  // Teal
		{0.9f, 0.7f, 0.5f, 1.0f},  // Peach
		{0.7f, 0.4f, 0.4f, 1.0f},  // Light Brown
		{0.5f, 0.8f, 0.6f, 1.0f},  // Mint
		{0.6f, 0.6f, 1.0f, 1.0f},  // Light Blue
		{0.9f, 0.3f, 0.3f, 1.0f},  // Crimson
		{0.3f, 0.9f, 0.6f, 1.0f}   // Aquamarine
	};
	if (level >= Config.DrawOctreeLevelMin && level <= Config.DrawOctreeLevelMax)
	{
		InstanceData debugData;
		Vector3 translate = octant.Box.Center;
		Vector3 scale = octant.Box.Extents;
		Matrix matrix = Matrix::CreateScale(scale);
		matrix *= Matrix::CreateTranslation(translate);
		debugData.ModelToWorld = matrix;
		debugData.bIsLit = false;
		debugData.Color = colors[level];
		StaticDebugInstanceDatas.emplace_back(debugData);
	}
	for (const Octant& itOctant : octant.Octants)
	{
		AddOctantDebug(itOctant, level + 1);
	}
}
