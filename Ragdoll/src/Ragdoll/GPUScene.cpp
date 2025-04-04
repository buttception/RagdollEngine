#include "ragdollpch.h"
#include "GPUScene.h"

#include <nvrhi/utils.h>

#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Profiler.h"

#define INSTANCE_DATA_BUFFER_SRV_SLOT 1
#define MESH_BUFFER_SRV_SLOT 2
#define MATERIAL_BUFFER_SRV_SLOT 3

#define INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT 0
#define INSTANCE_ID_BUFFER_UAV_SLOT 1
#define INSTANCE_VISIBLE_COUNT_UAV_SLOT 2
#define INSTANCE_NOT_OCCLUDED_COUNT_UAV_SLOT 3
#define INSTANCE_OCCLUDED_ID_BUFFER_UAV_SLOT 4
#define INSTANCE_OCCLUDED_COUNT_UAV_SLOT 5

#define DEBUG_BUFFER_UAV_SLOT 9

#define INFINITE_Z_ENABLED 1
#define PREVIOUS_FRAME_ENABLED 1 << 1
#define IS_PHASE_1 1 << 2
#define ALPHA_TEST_ENABLED 1 << 3
#define CULL_ALL 1 << 4
#define ENABLE_INSTANCE_FRUSTUM_CULL 1 << 5
#define ENABLE_INSTANCE_OCCLUSION_CULL 1 << 6
#define ENABLE_AS_FRUSTUM_CULL 1 << 6
#define ENABLE_AS_CONE_CULL 1 << 7
#define ENABLE_AS_OCCLUSION_CULL 1 << 8
#define ENABLE_MESHLET_COLOR 1 << 9

#define MAX_HZB_MIP_COUNT 16

#define MAX_LIGHT_COUNT 1024
#define TILE_SIZE 64
#define DEPTH_SLICE_COUNT 17

struct FConstantBuffer
{
	Matrix ViewMatrix{};
	Matrix ProjectionMatrix{};
	Vector4 FrustumPlanes[6]{};
	uint32_t MipBaseWidth;
	uint32_t MipBaseHeight;
	uint32_t MipLevels;
	uint32_t ProxyCount{};
	uint32_t Flags{};
};

struct FBoundingBox
{
	Vector3 Center;
	float pad0;
	Vector3 Extents;
	float pad1;
};

//used in compute shader to populate indirect calls
struct FMeshData
{
	uint32_t IndexCount;
	uint32_t VertexCount;
	uint32_t IndexOffset;
	uint32_t VertexOffset;

	uint32_t MeshletCount;
	uint32_t MeshletGroupOffset;
	uint32_t MeshletGroupPrimitivesOffset;
	uint32_t MeshletGroupVerticesOffset;

	Vector3 Center;
	Vector3 Extents;
};

struct FInstanceData
{
	Matrix ModelToWorld;
	Matrix PrevModelToWorld;
	uint32_t MeshIndex;
	uint32_t MaterialIndex;
};

#define ALPHA_MODE_OPAQUE 1
#define ALPHA_MODE_MASK 1 << 1
#define ALPHA_MODE_BLEND 1 << 2
#define DOUBLE_SIDED 1 << 3

struct FMaterialData
{
	Vector4 AlbedoFactor = Vector4::One;
	float RoughnessFactor = 0.f;
	float MetallicFactor = 0.f;

	int AlbedoIndex = -1;
	int AlbedoSamplerIndex = 0;
	int NormalIndex = -1;
	int NormalSamplerIndex = 0;
	int ORMIndex = -1;
	int ORMSamplerIndex = 0;

	float AlphaCutoff = 0.5f;
	uint32_t Flags;
};

struct FAmplificationGroupInfo
{
	uint32_t InstanceId;
	uint32_t MeshletGroupOffset;
};

void ragdoll::FGPUScene::Update(Scene* Scene)
{
	//if the projection matrix change, update the frustum aabb
	if (Scene->SceneInfo.PrevMainCameraProj != Scene->SceneInfo.MainCameraProj)
	{
		//prepare the bounding boxes of all the lightgrid
		nvrhi::CommandListHandle CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
		CommandList->open();
		UpdateLightGrid(Scene, CommandList);
		CommandList->close();
		DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	}
	SceneRef = Scene;
}

