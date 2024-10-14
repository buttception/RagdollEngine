#include "ragdollpch.h"
#include "XeGTAOPass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

#define GET_BATCH_DIMENSION_DEPTH(x, y) (x + 16-1) / 16, (y + 16-1) / 16, 1
#define GET_BATCH_DIMENSION_AO(x, y) (x + XE_GTAO_NUMTHREADS_X-1) / XE_GTAO_NUMTHREADS_X, (y + XE_GTAO_NUMTHREADS_Y-1) / XE_GTAO_NUMTHREADS_Y, 1
#define GET_BATCH_DIMENSION_NOISE(x,y) (x + (XE_GTAO_NUMTHREADS_X*2)-1) / (XE_GTAO_NUMTHREADS_X*2), (y + XE_GTAO_NUMTHREADS_Y-1) / XE_GTAO_NUMTHREADS_Y, 1

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
	XeGTAO::GTAOUpdateConstants(CBuffer, width, height, Settings, &projMatrix._11, true, false);
}

void XeGTAOPass::GenerateAO(const ragdoll::SceneInformation& sceneInfo)
{
	CommandListRef->beginMarker("XeGTAO");
	UpdateConstants(DepthBuffer->getDesc().width, DepthBuffer->getDesc().height, sceneInfo.InfiniteReverseZProj);
	//cbuffer shared amongst all
	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(XeGTAO::GTAOConstants), "XeGTAO CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &CBuffer, sizeof(XeGTAO::GTAOConstants));

	CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Matrix), "ViewMatrix CBuffer", 1);
	nvrhi::BufferHandle MatrixHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(MatrixHandle, &sceneInfo.MainCameraView, sizeof(Matrix));

	GenerateDepthMips(sceneInfo, ConstantBufferHandle, MatrixHandle);
	MainPass(sceneInfo, ConstantBufferHandle, MatrixHandle);
	Denoise(sceneInfo, ConstantBufferHandle, MatrixHandle);
	Compose(sceneInfo, ConstantBufferHandle, MatrixHandle);

	CommandListRef->endMarker();
}

void XeGTAOPass::GenerateDepthMips(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix)
{
	MICROPROFILE_SCOPEI("Render", "Depth Mip", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Prepare Depth Mip");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
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
	CommandListRef->dispatch(GET_BATCH_DIMENSION_DEPTH(DepthMips->getDesc().width, DepthMips->getDesc().height));
	CommandListRef->endMarker();
}

void XeGTAOPass::MainPass(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix)
{
	MICROPROFILE_SCOPEI("Render", "Main Pass", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Main Pass");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
		nvrhi::BindingSetItem::Texture_SRV(0, DepthMips),
		nvrhi::BindingSetItem::Texture_SRV(1, NormalMap),
		nvrhi::BindingSetItem::Texture_UAV(0, AOTerm),
		nvrhi::BindingSetItem::Texture_UAV(1, EdgeMap),
		nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CSGTAOHigh.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(GET_BATCH_DIMENSION_AO(DepthMips->getDesc().width, DepthMips->getDesc().height));
	CommandListRef->endMarker();
}

void XeGTAOPass::Denoise(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix)
{
	MICROPROFILE_SCOPEI("Render", "Denoise", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Denoise");

	const int passCount = std::max(1, Settings.DenoisePasses); // even without denoising we have to run a single last pass to output correct term into the external output texture
	nvrhi::TextureHandle AOPing{ AOTerm }, AOPong{ FinalAOTerm };
	for (int i = 0; i < passCount; i++)
	{
		const bool lastPass = i == passCount - 1;

		std::string shaderName = (lastPass) ? ("CSDenoiseLastPass.cs.cso") : ("CSDenoisePass.cs.cso");

		nvrhi::BindingSetDesc setDesc;
		setDesc.bindings = {
			nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
		nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
			nvrhi::BindingSetItem::Texture_SRV(0, AOPing),
			nvrhi::BindingSetItem::Texture_SRV(1, EdgeMap),
			nvrhi::BindingSetItem::Texture_UAV(0, AOPong),
			nvrhi::BindingSetItem::Texture_UAV(1, ORM),
			nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
		};
		nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
		nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

		std::swap(AOPing, AOPong);      // ping becomes pong, pong becomes ping.

		nvrhi::ComputePipelineDesc PipelineDesc;
		PipelineDesc.bindingLayouts = { layoutHandle };
		nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader(shaderName);
		PipelineDesc.CS = shader;

		nvrhi::ComputeState state;
		state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
		state.bindings = { setHandle };
		CommandListRef->setComputeState(state);
		CommandListRef->dispatch(GET_BATCH_DIMENSION_NOISE(DepthMips->getDesc().width, DepthMips->getDesc().height));
	}
	CommandListRef->endMarker();
}

void XeGTAOPass::Compose(const ragdoll::SceneInformation& sceneInfo, nvrhi::BufferHandle BufferHandle, nvrhi::BufferHandle matrix)
{
	MICROPROFILE_SCOPEI("Render", "Compose", MP_BLUEVIOLET);
	CommandListRef->beginMarker("Compose");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BufferHandle),
	nvrhi::BindingSetItem::ConstantBuffer(1, matrix),
		nvrhi::BindingSetItem::Texture_SRV(0, FinalAOTerm),
		nvrhi::BindingSetItem::Texture_UAV(1, ORM),
		nvrhi::BindingSetItem::Sampler(10, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Point_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("CSComposeAO.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(GET_BATCH_DIMENSION_NOISE(DepthMips->getDesc().width, DepthMips->getDesc().height));

	CommandListRef->endMarker();
}
