#include "ragdollpch.h"
#include "GPUScene.h"

#include <nvrhi/utils.h>

#include "DirectXDevice.h"
#include "AssetManager.h"
#include "Profiler.h"

#define INSTANCE_DATA_BUFFER_SRV_SLOT 0
#define INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT 1
#define MESH_BUFFER_SRV_SLOT 2

#define INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT 0
#define INSTANCE_ID_BUFFER_UAV_SLOT 1
#define INSTANCE_VISIBLE_COUNT_UAV_SLOT 2

struct FConstantBuffer
{
	Matrix ViewMatrix{};
	Vector4 FrustumPlanes[6]{};
	uint32_t ProxyCount{};
	uint32_t InfiniteZEnabled{};
};

struct FBoundingBox
{
	Vector3 Center;
	float pad0;
	Vector3 Extents;
	float pad1;
};

//used in compute shader to populate indirect calls
struct MeshData
{
	uint32_t IndexCount;
	uint32_t VertexCount;
	uint32_t IndexOffset;
	uint32_t VertexOffset;
	//TODO: bounding box if needed in the future
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
	std::vector<MeshData> MeshDatas;
	MeshDatas.resize(AssetManager::GetInstance()->VertexBufferInfos.size());
	for (int i = 0; i < AssetManager::GetInstance()->VertexBufferInfos.size(); ++i)
	{
		const VertexBufferInfo& info = AssetManager::GetInstance()->VertexBufferInfos[i];
		MeshData data;
		data.IndexCount = info.IndicesCount;
		data.VertexCount = info.VerticesCount;
		data.IndexOffset = info.IBOffset;
		data.VertexOffset = info.VBOffset;
		MeshDatas[i] = data;
	}
	//get a instance data buffer copy
	std::sort(Proxies.begin(), Proxies.end(), [](const Proxy& a, const Proxy& b) { return a.BufferIndex < b.BufferIndex; });
	//get a copy of all proxies as instances then sort them
	std::vector<InstanceData> Instances;
	Instances.resize(Proxies.size());
	for (int i = 0; i < Proxies.size(); ++i)
	{
		Instances[i] = Proxies[i];
		Instances[i].MeshIndex = Proxies[i].BufferIndex;
	}
	//get all the mesh index offsets for the proxies
	std::vector<uint32_t> Offsets;
	Offsets.resize(AssetManager::GetInstance()->VertexBufferInfos.size());
	uint32_t Offset = 0;
	uint32_t MeshIndex = Proxies[0].BufferIndex;
	for (int i = 0; i < Proxies.size(); ++i)
	{
		if (Proxies[i].BufferIndex != MeshIndex)
		{
			Offsets[Proxies[i].BufferIndex] = Offset;
			MeshIndex = Proxies[i].BufferIndex;
		}
		Offset++;
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
	CommandList->writeBuffer(MeshBuffer, MeshDatas.data(), sizeof(MeshData) * MeshDatas.size());
	CommandList->setPermanentBufferState(MeshBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();

	CommandList->beginMarker("Writing Instance Data");
	CommandList->beginTrackingBufferState(InstanceBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceBuffer, Instances.data(), sizeof(InstanceData) * Instances.size());
	CommandList->setPermanentBufferState(InstanceBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();

	CommandList->beginMarker("Writing Bounding Box Data");
	CommandList->beginTrackingBufferState(InstanceBoundingBoxBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceBoundingBoxBuffer, BoundingBoxes.data(), sizeof(FBoundingBox) * BoundingBoxes.size());
	CommandList->setPermanentBufferState(InstanceBoundingBoxBuffer, nvrhi::ResourceStates::ShaderResource);
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

nvrhi::BufferHandle ragdoll::FGPUScene::InstanceCull(nvrhi::CommandListHandle CommandList, const Matrix& Projection, const Matrix& View, uint32_t ProxyCount, bool InfiniteZEnabled)
{
	RD_SCOPE(Culling, Instance Culling);
	CommandList->beginMarker("Instance Culling");
	FConstantBuffer ConstantBuffer;
	ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, Projection, View);
	ConstantBuffer.ViewMatrix = View;//create a constant buffer here
	ConstantBuffer.ProxyCount = ProxyCount;
	ConstantBuffer.InfiniteZEnabled = InfiniteZEnabled;
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(FConstantBuffer), "InstanceCull ConstantBuffer", 1));
	CommandList->writeBuffer(ConstantBufferHandle, &ConstantBuffer, sizeof(FConstantBuffer));
	uint32_t VisibleCount{};

	nvrhi::BufferDesc CountBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t), "CountBuffer");
	CountBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	CountBufferDesc.keepInitialState = true;
	CountBufferDesc.canHaveUAVs = true;
	CountBufferDesc.isDrawIndirectArgs = true;
	CountBufferDesc.structStride = sizeof(uint32_t);
	nvrhi::BufferHandle CountBuffer = DirectXDevice::GetNativeDevice()->createBuffer(CountBufferDesc);
	CommandList->writeBuffer(CountBuffer, &VisibleCount, sizeof(uint32_t));

	//create the binding set
	nvrhi::BindingSetDesc BindingSetDesc;
	BindingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_DATA_BUFFER_SRV_SLOT, InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT, InstanceBoundingBoxBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(MESH_BUFFER_SRV_SLOT, MeshBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT, IndirectDrawArgsBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_ID_BUFFER_UAV_SLOT, InstanceIdBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_VISIBLE_COUNT_UAV_SLOT, CountBuffer),
	};
	nvrhi::BindingLayoutHandle BindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(BindingSetDesc);
	nvrhi::BindingSetHandle BindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(BindingSetDesc, BindingLayoutHandle);

	//frustum cull the scene
	FrustumCullScene(CommandList, ConstantBufferHandle, BindingSetHandle, ProxyCount);
	CommandList->endMarker();
	return CountBuffer;
}