void ragdoll::FGPUScene::UpdateBuffers(Scene* Scene)
{
	//check if instance buffer is large enough to hold the instances
	if (InstanceBuffer)
	{
		if (InstanceBuffer->getDesc().byteSize < sizeof(InstanceData) * Scene->StaticProxies.size())
		{
			//recreate the buffer
			CreateBuffers(Scene->StaticProxies);
		}
	}
	else
	{
		CreateBuffers(Scene->StaticProxies);
	}
	//update all the buffers
	//get all the relevant mesh details
	std::vector<FMeshData> MeshDatas;
	MeshDatas.resize(AssetManager::GetInstance()->VertexBufferInfos.size());
	for (int i = 0; i < AssetManager::GetInstance()->VertexBufferInfos.size(); ++i)
	{
		const VertexBufferInfo& info = AssetManager::GetInstance()->VertexBufferInfos[i];
		FMeshData& data = MeshDatas[i];
		data.IndexCount = info.IndicesCount;
		data.VertexCount = info.VerticesCount;
		data.IndexOffset = info.IndicesOffset;
		data.VertexOffset = info.VerticesOffset;
		data.MeshletCount = info.MeshletCount;
		data.MeshletGroupOffset = info.MeshletGroupOffset;
		data.MeshletGroupPrimitivesOffset = info.MeshletGroupPrimitivesOffset;
		data.MeshletGroupVerticesOffset = info.MeshletGroupVerticesOffset;
		data.Center = info.BestFitBox.Center;
		data.Extents = info.BestFitBox.Extents;
	}
	//upload all the materials
	std::vector<FMaterialData> MaterialDatas;
	MaterialDatas.resize(AssetManager::GetInstance()->Materials.size());
	for (int i = 0; i < AssetManager::GetInstance()->Materials.size(); ++i)
	{
		const Material& Material = AssetManager::GetInstance()->Materials[i];
		FMaterialData& data = MaterialDatas[i];
		data.AlbedoFactor = Material.Color;
		data.RoughnessFactor = Material.Roughness;
		data.MetallicFactor = Material.Metallic;
		if (Material.AlbedoTextureIndex != -1)
		{
			const Texture& AlbedoTexture = AssetManager::GetInstance()->Textures[Material.AlbedoTextureIndex];
			data.AlbedoIndex = AlbedoTexture.ImageIndex;
			data.AlbedoSamplerIndex = AlbedoTexture.SamplerIndex;
		}
		if (Material.NormalTextureIndex != -1)
		{
			const Texture& NormalTexture = AssetManager::GetInstance()->Textures[Material.NormalTextureIndex];
			data.NormalIndex = NormalTexture.ImageIndex;
			data.NormalSamplerIndex = NormalTexture.SamplerIndex;
		}
		if (Material.RoughnessMetallicTextureIndex != -1)
		{
			const Texture& ORMTexture = AssetManager::GetInstance()->Textures[Material.RoughnessMetallicTextureIndex];
			data.ORMIndex = ORMTexture.ImageIndex;
			data.ORMSamplerIndex = ORMTexture.SamplerIndex;
		}
		data.AlphaCutoff = Material.AlphaCutoff;
		data.Flags = 0;
		if (Material.AlphaMode == Material::AlphaMode::GLTF_OPAQUE)
		{
			data.Flags |= ALPHA_MODE_OPAQUE;
		}
		else if (Material.AlphaMode == Material::AlphaMode::GLTF_MASK)
		{
			data.Flags |= ALPHA_MODE_MASK;
		}
		else if (Material.AlphaMode == Material::AlphaMode::GLTF_BLEND)
		{
			data.Flags |= ALPHA_MODE_BLEND;
		}
		if (Material.bIsDoubleSided)
		{
			data.Flags |= DOUBLE_SIDED;
		}
	}
	//do not need to sort instances now as it contains mesh indices instead now
	std::vector<FInstanceData> Instances;
	Instances.resize(Scene->StaticProxies.size());
	for (int i = 0; i < Scene->StaticProxies.size(); ++i)
	{
		Instances[i].ModelToWorld = Scene->StaticProxies[i].ModelToWorld;
		Instances[i].PrevModelToWorld = Scene->StaticProxies[i].PrevWorldMatrix;
		Instances[i].MaterialIndex = Scene->StaticProxies[i].MaterialIndex;
		Instances[i].MeshIndex = Scene->StaticProxies[i].MeshIndex;
	}
	//indirect draw args do not need any values
	//get all the bounding boxes and upload onto gpu
	std::vector<FBoundingBox> BoundingBoxes;
	BoundingBoxes.resize(Scene->StaticProxies.size());
	for (int i = 0; i < Scene->StaticProxies.size(); ++i)
	{
		BoundingBoxes[i].Center = Scene->StaticProxies[i].BoundingBox.Center;
		BoundingBoxes[i].Extents = Scene->StaticProxies[i].BoundingBox.Extents;
	}

	//get command list from render in the future for multi threading
	nvrhi::CommandListHandle CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	CommandList->open();

	CommandList->beginMarker("Writing Mesh buffer");
	CommandList->beginTrackingBufferState(MeshBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(MeshBuffer, MeshDatas.data(), sizeof(FMeshData) * MeshDatas.size());
	CommandList->setPermanentBufferState(MeshBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();

	CommandList->beginMarker("Writing Material buffer");
	CommandList->beginTrackingBufferState(MaterialBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(MaterialBuffer, MaterialDatas.data(), sizeof(FMaterialData) * MaterialDatas.size());
	CommandList->setPermanentBufferState(MaterialBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();

	CommandList->beginMarker("Writing Instance Data");
	CommandList->beginTrackingBufferState(InstanceBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceBuffer, Instances.data(), sizeof(FInstanceData) * Instances.size());
	CommandList->setPermanentBufferState(InstanceBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();

	RD_ASSERT(Scene->PointLightProxies.size() > MAX_LIGHT_COUNT, "Exceeded maximum number of point lights");
	CommandList->beginMarker("Writing Point Lights Data");
	CommandList->beginTrackingBufferState(PointLightBufferHandle, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(PointLightBufferHandle, Scene->PointLightProxies.data(), sizeof(PointLightProxy) * Scene->PointLightProxies.size());
	CommandList->setPermanentBufferState(PointLightBufferHandle, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();
	PointLightCount = Scene->PointLightProxies.size();

	//create the raytracing resources
	CommandList->beginMarker("Creating BLASs");
	BottomLevelASs.clear();
	for (const VertexBufferInfo& BufferInfo : AssetManager::GetInstance()->VertexBufferInfos)
	{
		nvrhi::rt::GeometryDesc GeomDesc = nvrhi::rt::GeometryDesc();
		nvrhi::rt::AccelStructDesc BLASDesc = nvrhi::rt::AccelStructDesc();
		nvrhi::rt::GeometryTriangles& Triangles = GeomDesc.geometryData.triangles;
		Triangles.vertexBuffer = AssetManager::GetInstance()->VBO;
		Triangles.indexBuffer = AssetManager::GetInstance()->IBO;
		Triangles.indexFormat = nvrhi::Format::R32_UINT;
		Triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
		Triangles.vertexStride = sizeof(Vertex);
		Triangles.vertexCount = BufferInfo.VerticesCount;
		Triangles.indexCount = BufferInfo.IndicesCount;
		//offsets are in bytes
		Triangles.vertexOffset = BufferInfo.VerticesOffset * Triangles.vertexStride;
		Triangles.indexOffset = BufferInfo.IndicesOffset * sizeof(uint32_t);
		GeomDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
		//1 blas per mesh
		BLASDesc.isTopLevel = false;
		BLASDesc.debugName = "BLAS";
		BLASDesc.addBottomLevelGeometry(GeomDesc);
		nvrhi::rt::AccelStructHandle as = DirectXDevice::GetNativeDevice()->createAccelStruct(BLASDesc);
		nvrhi::utils::BuildBottomLevelAccelStruct(CommandList, as, BLASDesc);
		BottomLevelASs.push_back(as);
	}
	CommandList->endMarker();

	//create the new TLAS with all the new instances
	CommandList->beginMarker("Create TLAS");
	std::vector<nvrhi::rt::InstanceDesc> InstanceDescs(Scene->StaticProxies.size());
	for (uint32_t i = 0; i < Scene->StaticProxies.size(); ++i)
	{
		nvrhi::rt::InstanceDesc& InstanceDesc = InstanceDescs[i];
		InstanceDesc.bottomLevelAS = BottomLevelASs[Scene->StaticProxies[i].MeshIndex];
		InstanceDesc.instanceID = i;
		for (int j = 0; j < 3; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				InstanceDesc.transform[j * 4 + k] = Scene->StaticProxies[i].ModelToWorld.m[k][j];
			}
		}
		//if the material for this instance has alpha, mark instance as non opaque and cull disabled
		const Material& Mat = AssetManager::GetInstance()->Materials[Scene->StaticProxies[i].MaterialIndex];
		if(Mat.AlphaMode == Material::AlphaMode::GLTF_OPAQUE)
			InstanceDesc.flags = nvrhi::rt::InstanceFlags::ForceOpaque | nvrhi::rt::InstanceFlags::TriangleFrontCounterclockwise;
		else
			InstanceDesc.flags = nvrhi::rt::InstanceFlags::TriangleCullDisable | nvrhi::rt::InstanceFlags::ForceNonOpaque;
		InstanceDesc.instanceMask = 1;
	}
	nvrhi::rt::AccelStructDesc TLASDesc = nvrhi::rt::AccelStructDesc();
	TLASDesc.debugName = "TLAS";
	TLASDesc.isTopLevel = true;
	TLASDesc.topLevelMaxInstances = Scene->StaticProxies.size();
	TopLevelAS = DirectXDevice::GetNativeDevice()->createAccelStruct(TLASDesc);
	CommandList->buildTopLevelAccelStruct(TopLevelAS, InstanceDescs.data(), InstanceDescs.size());
	CommandList->endMarker();

	//reset the indirect arg
	nvrhi::DispatchMeshleIndirectArguments IndirectArgs;
	IndirectArgs.threadGroupCountX = 0;
	IndirectArgs.threadGroupCountY = IndirectArgs.threadGroupCountZ = 1;
	CommandList->beginMarker("Writing Indirect Args");
	CommandList->writeBuffer(IndirectMeshletArgsBuffer, &IndirectArgs, sizeof(nvrhi::DispatchMeshleIndirectArguments));
	CommandList->close();

	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
	DirectXDevice::GetNativeDevice()->waitForIdle();
}

void ragdoll::FGPUScene::UpdateLightGrid(Scene* Scene, nvrhi::CommandListHandle CommandList)
{
	//dispatch compute to update the light grid bounding boxes
	CommandList->beginMarker("Update Light Grid");
	
	static bool InitalizedDepthBound = false;
	if (!InitalizedDepthBound)
	{
		const float DepthBoundsView[DEPTH_SLICE_COUNT] =
		{
			0.01f, 5.f, 6.8f, 9.2f, 12.6f, 17.1f, 23.2f, 31.5f, 42.8f, 58.2f, 79.1f, 107.6f, 146.4f, 199.1f, 271.1f, 368.7f, 500.f
		};
		float DepthBoundsCS[DEPTH_SLICE_COUNT];
		for (int i = 0; i < DEPTH_SLICE_COUNT; ++i)
		{
			Vector4 Point = Vector4(0, 0, DepthBoundsView[i], 1);
			Vector4 ProjectedPoint = Vector4::Transform(Point, Scene->SceneInfo.MainCameraProj);
			DepthBoundsCS[i] = ProjectedPoint.z / ProjectedPoint.w;
			//values here a negative, but will become positive again in the shader
			//DepthBoundsCS[i] = -DepthBoundsCS[i];
		}
		CommandList->beginTrackingBufferState(DepthSliceBoundsClipspaceBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(DepthSliceBoundsClipspaceBufferHandle, DepthBoundsCS, DEPTH_SLICE_COUNT * sizeof(float));
		CommandList->setPermanentBufferState(DepthSliceBoundsClipspaceBufferHandle, nvrhi::ResourceStates::ShaderResource);
		CommandList->beginTrackingBufferState(DepthSliceBoundsViewspaceBufferHandle, nvrhi::ResourceStates::CopyDest);
		CommandList->writeBuffer(DepthSliceBoundsViewspaceBufferHandle, DepthBoundsView, DEPTH_SLICE_COUNT * sizeof(float));
		CommandList->setPermanentBufferState(DepthSliceBoundsViewspaceBufferHandle, nvrhi::ResourceStates::ShaderResource);
		InitalizedDepthBound = true;
	}

	struct FLightGridConstantBuffer
	{
		Matrix InvProjection;
		uint32_t Width;
		float InvWidth;
		uint32_t Height;
		float InvHeight;
	} CBuffer;
	CBuffer.InvProjection = Scene->SceneInfo.MainCameraProjWithJitter.Invert();
	CBuffer.Width = TileCountX;
	CBuffer.Height = TileCountY;
	CBuffer.InvWidth = 1.f / CBuffer.Width;
	CBuffer.InvHeight = 1.f / CBuffer.Height;
	
	//create the constant buffer
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FLightGridConstantBuffer), "LightGrid ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(FLightGridConstantBuffer));
	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, DepthSliceBoundsClipspaceBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(0, LightGridBoundingBoxBufferHandle),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("UpdateBoundingBox.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(CBuffer.Width / 8 + (CBuffer.Width % 8 ? 1 : 0), CBuffer.Height / 8 + (CBuffer.Height % 8 ? 1 : 0), 1);
	CommandList->endMarker();
}

void ragdoll::FGPUScene::CullLightGrid(const SceneInformation& SceneInfo, nvrhi::CommandListHandle CommandList, ragdoll::SceneRenderTargets* RenderTargets)
{
	CommandList->beginMarker("Cull Light Grid");
	struct FLightGridCullConstantBuffer {
		Matrix View;
		uint32_t LightGridCount;
		uint32_t LightCount;
		uint32_t TextureWidth;
		uint32_t TextureHeight;
		uint32_t FieldsNeeded;
	} CBuffer;
	CBuffer.View = SceneInfo.MainCameraView * DirectX::XMMatrixScaling(1.f, 1.f, -1.f);
	CBuffer.LightGridCount = LightGridCount;
	CBuffer.LightCount = PointLightCount;
	CBuffer.TextureWidth = TileCountX;
	CBuffer.TextureHeight = TileCountY;
	CBuffer.FieldsNeeded = FieldsNeeded;

	//create the constant buffer
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FLightGridCullConstantBuffer), "LightGridCull ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(FLightGridCullConstantBuffer));
	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(0, LightGridBoundingBoxBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(1, PointLightBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(2, DepthSliceBoundsViewspaceBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(0, LightBitFieldsBufferHandle),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("CullLights.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(LightGridCount / 64 + (LightGridCount % 64 ? 1 : 0), 1, 1);
	CommandList->endMarker();
}

void ragdoll::FGPUScene::ExtractFrustumPlanes(Vector4 OutPlanes[6], const Matrix& Projection, const Matrix& View)
{
	Matrix ViewProjection = View * Projection;
	// Left plane
	OutPlanes[0] = Vector4(
		ViewProjection.m[0][3] + ViewProjection.m[0][0],
		ViewProjection.m[1][3] + ViewProjection.m[1][0],
		ViewProjection.m[2][3] + ViewProjection.m[2][0],
		ViewProjection.m[3][3] + ViewProjection.m[3][0]
	);

	// Right plane
	OutPlanes[1] = Vector4(
		ViewProjection.m[0][3] - ViewProjection.m[0][0],
		ViewProjection.m[1][3] - ViewProjection.m[1][0],
		ViewProjection.m[2][3] - ViewProjection.m[2][0],
		ViewProjection.m[3][3] - ViewProjection.m[3][0]
	);

	// Bottom plane
	OutPlanes[2] = Vector4(
		ViewProjection.m[0][3] + ViewProjection.m[0][1],
		ViewProjection.m[1][3] + ViewProjection.m[1][1],
		ViewProjection.m[2][3] + ViewProjection.m[2][1],
		ViewProjection.m[3][3] + ViewProjection.m[3][1]
	);

	// Top plane
	OutPlanes[3] = Vector4(
		ViewProjection.m[0][3] - ViewProjection.m[0][1],
		ViewProjection.m[1][3] - ViewProjection.m[1][1],
		ViewProjection.m[2][3] - ViewProjection.m[2][1],
		ViewProjection.m[3][3] - ViewProjection.m[3][1]
	);

	// Near plane
	OutPlanes[4] = Vector4(
		ViewProjection.m[0][3] + ViewProjection.m[0][2],
		ViewProjection.m[1][3] + ViewProjection.m[1][2],
		ViewProjection.m[2][3] + ViewProjection.m[2][2],
		ViewProjection.m[3][3] + ViewProjection.m[3][2]
	);

	// Far plane
	OutPlanes[5] = Vector4(
		ViewProjection.m[0][3] - ViewProjection.m[0][2],
		ViewProjection.m[1][3] - ViewProjection.m[1][2],
		ViewProjection.m[2][3] - ViewProjection.m[2][2],
		ViewProjection.m[3][3] - ViewProjection.m[3][2]
	);

	// Normalize the planes
	for (int i = 0; i < 6; ++i)
	{
		float Length = Vector3(OutPlanes[i].x, OutPlanes[i].y, OutPlanes[i].z).Length();
		OutPlanes[i] /= Length;
	}
}

nvrhi::BufferHandle ragdoll::FGPUScene::FrustumCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled, uint32_t AlphaTest)
{
	RD_SCOPE(Culling, Instance Culling);
	CommandList->beginMarker("Frustum Cull Scene");
	FConstantBuffer ConstantBuffer;
	ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, Projection, View);
	ConstantBuffer.ProxyCount = ProxyCount;
	ConstantBuffer.Flags |= InfiniteZEnabled ? INFINITE_Z_ENABLED : 0;
	//decides to include or exclude the alpha test flag
	ConstantBuffer.Flags |= AlphaTest == 0 ? CULL_ALL : 0;
	ConstantBuffer.Flags |= AlphaTest == 2 ? ALPHA_TEST_ENABLED : 0;
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "InstanceCull ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));

	uint32_t VisibleCount{};
	CommandList->writeBuffer(PassedFrustumTestCountBuffer, &VisibleCount, sizeof(uint32_t));

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_DATA_BUFFER_SRV_SLOT, InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(MESH_BUFFER_SRV_SLOT, MeshBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(MATERIAL_BUFFER_SRV_SLOT, MaterialBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT, IndirectDrawArgsBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_ID_BUFFER_UAV_SLOT, InstanceIdBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_VISIBLE_COUNT_UAV_SLOT, PassedFrustumTestCountBuffer),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("FrustumCull.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(ProxyCount / 64 + 1, 1, 1);
	CommandList->endMarker();
	return PassedFrustumTestCountBuffer;
}

void ragdoll::FGPUScene::OcclusionCullPhase1(
	nvrhi::CommandListHandle CommandList,
	SceneRenderTargets* Targets,
	Matrix ViewMatrix,
	Matrix ProjectionMatrix,
	nvrhi::BufferHandle FrustumVisibleCountBuffer,
	nvrhi::BufferHandle& PassedOcclusionCountOutput,
	nvrhi::BufferHandle& FailedOcclusionCountOutput,
	uint32_t ProxyCount
)
{
	RD_SCOPE(Culling, Occlusion Cull Phase 1);
	CommandList->beginMarker("Occlusion Cull Phase 1");
	FConstantBuffer ConstantBuffer;
	ConstantBuffer.ViewMatrix = ViewMatrix;
	ConstantBuffer.ProjectionMatrix = ProjectionMatrix;
	ConstantBuffer.Flags |= PREVIOUS_FRAME_ENABLED | IS_PHASE_1;
	ConstantBuffer.MipBaseWidth = Targets->HZBMips->getDesc().width;
	ConstantBuffer.MipBaseHeight = Targets->HZBMips->getDesc().height;
	ConstantBuffer.MipLevels = Targets->HZBMips->getDesc().mipLevels;

	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "InstanceCull ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));

	//buffer containing non occluded count to draw after phase 1
	uint32_t VisibleCount{};
	CommandList->writeBuffer(Phase1NonOccludedCountBuffer, &VisibleCount, sizeof(uint32_t));
	PassedOcclusionCountOutput = Phase1NonOccludedCountBuffer;

	//buffer containing the count of items that failed the culling test to be tested again in phase 2
	uint32_t Occluded{};
	CommandList->writeBuffer(Phase1OccludedCountBuffer, &Occluded, sizeof(uint32_t));
	FailedOcclusionCountOutput = Phase1OccludedCountBuffer;

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, Targets->HZBMips, nvrhi::Format::D32, nvrhi::AllSubresources),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp_Reduction]),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_DATA_BUFFER_SRV_SLOT, InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(MESH_BUFFER_SRV_SLOT, MeshBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT, IndirectDrawArgsBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_ID_BUFFER_UAV_SLOT, InstanceIdBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_VISIBLE_COUNT_UAV_SLOT, FrustumVisibleCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_NOT_OCCLUDED_COUNT_UAV_SLOT, Phase1NonOccludedCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_OCCLUDED_ID_BUFFER_UAV_SLOT, OccludedInstanceIdBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_OCCLUDED_COUNT_UAV_SLOT, Phase1OccludedCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(DEBUG_BUFFER_UAV_SLOT, DebugBuffer),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("OcclusionCull.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(ProxyCount / 64 + 1, 1, 1);
	CommandList->endMarker();
}

nvrhi::BufferHandle ragdoll::FGPUScene::OcclusionCullPhase2(
	nvrhi::CommandListHandle CommandList,
	SceneRenderTargets* Targets,
	Matrix ViewMatrix,
	Matrix ProjectionMatrix,
	nvrhi::BufferHandle Phase1OccludedCountBuffer,
	uint32_t ProxyCount)
{
	RD_SCOPE(Culling, Occlusion Cull Phase 2);
	CommandList->beginMarker("Occlusion Cull Phase 2");
	FConstantBuffer ConstantBuffer;
	ConstantBuffer.ViewMatrix = ViewMatrix;
	ConstantBuffer.ProjectionMatrix = ProjectionMatrix;
	ConstantBuffer.Flags = 0;
	ConstantBuffer.MipBaseWidth = Targets->HZBMips->getDesc().width;
	ConstantBuffer.MipBaseHeight = Targets->HZBMips->getDesc().height;
	ConstantBuffer.MipLevels = Targets->HZBMips->getDesc().mipLevels;

	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "InstanceCull ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));

	uint32_t VisibleCount{};
	CommandList->writeBuffer(Phase2NonOccludedCountBuffer, &VisibleCount, sizeof(uint32_t));

	uint32_t Occluded{};
	CommandList->writeBuffer(Phase2OccludedCountBuffer, &Occluded, sizeof(uint32_t));

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, Targets->HZBMips, nvrhi::Format::D32, nvrhi::AllSubresources),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp_Reduction]),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_DATA_BUFFER_SRV_SLOT, InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(MESH_BUFFER_SRV_SLOT, MeshBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT, IndirectDrawArgsBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_ID_BUFFER_UAV_SLOT, InstanceIdBuffer),
		//reuse as the number of occluded objects, because i am testing every object that failed occlusion in phase 1
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_VISIBLE_COUNT_UAV_SLOT, Phase1OccludedCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_NOT_OCCLUDED_COUNT_UAV_SLOT, Phase2NonOccludedCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_OCCLUDED_ID_BUFFER_UAV_SLOT, OccludedInstanceIdBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_OCCLUDED_COUNT_UAV_SLOT, Phase2OccludedCountBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(DEBUG_BUFFER_UAV_SLOT, DebugBuffer),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("OcclusionCull.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(ProxyCount / 64 + 1, 1, 1);
	CommandList->endMarker();

	return Phase2NonOccludedCountBuffer;
}

void ragdoll::FGPUScene::BuildHZB(nvrhi::CommandListHandle CommandList, SceneRenderTargets* Targets)
{
	RD_SCOPE(HZB, BuildHZB);
	CommandList->beginMarker("Build HZB");
	struct FConstantBuffer
	{
		uint32_t OutputWidth, OutputHeight;
		uint32_t MipLevel;
	} ConstantBuffer;
	ConstantBuffer.OutputWidth = Targets->HZBMips->getDesc().width;
	ConstantBuffer.OutputHeight = Targets->HZBMips->getDesc().height;
	ConstantBuffer.MipLevel = 0;
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "HZB ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));

	//create 1 binding set for use for all mip passes
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, Targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Sampler(0, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp]),
	};
	//add all of the mip levels
	RD_ASSERT(Targets->HZBMips->getDesc().mipLevels >= MAX_HZB_MIP_COUNT, "Too many mip levels for HZB");
	for (uint32_t i = 0; i < MAX_HZB_MIP_COUNT; ++i)
	{
		if (i < Targets->HZBMips->getDesc().mipLevels)
		{
			BindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(i, Targets->HZBMips, nvrhi::Format::D32, { i, 1, 0, 1 }));
		}
		else
		{
			BindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_UAV(i, Targets->HZBMips, nvrhi::Format::D32, { 0, 1, 0, 1 }));
		}
	}
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.addBindingLayout(BindingLayoutHandle);
	nvrhi::ShaderHandle ComputeShader = AssetManager::GetInstance()->GetShader("DownSamplePoT.cs.cso");
	PipelineDesc.CS = ComputeShader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);

	for (uint32_t i = 0; i < Targets->HZBMips->getDesc().mipLevels; ++i)
	{
		RD_SCOPE(HZB, DownSample);
		ConstantBuffer.MipLevel = i;
		CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));

		uint32_t DestWidth = ConstantBuffer.OutputWidth >> 1;
		uint32_t DestHeight = ConstantBuffer.OutputHeight >> 1;
		CommandList->dispatch(ConstantBuffer.OutputWidth / 8 + 1, ConstantBuffer.OutputHeight / 8 + 1, 1);
		ConstantBuffer.OutputWidth = DestWidth;
		ConstantBuffer.OutputHeight = DestHeight;
	}
	CommandList->endMarker();
}

