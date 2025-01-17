#include "ragdollpch.h"
#include "SkyGeneratePass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

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

void SkyGeneratePass::GenerateSky(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets)
{
	RD_SCOPE(Render, GenerateSky);
	RD_GPU_SCOPE("GenerateSky", CommandListRef);
	CommandListRef->beginMarker("Generate Sky");
	//update the sun sky and tables according to scene info
	Vector3 lightDir = Vector3(sceneInfo.LightDirection.x, sceneInfo.LightDirection.z, sceneInfo.LightDirection.y);
	SunSky->Update(lightDir, 2.5f, 0.f, 0.f);
	Table->FindThetaGammaTables(*SunSky);
	//write to the table textures
	CommandListRef->writeTexture(targets->SkyThetaGammaTable, 0, 0, Table->Data, Table->kTableSize * sizeof(uint32_t));
	//dispatch cs to make the sky texture
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(ConstantBuffer), "Sky CBuffer", 1);
	nvrhi::BufferHandle cBufHandle = DirectXDevice::GetNativeDevice()->createBuffer(cBufDesc);

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, cBufHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->SkyThetaGammaTable),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->SkyTexture)
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("SkyGenerate.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CBuffer.width = targets->SkyTexture->getDesc().width;
	CBuffer.height = targets->SkyTexture->getDesc().height;
	CBuffer.sun = lightDir;
	CBuffer.PerezInvDen = SunSky->mPerezInvDen;
	CBuffer.maxTheta = Table->mMaxTheta;
	CBuffer.maxGamma = Table->mMaxGamma;
	if (sceneInfo.SkyDimmer == 0.f)
		CBuffer.scalar = 0.499999987e-04f;
	else
		CBuffer.scalar = sceneInfo.SkyDimmer * 1e-04f;
	CommandListRef->writeBuffer(cBufHandle, &CBuffer, sizeof(ConstantBuffer));
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(targets->SkyTexture->getDesc().width / 16 + 1, targets->SkyTexture->getDesc().height / 16 + 1, 1);
	CommandListRef->endMarker();
}