void ragdoll::FGPUScene::FrustumCullScene(nvrhi::CommandListHandle CommandList, nvrhi::BufferHandle ConstantBufferHandle, nvrhi::BindingSetHandle BindingSetHandle, uint32_t ProxyCount)
{
	RD_SCOPE(Culling, Frustum Culling);
	//pipeline descs
	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { BindingSetHandle->getLayout() };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("InstanceCull.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	CommandList->beginMarker("Frustum Cull Scene");
	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { BindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(ProxyCount / 64 + 1, 1, 1);
	CommandList->endMarker();
}

void ragdoll::FGPUScene::CreateBuffers(const std::vector<Proxy>& Proxies)
{
	//create the buffers
	//mesh buffer
	nvrhi::BufferDesc MeshBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(MeshData) * AssetManager::GetInstance()->VertexBufferInfos.size(), "MeshBuffer");
	MeshBufferDesc.structStride = sizeof(MeshData);
	MeshBuffer = DirectXDevice::GetNativeDevice()->createBuffer(MeshBufferDesc);	  //worst case size

	//instance buffer
	nvrhi::BufferDesc InstanceBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(InstanceData) * Proxies.size(), "InstanceBuffer");
	InstanceBufferDesc.structStride = sizeof(InstanceData);
	InstanceBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);	   //worst case size
	//instance id buffer
	nvrhi::BufferDesc InstanceIdBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(int32_t) * Proxies.size(), "InstanceIdsBuffer");
	InstanceIdBufferDesc.structStride = sizeof(int32_t);
	InstanceIdBufferDesc.canHaveUAVs = true;
	InstanceIdBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	InstanceIdBufferDesc.keepInitialState = true;
	InstanceIdBufferDesc.isVertexBuffer = true;
	InstanceIdBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceIdBufferDesc);	  //worst case size
	//bounding box buffer
	nvrhi::BufferDesc InstanceBoundingBoxBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(FBoundingBox) * Proxies.size(), "InstanceBoundingBoxBuffer");
	InstanceBoundingBoxBufferDesc.structStride = sizeof(FBoundingBox);
	InstanceBoundingBoxBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBoundingBoxBufferDesc);	  //worst case size
	
	//create the indirect draw args buffer
	nvrhi::BufferDesc IndirectDrawArgsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(nvrhi::DrawIndexedIndirectArguments) * AssetManager::GetInstance()->VertexBufferInfos.size(), "IndirectDrawArgsBuffer");
	IndirectDrawArgsBufferDesc.structStride = sizeof(nvrhi::DrawIndexedIndirectArguments);
	IndirectDrawArgsBufferDesc.canHaveUAVs = true;
	IndirectDrawArgsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	IndirectDrawArgsBufferDesc.keepInitialState = true;
	IndirectDrawArgsBufferDesc.isDrawIndirectArgs = true;
	IndirectDrawArgsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(IndirectDrawArgsBufferDesc);
}