void ragdoll::FGPUScene::MeshletInstanceCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled, uint32_t Flags)
{
	RD_SCOPE(Culling, Instance Culling);
	CommandList->beginMarker("Frustum Cull Scene");
	{
		//create the binding set
		nvrhi::BindingSetDesc BindingSetDesc;
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::StructuredBuffer_UAV(1, IndirectMeshletArgsBuffer),
		};
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

		//pipeline descs
		nvrhi::ComputePipelineDesc CullingPipelineDesc;
		CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
		nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("ResetMeshlet.cs.cso");
		CullingPipelineDesc.CS = CullingShader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
		state.bindings = { BindingSetHandle };
		CommandList->setComputeState(state);
		CommandList->dispatch(1, 1, 1);
	}
#if 1
	{
		struct FMSConstantBuffer
		{
			Matrix ViewMatrix{};
			Matrix ProjectionMatrix{};
			Vector4 FrustumPlanes[6]{};
			Vector3 CameraPosition;
			uint32_t MipBaseWidth;
			uint32_t MipBaseHeight;
			uint32_t MipLevels;
			uint32_t ProxyCount{};
			uint32_t Flags{};
		} ConstantBuffer;
		ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, Projection, View);
		ConstantBuffer.ProxyCount = ProxyCount;
		ConstantBuffer.Flags |= Flags;
		nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FMSConstantBuffer), "InstanceCull ConstantBuffer", 1));
		CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FMSConstantBuffer));

		//create the binding set
		nvrhi::BindingSetDesc BindingSetDesc;
		BindingSetDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(0, InstanceBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_SRV(6, MeshBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(0, AmplificationGroupInfoBuffer),
			nvrhi::BindingSetItem::StructuredBuffer_UAV(1, IndirectMeshletArgsBuffer),
		};
		nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
		nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

		//pipeline descs
		nvrhi::ComputePipelineDesc CullingPipelineDesc;
		CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
		nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("MeshletCull.cs.cso");
		CullingPipelineDesc.CS = CullingShader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
		state.bindings = { BindingSetHandle };
		CommandList->setComputeState(state);
		CommandList->dispatch((ProxyCount + 63) / 64, 1, 1);
	}
