#include "ragdollpch.h"
#include "XeGTAOPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

#define GET_BATCH_DIMENSION(x, y) (x + 16-1) / 16, (y + 16-1) / 16, 1

void XeGTAOPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void XeGTAOPass::SetDependencies(Textures dependencies)
{
	DepthBuffer = dependencies.DepthBuffer;
	NormalMap = dependencies.NormalMap;
	ORM = dependencies.ORM;
	DepthMips = dependencies.DepthMips;
	AOTerm = dependencies.AOTerm;
	EdgeMap = dependencies.EdgeMap;
	FinalAOTerm = dependencies.FinalAOTerm;
}

void XeGTAOPass::UpdateConstants(const uint32_t width, const uint32_t height, const Matrix& projMatrix)
{
	XeGTAO::GTAOUpdateConstants(CBuffer, width, height, Settings, &projMatrix._11, false, false);
}

void XeGTAOPass::GenerateAO(const ragdoll::SceneInformation& sceneInfo)
{

	CommandListRef->beginMarker("XeGTAO");
	UpdateConstants(DepthBuffer->getDesc().width, DepthBuffer->getDesc().height, sceneInfo.InfiniteReverseZProj);
	//cbuffer shared amongst all
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(XeGTAO::GTAOConstants), "XeGTAO CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(XeGTAO::GTAOConstants));

	GenerateDepthMips(sceneInfo, ConstantBufferHandle);

	CommandListRef->endMarker();
}

void XeGTAOPass::GenerateDepthMips(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle)
{

	MICROPROFILE_SCOPEI("Render", "Depth Mip", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Depth Mip");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, DepthBuffer),
		nvrhi::BindingSetItem::Texture_UAV(0, DepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{0,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(1, DepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{1,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(2, DepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{2,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(3, DepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{3,1,0,1}),
		nvrhi::BindingSetItem::Texture_UAV(4, DepthMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{4,1,0,1}),
		nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CSPrefilterDepths16x16.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(GET_BATCH_DIMENSION(DepthMips->getDesc().width, DepthMips->getDesc().height));
	CommandListRef->endMarker();
}
