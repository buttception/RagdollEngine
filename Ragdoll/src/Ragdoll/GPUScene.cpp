#include "ragdollpch.h"
#include "GPUScene.h"

#include <nvrhi/utils.h>

#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Profiler.h"

#define INSTANCE_DATA_BUFFER_SRV_SLOT 1
#define MESH_BUFFER_SRV_SLOT 2

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

#define MAX_HZB_MIP_COUNT 16

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
	Vector3 Extents;
};

//used in compute shader to populate indirect calls
struct FMeshData
{
	uint32_t IndexCount;
	uint32_t VertexCount;
	uint32_t IndexOffset;
	uint32_t VertexOffset;
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
	int bIsLit = 1;
};

void ragdoll::FGPUScene::Update(Scene* Scene)
{
	if (InstanceBuffer == nullptr)
		CreateBuffers(Scene->StaticProxies);
}

void ragdoll::FGPUScene::UpdateInstanceBuffer(std::vector<Proxy>& Proxies)
{
	//check if instance buffer is large enough to hold the instances
	if (InstanceBuffer)
	{
		if (InstanceBuffer->getDesc().byteSize < sizeof(InstanceData) * Proxies.size())
		{
			//recreate the buffer
			CreateBuffers(Proxies);
		}
	}
	else
	{
		CreateBuffers(Proxies);
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
		data.IndexOffset = info.IBOffset;
		data.VertexOffset = info.VBOffset;
		data.Center = info.BestFitBox.Center;
		data.Extents = info.BestFitBox.Extents;
	}
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
		data.bIsLit = Material.bIsLit;
	}
	//do not need to sort instances now as it contains mesh indices instead now
	std::vector<FInstanceData> Instances;
	Instances.resize(Proxies.size());
	for (int i = 0; i < Proxies.size(); ++i)
	{
		Instances[i].ModelToWorld = Proxies[i].ModelToWorld;
		Instances[i].PrevModelToWorld = Proxies[i].PrevWorldMatrix;
		Instances[i].MaterialIndex = Proxies[i].MaterialIndex;
		Instances[i].MeshIndex = Proxies[i].BufferIndex;
	}
	//indirect draw args do not need any values
	//get all the bounding boxes and upload onto gpu
	std::vector<FBoundingBox> BoundingBoxes;
	BoundingBoxes.resize(Proxies.size());
	for (int i = 0; i < Proxies.size(); ++i)
	{
		BoundingBoxes[i].Center = Proxies[i].BoundingBox.Center;
		BoundingBoxes[i].Extents = Proxies[i].BoundingBox.Extents;
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

	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
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

nvrhi::BufferHandle ragdoll::FGPUScene::FrustumCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled)
{
	RD_SCOPE(Culling, Instance Culling);
	CommandList->beginMarker("Frustum Cull Scene");
	FConstantBuffer ConstantBuffer;
	ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, Projection, View);
	ConstantBuffer.ProxyCount = ProxyCount;
	ConstantBuffer.Flags |= InfiniteZEnabled ? INFINITE_Z_ENABLED : 0;
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
	nvrhi::BufferDesc IndirectDrawArgsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(nvrhi::DrawIndexedIndirectArguments) * AssetManager::GetInstance()->VertexBufferInfos.size(), "IndirectDrawArgsBuffer");
	IndirectDrawArgsBufferDesc.structStride = sizeof(nvrhi::DrawIndexedIndirectArguments);
	IndirectDrawArgsBufferDesc.canHaveUAVs = true;
	IndirectDrawArgsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	IndirectDrawArgsBufferDesc.keepInitialState = true;
	IndirectDrawArgsBufferDesc.isDrawIndirectArgs = true;
	IndirectDrawArgsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(IndirectDrawArgsBufferDesc);

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
}