#endif
	CommandList->endMarker();
}

void ragdoll::FGPUScene::CreateBuffers(const std::vector<Proxy>& Proxies)
{
	//create the buffers
	//mesh buffer
	nvrhi::BufferDesc MeshBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(FMeshData) * AssetManager::GetInstance()->VertexBufferInfos.size(), "MeshBuffer");
	MeshBufferDesc.structStride = sizeof(FMeshData);
	MeshBuffer = DirectXDevice::GetNativeDevice()->createBuffer(MeshBufferDesc);
	//material buffer
	nvrhi::BufferDesc MaterialBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(FMaterialData) * AssetManager::GetInstance()->Materials.size(), "MaterialBuffer");
	MaterialBufferDesc.structStride = sizeof(FMaterialData);
	MaterialBuffer = DirectXDevice::GetNativeDevice()->createBuffer(MaterialBufferDesc);
	//instance buffer
	nvrhi::BufferDesc InstanceBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(FInstanceData) * Proxies.size(), "InstanceBuffer");
	InstanceBufferDesc.structStride = sizeof(FInstanceData);
	InstanceBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);
	//instance id buffer
	nvrhi::BufferDesc InstanceIdBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(int32_t) * Proxies.size(), "InstanceIdsBuffer");
	InstanceIdBufferDesc.structStride = sizeof(int32_t);
	InstanceIdBufferDesc.canHaveUAVs = true;
	InstanceIdBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	InstanceIdBufferDesc.keepInitialState = true;
	InstanceIdBufferDesc.isVertexBuffer = true;
	InstanceIdBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceIdBufferDesc);	  //worst case size
	//instance id buffer
	InstanceIdBufferDesc.debugName = "OccludedInstanceIdsBuffer";
	OccludedInstanceIdBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceIdBufferDesc);	  //worst case size
	
	//create the indirect draw args buffer
	nvrhi::BufferDesc IndirectDrawArgsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(nvrhi::DrawIndexedIndirectArguments) * Proxies.size(), "IndirectDrawArgsBuffer");
	IndirectDrawArgsBufferDesc.structStride = sizeof(nvrhi::DrawIndexedIndirectArguments);
	IndirectDrawArgsBufferDesc.canHaveUAVs = true;
	IndirectDrawArgsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	IndirectDrawArgsBufferDesc.keepInitialState = true;
	IndirectDrawArgsBufferDesc.isDrawIndirectArgs = true;
	IndirectDrawArgsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(IndirectDrawArgsBufferDesc);

	//create the indirect meshlet arg buffer
	nvrhi::BufferDesc IndirectMeshletArgsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(nvrhi::DispatchMeshleIndirectArguments), "IndirectMeshletArgsBuffer");
	IndirectMeshletArgsBufferDesc.structStride = sizeof(nvrhi::DispatchMeshleIndirectArguments);
	IndirectMeshletArgsBufferDesc.canHaveUAVs = true;
	IndirectMeshletArgsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	IndirectMeshletArgsBufferDesc.keepInitialState = true;
	IndirectMeshletArgsBufferDesc.isDrawIndirectArgs = true;
	IndirectMeshletArgsBufferDesc.isConstantBuffer = false;
	IndirectMeshletArgsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(IndirectMeshletArgsBufferDesc);

	nvrhi::BufferDesc AmplificationGroupInfoBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(FAmplificationGroupInfo) * 65535, "AmplificationGroupInfoBuffer");
	AmplificationGroupInfoBufferDesc.structStride = sizeof(uint32_t);
	AmplificationGroupInfoBufferDesc.canHaveUAVs = true;
	AmplificationGroupInfoBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	AmplificationGroupInfoBufferDesc.keepInitialState = true;
	AmplificationGroupInfoBuffer = DirectXDevice::GetNativeDevice()->createBuffer(AmplificationGroupInfoBufferDesc);

	nvrhi::BufferDesc DebugBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(float) * Proxies.size(), "DebugBuffer");
	DebugBufferDesc.structStride = sizeof(float);
	DebugBufferDesc.canHaveUAVs = true;
	DebugBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	DebugBufferDesc.keepInitialState = true;
	DebugBuffer = DirectXDevice::GetNativeDevice()->createBuffer(DebugBufferDesc);

	//create all of the count buffers
	nvrhi::BufferDesc CountBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t), "PassedFrustumTestCountBuffer");
	CountBufferDesc.structStride = sizeof(uint32_t);
	CountBufferDesc.canHaveUAVs = true;
	CountBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	CountBufferDesc.keepInitialState = true;
	CountBufferDesc.isDrawIndirectArgs = true;
	PassedFrustumTestCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "Phase1NonOccludedCountBuffer";
	Phase1NonOccludedCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "Phase1OccludedCountBuffer";
	Phase1OccludedCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "Phase2NonOccludedCountBuffer";
	Phase2NonOccludedCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CountBufferDesc.debugName = "Phase2OccludedCountBuffer";
	Phase2OccludedCountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);

	nvrhi::BufferDesc PointLightBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(PointLightProxy) * MAX_LIGHT_COUNT, "PointLightBuffer");
	PointLightBufferDesc.structStride = sizeof(PointLightProxy);
	PointLightBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(PointLightBufferDesc);
}

