#include "ragdollpch.h"
#include "GPUScene.h"

#include <nvrhi/utils.h>

#include "DirectXDevice.h"
#include "AssetManager.h"

void ragdoll::FGPUScene::Update(Scene* Scene)
{
	if (InstanceBuffer == nullptr)
		CreateBuffers(Scene->StaticProxies);
}

void ragdoll::FGPUScene::UpdateInstanceBuffer(std::vector<Proxy>& Proxies)
{
	std::sort(Proxies.begin(), Proxies.end(), [](const Proxy& a, const Proxy& b) { return a.BufferIndex < b.BufferIndex; });
	//get a copy of all proxies as instances then sort them
	std::vector<InstanceData> Instances;
	Instances.resize(Proxies.size());
	for (int i = 0; i < Proxies.size(); ++i)
	{
		Instances[i] = Proxies[i];
	}
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
	//get command list from render in the future for multi threading
	nvrhi::CommandListHandle CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	//update the instance buffer
	CommandList->open();
	CommandList->beginMarker("Writing Instance Data");
	CommandList->beginTrackingBufferState(InstanceBuffer, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(InstanceBuffer, Instances.data(), sizeof(InstanceData) * Instances.size());
	CommandList->setPermanentBufferState(InstanceBuffer, nvrhi::ResourceStates::ShaderResource);
	CommandList->endMarker();
	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
}

void ragdoll::FGPUScene::InstanceCull(nvrhi::CommandListHandle CommandList, const Matrix& ViewProjection, const Vector3& CameraPosition)
{
	CommandList->beginMarker("Instance Culling");
	//reset the indirect draw args first
	ResetBuffers(CommandList);
	//frustum cull the scene
	FrustumCullScene(CommandList, ViewProjection, CameraPosition);
	CommandList->endMarker();
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

void ragdoll::FGPUScene::FrustumCullScene(nvrhi::CommandListHandle Commandlist, const Matrix& ViewProjection, const Vector3& CameraPosition)
{
	struct ConstantBuffer {
		Vector4 FrustumPlanes[6];
		Vector3 CameraPosition;
	} ConstantBuffer;
	//extract the frustum planes
	ExtractFrustumPlanes(ConstantBuffer.FrustumPlanes, ViewProjection);
	ConstantBuffer.CameraPosition = CameraPosition;
	//dispatch compute to frustum cull the scene
}

void ragdoll::FGPUScene::CreateBuffers(const std::vector<Proxy>& Proxies)
{
	//get command list from render in the future for multi threading
	nvrhi::CommandListHandle CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	//create the buffers
	nvrhi::BufferDesc InstanceBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(InstanceData) * Proxies.size(), "InstanceBuffer");
	InstanceBufferDesc.structStride = sizeof(InstanceData);
	InstanceBuffer = DirectXDevice::GetNativeDevice()->createBuffer(InstanceBufferDesc);	   //worst case size
	nvrhi::BufferDesc InstanceIdBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(int32_t) * Proxies.size(), "InstanceIdsBuffer");
	InstanceIdBufferDesc.structStride = sizeof(int32_t);
	InstanceIdBufferDesc.canHaveUAVs = true;
	InstanceIdBuffer = DirectXDevice::GetNativeDevice()->createBuffer(nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(int32_t) * Proxies.size(), "InstanceIdsBuffer"));	  //worst case size
	//update the instance buffer
	//TODO: write all the instances from the proxies to the buffer
	
	//update the indirect draw args buffer
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
		Args[i].startIndexLocation = 0;
	}
	nvrhi::BufferDesc IndirectDrawArgsBufferDesc = nvrhi::utils::CreateStaticConstantBufferDesc(sizeof(nvrhi::DrawIndexedIndirectArguments) * Args.size(), "IndirectDrawArgsBuffer");
	IndirectDrawArgsBufferDesc.structStride = sizeof(nvrhi::DrawIndexedIndirectArguments);
	IndirectDrawArgsBufferDesc.canHaveUAVs = true;
	IndirectDrawArgsBufferDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	IndirectDrawArgsBufferDesc.keepInitialState = true;
	IndirectDrawArgsBuffer = DirectXDevice::GetNativeDevice()->createBuffer(IndirectDrawArgsBufferDesc);

	CommandList->open();
	CommandList->beginMarker("Writing Indirect Draw Args");
	CommandList->writeBuffer(IndirectDrawArgsBuffer, Args.data(), sizeof(nvrhi::DrawIndexedIndirectArguments) * Args.size());
	CommandList->endMarker();
	CommandList->close();
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
}

void ragdoll::FGPUScene::ResetBuffers(nvrhi::CommandListHandle CommandList)
{
	struct ConstantBuffer
	{
		uint32_t MeshCount;
	} ConstantBuffer;
	//create a constant buffer here
	nvrhi::BufferDesc ConstBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(struct ConstantBuffer), "Reset Buffers", 1);
	nvrhi::BufferHandle ConstBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(ConstBufferDesc);

	//binding layout and sets
	nvrhi::BindingSetDesc ResetSetDesc;
	ResetSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstBufferHandle),
		nvrhi::BindingSetItem::StructuredBuffer_UAV(0, IndirectDrawArgsBuffer)
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
	ConstantBuffer.MeshCount = AssetManager::GetInstance()->VertexBufferInfos.size();
	CommandList->writeBuffer(ConstBufferHandle, &ConstantBuffer, sizeof(struct ConstantBuffer));
	CommandList->setComputeState(state);
	CommandList->dispatch(AssetManager::GetInstance()->VertexBufferInfos.size() / 64 + 1, 1, 1);
	CommandList->endMarker();
}
