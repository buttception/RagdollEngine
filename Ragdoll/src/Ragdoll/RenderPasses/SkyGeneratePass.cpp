#include "ragdollpch.h"
#include "SkyGeneratePass.h"

#include <nvrhi/utils.h>

#include "Ragdoll/DirectXDevice.h"
#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/SunSky.h"

void SkyGeneratePass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;

	Table = std::make_shared<FSunSkyTable>();
	SunSky = std::make_shared<FSunSkyPreetham>();
}

void SkyGeneratePass::SetDependencies(nvrhi::TextureHandle sky, nvrhi::TextureHandle table)
{
	SkyTexture = sky;
	ThetaGammaTable = table;
}

void SkyGeneratePass::GenerateSky(const ragdoll::SceneInformation& sceneInfo)
{
	//update the sun sky and tables according to scene info
	Vector3 lightDir = -sceneInfo.LightDirection;
	SunSky->Update(lightDir, 2.5f, 0.f, 0.f);
	Table->FindThetaGammaTables(*SunSky);
	//write to the table textures
	uint32_t size;
	CommandListRef->writeTexture(ThetaGammaTable, 0, 0, Table->Data, Table->kTableSize * sizeof(uint32_t));
	//dispatch cs to make the sky texture
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "Sky CBuffer", 1);
	nvrhi::BufferHandle cBufHandle = DirectXDevice::GetNativeDevice()->createBuffer(cBufDesc);

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, cBufHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, ThetaGammaTable),
		nvrhi::BindingSetItem::Texture_UAV(0, SkyTexture)
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetNativeDevice()->createBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("SkyGenerate.cs.cso");
	PipelineDesc.CS = shader;

	CommandListRef->beginMarker("Generate Sky");
	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CBuffer.width = SkyTexture->getDesc().width;
	CBuffer.height = SkyTexture->getDesc().height;
	CBuffer.sun = lightDir;
	CBuffer.PerezInvDen = SunSky->mPerezInvDen;
	CommandListRef->writeBuffer(cBufHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(SkyTexture->getDesc().width / 16 + 1, SkyTexture->getDesc().height / 16 + 1, 1);
	CommandListRef->endMarker();
}