void ragdoll::FGPUScene::CreateLightGrid(Scene* Scene)
{
	TileCountX = Scene->SceneInfo.RenderWidth / TILE_SIZE + (Scene->SceneInfo.RenderWidth % TILE_SIZE ? 1 : 0);
	TileCountY = Scene->SceneInfo.RenderHeight / TILE_SIZE + (Scene->SceneInfo.RenderHeight % TILE_SIZE ? 1 : 0);
	TileCountZ = DEPTH_SLICE_COUNT - 1;

	LightGridCount = TileCountX * TileCountY * TileCountZ;
	nvrhi::BufferDesc LightGridBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(FBoundingBox) * LightGridCount, "LightGridBoundingBoxBuffer");
	LightGridBufferDesc.structStride = sizeof(FBoundingBox);
	LightGridBufferDesc.canHaveUAVs = true;
	LightGridBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	LightGridBufferDesc.keepInitialState = true;
	LightGridBoundingBoxBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(LightGridBufferDesc);

	FieldsNeeded = Scene->PointLightProxies.size() / 32 + (Scene->PointLightProxies.size() % 32 ? 1 : 0);
	if (FieldsNeeded == 0)
	{
		FieldsNeeded = 1;
	}
	nvrhi::BufferDesc LightBitFieldsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t) * FieldsNeeded * LightGridCount, "LightBitFieldsBuffer");
	LightBitFieldsBufferDesc.structStride = sizeof(uint32_t);
	LightBitFieldsBufferDesc.canHaveUAVs = true;
	LightBitFieldsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	LightBitFieldsBufferDesc.keepInitialState = true;
	LightBitFieldsBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(LightBitFieldsBufferDesc);

	DepthSliceBoundsClipspaceBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateStaticConstantBufferDesc(DEPTH_SLICE_COUNT * sizeof(float), "LightGridDepthBoundsClipspaceBuffer").setStructStride(sizeof(float)));
	DepthSliceBoundsViewspaceBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateStaticConstantBufferDesc(DEPTH_SLICE_COUNT * sizeof(float), "LightGridDepthBoundsViewspaceBuffer").setStructStride(sizeof(float)));
}
