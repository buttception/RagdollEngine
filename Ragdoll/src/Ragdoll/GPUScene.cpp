#include "ragdollpch.h"
#include "GPUScene.h"

#include <nvrhi/utils.h>

#include "DirectXDevice.h"
#include "AssetManager.h"

#define INSTANCE_DATA_BUFFER_SRV_SLOT 0
#define INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT 1
#define INSTANCE_OFFSET_BUFFER_SRV_SLOT 2

#define INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT 0
#define INSTANCE_ID_BUFFER_UAV_SLOT 1

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
	//get a instance data buffer copy
	std::sort(Proxies.begin(), Proxies.end(), [](const Proxy& a, const Proxy& b) { return a.BufferIndex < b.BufferIndex; });
	//get a copy of all proxies as instances then sort them
	std::vector<InstanceData> Instances;
	Instances.resize(Proxies.size());
	for (int i = 0; i < Proxies.size(); ++i)
	{
		Instances[i] = Proxies[i];
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
	//every mesh gets 1 indirect draw args
	std::vector<nvrhi::DrawIndexedIndirectArguments> Args;
	Args.resize(AssetManager::GetInstance()->VertexBufferInfos.size());
	for (int i = 0; i < AssetManager::GetInstance()->VertexBufferInfos.size(); ++i)
	{
		const VertexBufferInfo& info = AssetManager::GetInstance()->VertexBufferInfos[i];
		Args[i].indexCount = info.IndicesCount;
		Args[i].instanceCount = 0;
		Args[i].startIndexLocation = info.IBOffset;
		Args[i].baseVertexLocation = info.VBOffset;
		Args[i].startInstanceLocation = 0;
	}
	//get all the bounding boxes and upload onto gpu
	std::vector<DirectX::BoundingBox> BoundingBoxes;
	BoundingBoxes.resize(Proxies.size());
	for (int i = 0; i < Proxies.size(); ++i)
	{
		BoundingBoxes[i] = Proxies[i].BoundingBox;
	}

	//get command list from render in the future for multi threading
	nvrhi::CommandListHandle CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	CommandList->open();
	CommandList->beginMarker("Writing Indirect Draw Args");
	CommandList->writeBuffer(IndirectDrawArgsBuffer, Args.data(), sizeof(nvrhi::DrawIndexedIndirectArguments) * Args.size());
	CommandList->beginTrackingBufferState(InstanceOffsetBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceOffsetBuffer, Offsets.data(), sizeof(uint32_t) * Offsets.size());
	CommandList->setPermanentBufferState(InstanceOffsetBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();
	CommandList->beginMarker("Writing Instance Data");
	CommandList->beginTrackingBufferState(InstanceBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceBuffer, Instances.data(), sizeof(InstanceData) * Instances.size());
	CommandList->setPermanentBufferState(InstanceBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();
	CommandList->beginMarker("Writing Bounding Box Data");
	CommandList->beginTrackingBufferState(InstanceBoundingBoxBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceBoundingBoxBuffer, BoundingBoxes.data(), sizeof(DirectX::BoundingBox) * BoundingBoxes.size());
	CommandList->setPermanentBufferState(InstanceBoundingBoxBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();
	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
}

void ExtractFrustumPlanes(Vector4 OutPlanes[6], const Matrix& ViewProjectionMatrix)
{
	// Left plane
	OutPlanes[0] = Vector4(
		ViewProjectionMatrix.m[0][3] + ViewProjectionMatrix.m[0][0],
		ViewProjectionMatrix.m[1][3] + ViewProjectionMatrix.m[1][0],
		ViewProjectionMatrix.m[2][3] + ViewProjectionMatrix.m[2][0],
		ViewProjectionMatrix.m[3][3] + ViewProjectionMatrix.m[3][0]
	);

	// Right plane
	OutPlanes[1] = Vector4(
		ViewProjectionMatrix.m[0][3] - ViewProjectionMatrix.m[0][0],
		ViewProjectionMatrix.m[1][3] - ViewProjectionMatrix.m[1][0],
		ViewProjectionMatrix.m[2][3] - ViewProjectionMatrix.m[2][0],
		ViewProjectionMatrix.m[3][3] - ViewProjectionMatrix.m[3][0]
	);

	// Bottom plane
	OutPlanes[2] = Vector4(
		ViewProjectionMatrix.m[0][3] + ViewProjectionMatrix.m[0][1],
		ViewProjectionMatrix.m[1][3] + ViewProjectionMatrix.m[1][1],
		ViewProjectionMatrix.m[2][3] + ViewProjectionMatrix.m[2][1],
		ViewProjectionMatrix.m[3][3] + ViewProjectionMatrix.m[3][1]
	);

	// Top plane
	OutPlanes[3] = Vector4(
		ViewProjectionMatrix.m[0][3] - ViewProjectionMatrix.m[0][1],
		ViewProjectionMatrix.m[1][3] - ViewProjectionMatrix.m[1][1],
		ViewProjectionMatrix.m[2][3] - ViewProjectionMatrix.m[2][1],
		ViewProjectionMatrix.m[3][3] - ViewProjectionMatrix.m[3][1]
	);

	// Near plane
	OutPlanes[4] = Vector4(
		ViewProjectionMatrix.m[0][3] + ViewProjectionMatrix.m[0][2],
		ViewProjectionMatrix.m[1][3] + ViewProjectionMatrix.m[1][2],
		ViewProjectionMatrix.m[2][3] + ViewProjectionMatrix.m[2][2],
		ViewProjectionMatrix.m[3][3] + ViewProjectionMatrix.m[3][2]
	);

	// Far plane
	OutPlanes[5] = Vector4(
		ViewProjectionMatrix.m[0][3] - ViewProjectionMatrix.m[0][2],
		ViewProjectionMatrix.m[1][3] - ViewProjectionMatrix.m[1][2],
		ViewProjectionMatrix.m[2][3] - ViewProjectionMatrix.m[2][2],
		ViewProjectionMatrix.m[3][3] - ViewProjectionMatrix.m[3][2]
	);

	// Normalize the planes
	for (int i = 0; i < 6; ++i)
	{
		float Length = Vector3(OutPlanes[i].x, OutPlanes[i].y, OutPlanes[i].z).Length();
		OutPlanes[i] /= Length;
	}
}

void ragdoll::FGPUScene::InstanceCull(nvrhi::CommandListHandle CommandList, const Matrix& ViewProjection, const Matrix& View, uint32_t ProxyCount)
{
	struct ConstantBuffer
	{
		Matrix ViewMatrix;
		Vector4 FrustumPlanes[6];
		uint32_t ProxyCount;
		uint32_t MeshCount;
	} ConstantBuffer;
	CommandList->beginMarker("Instance Culling");
	ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, ViewProjection);
	ConstantBuffer.ViewMatrix = View;//create a constant buffer here
	ConstantBuffer.ProxyCount = ProxyCount;
	ConstantBuffer.MeshCount = AssetManager::GetInstance()->VertexBufferInfos.size();
	nvrhi::BufferDesc ConstBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(struct ConstantBuffer), "Instance Culling Buffer", 1);
	VolatileConstantBuffer = DirectXDevice::GetNativeDevice()->createBuffer(ConstBufferDesc);
	CommandList->writeBuffer(VolatileConstantBuffer, &ConstantBuffer, sizeof(struct ConstantBuffer));
	//reset the indirect draw args first
	ResetBuffers(CommandList);
	//frustum cull the scene
	FrustumCullScene(CommandList, ProxyCount);
	//pack the instance ids
	PackInstanceIds(CommandList);
	CommandList->endMarker();
}

void ragdoll::FGPUScene::FrustumCullScene(nvrhi::CommandListHandle CommandList, uint32_t ProxyCount)
{
	//binding layout and sets
	nvrhi::BindingSetDesc CullingSetDesc;
	CullingSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, VolatileConstantBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_DATA_BUFFER_SRV_SLOT, InstanceBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_BOUNDING_BOX_BUFFER_SRV_SLOT, InstanceBoundingBoxBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_ID_BUFFER_UAV_SLOT, InstanceIdBuffer),
	};
	nvrhi::BindingLayoutHandle CullingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(CullingSetDesc);
	nvrhi::BindingSetHandle CullingBindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(CullingSetDesc, CullingLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc CullingPipelineDesc;
	CullingPipelineDesc.bindingLayouts = { CullingLayoutHandle };
	nvrhi::ShaderHandle CullingShader = AssetManager::GetInstance()->GetShader("InstanceCull.cs.cso");
	CullingPipelineDesc.CS = CullingShader;

	CommandList->beginMarker("Frustum Cull Scene");
	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(CullingPipelineDesc);
	state.bindings = { CullingBindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(ProxyCount / 64 + 1, 1, 1);
	CommandList->endMarker();
}

void ragdoll::FGPUScene::PackInstanceIds(nvrhi::CommandListHandle CommandList)
{
	//binding layout and sets
	nvrhi::BindingSetDesc PackIdsSetDesc;
	PackIdsSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, VolatileConstantBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_SRV(INSTANCE_OFFSET_BUFFER_SRV_SLOT, InstanceOffsetBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT, IndirectDrawArgsBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INSTANCE_ID_BUFFER_UAV_SLOT, InstanceIdBuffer),
	};
	nvrhi::BindingLayoutHandle PackIdsLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(PackIdsSetDesc);
	nvrhi::BindingSetHandle PackIdsBindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(PackIdsSetDesc, PackIdsLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc PackIdsPipelineDesc;
	PackIdsPipelineDesc.bindingLayouts = { PackIdsLayoutHandle };
	nvrhi::ShaderHandle PackingShader = AssetManager::GetInstance()->GetShader("PackInstanceIds.cs.cso");
	PackIdsPipelineDesc.CS = PackingShader;

	CommandList->beginMarker("Pack Ids");
	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PackIdsPipelineDesc);
	state.bindings = { PackIdsBindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(AssetManager::GetInstance()->VertexBufferInfos.size() / 64 + 1, 1, 1);
	CommandList->endMarker();
}

void ragdoll::FGPUScene::CreateBuffers(const std::vector<Proxy>& Proxies)
{
	//create the buffers
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
	InstanceIdBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceIdBufferDesc);	  //worst case size
	//bounding box buffer
	nvrhi::BufferDesc InstanceBoundingBoxBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(DirectX::BoundingBox) * Proxies.size(), "InstanceBoundingBoxBuffer");
	InstanceBoundingBoxBufferDesc.structStride = sizeof(DirectX::BoundingBox);
	InstanceBoundingBoxBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBoundingBoxBufferDesc);	  //worst case size
	
	//create the indirect draw args buffer
	nvrhi::BufferDesc IndirectDrawArgsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(nvrhi::DrawIndexedIndirectArguments) * AssetManager::GetInstance()->VertexBufferInfos.size(), "IndirectDrawArgsBuffer");
	IndirectDrawArgsBufferDesc.structStride = sizeof(nvrhi::DrawIndexedIndirectArguments);
	IndirectDrawArgsBufferDesc.canHaveUAVs = true;
	IndirectDrawArgsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	IndirectDrawArgsBufferDesc.keepInitialState = true;
	IndirectDrawArgsBufferDesc.isDrawIndirectArgs = true;
	IndirectDrawArgsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(IndirectDrawArgsBufferDesc);

	//create the instance offset buffer
	nvrhi::BufferDesc InstanceOffsetBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(uint32_t) * AssetManager::GetInstance()->VertexBufferInfos.size(), "InstanceOffsetBuffer");
	InstanceOffsetBufferDesc.structStride = sizeof(uint32_t);
	InstanceOffsetBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceOffsetBufferDesc);
}

void ragdoll::FGPUScene::ResetBuffers(nvrhi::CommandListHandle CommandList)
{
	//binding layout and sets
	nvrhi::BindingSetDesc ResetSetDesc;
	ResetSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, VolatileConstantBuffer),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(INDIRECT_DRAW_ARGS_BUFFER_UAV_SLOT, IndirectDrawArgsBuffer)
	};
	nvrhi::BindingLayoutHandle ResetLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(ResetSetDesc);
	nvrhi::BindingSetHandle ResetBindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(ResetSetDesc, ResetLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc ResetPipelineDesc;
	ResetPipelineDesc.bindingLayouts = { ResetLayoutHandle };
	nvrhi::ShaderHandle ResetShader = AssetManager::GetInstance()->GetShader("ResetIndirect.cs.cso");
	ResetPipelineDesc.CS = ResetShader;

	CommandList->beginMarker("Reset Indirect Draw Args");
	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(ResetPipelineDesc);
	state.bindings = { ResetBindingSetHandle };
	CommandList->setComputeState(state);
	CommandList->dispatch(AssetManager::GetInstance()->VertexBufferInfos.size() / 64 + 1, 1, 1);
	CommandList->endMarker();
}
