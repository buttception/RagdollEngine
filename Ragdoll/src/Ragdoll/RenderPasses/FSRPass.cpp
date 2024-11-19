#include "ragdollpch.h"
#include "FSRPass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"
#include "Ragdoll/DirectXDevice.h"

#include "Ragdoll/Scene.h"
#include "Ragdoll/AssetManager.h"

uint32_t ffxF32ToF16(float f)
{
	static uint16_t base[512] = {
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400,
		0x0800, 0x0c00, 0x1000, 0x1400, 0x1800, 0x1c00, 0x2000, 0x2400, 0x2800, 0x2c00, 0x3000, 0x3400, 0x3800, 0x3c00, 0x4000, 0x4400, 0x4800, 0x4c00, 0x5000,
		0x5400, 0x5800, 0x5c00, 0x6000, 0x6400, 0x6800, 0x6c00, 0x7000, 0x7400, 0x7800, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff,
		0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff,
		0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff,
		0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff,
		0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff,
		0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff,
		0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x7bff, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8001, 0x8002,
		0x8004, 0x8008, 0x8010, 0x8020, 0x8040, 0x8080, 0x8100, 0x8200, 0x8400, 0x8800, 0x8c00, 0x9000, 0x9400, 0x9800, 0x9c00, 0xa000, 0xa400, 0xa800, 0xac00,
		0xb000, 0xb400, 0xb800, 0xbc00, 0xc000, 0xc400, 0xc800, 0xcc00, 0xd000, 0xd400, 0xd800, 0xdc00, 0xe000, 0xe400, 0xe800, 0xec00, 0xf000, 0xf400, 0xf800,
		0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff,
		0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff,
		0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff,
		0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff,
		0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff,
		0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff, 0xfbff
	};

	static uint8_t shift[512] = {
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f, 0x0e, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
		0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f, 0x0e, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
		0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18
	};

	union
	{
		float		f;
		uint32_t	u;
	} bits;

	bits.f = f;
	uint32_t u = bits.u;
	uint32_t i = u >> 23;
	return (uint32_t)(base[i]) + ((u & 0x7fffff) >> shift[i]);
}

/// Pack 2x32-bit floating point values in a single 32bit value.
///
/// This function first converts each component of <c><i>value</i></c> into their nearest 16-bit floating
/// point representation, and then stores the X and Y components in the lower and upper 16 bits of the
/// 32bit unsigned integer respectively.
///
/// @param [in] x               A 2-dimensional floating point value to convert and pack.
///
/// @returns
/// A packed 32bit value containing 2 16bit floating point values.
///
/// @ingroup CPUCore
uint32_t ffxPackHalf2x16(Vector2 x)
{
	return ffxF32ToF16(x.x) + (ffxF32ToF16(x.y) << 16);
}

uint32_t ffxAsUInt32(float x)
{
	union
	{
		float		f;
		uint32_t	u;
	} bits;

	bits.f = x;
	return bits.u;
}

void FSRPass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
}

void FSRPass::Upscale(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, float _dt)
{
	RD_SCOPE(Render, FSR);
	RD_GPU_SCOPE("FSR", CommandListRef);
	CommandListRef->beginMarker("FSR");
	const int32_t threadGroupWorkRegionDim = 8;
	DispatchSrcX = (sceneInfo.RenderWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	DispatchSrcY = (sceneInfo.RenderHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	DispatchDstX = (sceneInfo.TargetWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	DispatchDstY = (sceneInfo.TargetHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

	UpdateConstants(sceneInfo, _dt);

	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Fsr3UpscalerConstants), "FSR CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &Constants, sizeof(Fsr3UpscalerConstants));

	CommandListRef->clearTextureUInt(targets->RecontDepth, nvrhi::AllSubresources, 0);
	CommandListRef->clearTextureUInt(targets->SpdAtomic, nvrhi::AllSubresources, 0);
	CommandListRef->clearTextureFloat(targets->LumaInstability, nvrhi::AllSubresources, 0);
	CommandListRef->clearTextureFloat(targets->ShadingChange, nvrhi::AllSubresources, 0);
	CommandListRef->clearTextureFloat (targets->FarthestDepthMip, nvrhi::AllSubresources, 0);
	CommandListRef->clearTextureFloat(targets->DilatedReactiveMask, nvrhi::AllSubresources, 0);
	CommandListRef->clearTextureFloat(targets->SpdMips, nvrhi::AllSubresources, 0);
	PrepareInputs(sceneInfo, targets, ConstantBufferHandle);
	ComputeLuminancePyramid(sceneInfo, targets, ConstantBufferHandle);
	ComputeShadingChangePyramid(sceneInfo, targets, ConstantBufferHandle);
	ComputeShadingChange(sceneInfo, targets, ConstantBufferHandle);
	ComputeReactivity(sceneInfo, targets, ConstantBufferHandle);
	ComputeLumaInstability(sceneInfo, targets, ConstantBufferHandle);
	Accumulate(sceneInfo, targets, ConstantBufferHandle);
	//RCAS(sceneInfo, targets, ConstantBufferHandle);

	CommandListRef->endMarker();
}

void FSRPass::PrepareInputs(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Prepare Inputs");
	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->CurrDepthBuffer),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->FinalColor),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->VelocityBuffer),
		nvrhi::BindingSetItem::Texture_UAV(3, targets->FarthestDepth),
		nvrhi::BindingSetItem::Texture_UAV(4, targets->CurrLuminance),
		nvrhi::BindingSetItem::Texture_UAV(2, targets->RecontDepth),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->DilatedDepth),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->DilatedMotionVectors),
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrPrepareInputs.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchSrcX, DispatchSrcY, 1);
	CommandListRef->endMarker();
}

void FSRPass::ComputeLuminancePyramid(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Compute Luminance Pyramid");

	uint32_t workGroupOffset[2];
	uint32_t numWorkGroupsAndMips[2];
	uint32_t rectInfo[4] = { 0, 0, sceneInfo.RenderWidth, sceneInfo.RenderHeight };

	// determines the offset of the first tile to downsample based on
	// left (rectInfo[0]) and top (rectInfo[1]) of the subregion.
	workGroupOffset[0] = rectInfo[0] / 64;
	workGroupOffset[1] = rectInfo[1] / 64;

	uint32_t endIndexX = (rectInfo[0] + rectInfo[2] - 1) / 64;  // rectInfo[0] = left, rectInfo[2] = width
	uint32_t endIndexY = (rectInfo[1] + rectInfo[3] - 1) / 64;  // rectInfo[1] = top, rectInfo[3] = height

	// we only need to dispatch as many thread groups as tiles we need to downsample
	// number of tiles per slice depends on the subregion to downsample
	DispatchThreadGroupCountXY[0] = endIndexX + 1 - workGroupOffset[0];
	DispatchThreadGroupCountXY[1] = endIndexY + 1 - workGroupOffset[1];

	// number of thread groups per slice
	numWorkGroupsAndMips[0] = (DispatchThreadGroupCountXY[0]) * (DispatchThreadGroupCountXY[1]);

	// calculate based on rect width and height
	uint32_t resolution = std::max(rectInfo[2], rectInfo[3]);
	numWorkGroupsAndMips[1] = uint32_t((std::min(floor(log2(float(resolution))), float(12))));

	SpdConstants.numworkGroups = numWorkGroupsAndMips[0];
	SpdConstants.mips = numWorkGroupsAndMips[1];
	SpdConstants.workGroupOffset[0] = workGroupOffset[0];
	SpdConstants.workGroupOffset[1] = workGroupOffset[1];
	SpdConstants.renderSize[0] = sceneInfo.RenderWidth;
	SpdConstants.renderSize[1] = sceneInfo.RenderHeight;

	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Fsr3UpscalerSpdConstants), "Spd CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &SpdConstants, sizeof(Fsr3UpscalerSpdConstants));

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->FarthestDepth),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->Luminance0),
		nvrhi::BindingSetItem::Texture_UAV(8, targets->FarthestDepthMip),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->FrameInfo),
		nvrhi::BindingSetItem::Texture_UAV(7, targets->SpdMips),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->SpdAtomic),
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrLumaPyramid.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchThreadGroupCountXY[0], DispatchThreadGroupCountXY[1], 1);
	CommandListRef->endMarker();
}

void FSRPass::ComputeShadingChangePyramid(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Shading Change Pyramid");

	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Fsr3UpscalerSpdConstants), "Spd CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &SpdConstants, sizeof(Fsr3UpscalerSpdConstants));

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->CurrLuminance),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->PrevLuminance),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->DilatedMotionVectors),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->FrameInfo),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->SpdMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{0, 1,0, 1}),
		nvrhi::BindingSetItem::Texture_UAV(2, targets->SpdMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{1, 1,0, 1}),
		nvrhi::BindingSetItem::Texture_UAV(3, targets->SpdMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{2, 1,0, 1}),
		nvrhi::BindingSetItem::Texture_UAV(4, targets->SpdMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{3, 1,0, 1}),
		nvrhi::BindingSetItem::Texture_UAV(5, targets->SpdMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{4, 1,0, 1}),
		nvrhi::BindingSetItem::Texture_UAV(6, targets->SpdMips, nvrhi::Format::UNKNOWN, nvrhi::TextureSubresourceSet{5, 1,0, 1}),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->SpdAtomic),
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrShadeChangePyramid.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchThreadGroupCountXY[0], DispatchThreadGroupCountXY[1], 1);
	CommandListRef->endMarker();
}

void FSRPass::ComputeShadingChange(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Shading Change");

	const int32_t dispatchShadingChangePassX = (int32_t(sceneInfo.RenderWidth * 0.5f) + (8 - 1)) / 8;
	const int32_t dispatchShadingChangePassY = (int32_t(sceneInfo.RenderHeight * 0.5f) + (8 - 1)) / 8;

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->SpdMips),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->ShadingChange),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrShadeChange.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(dispatchShadingChangePassX, dispatchShadingChangePassY, 1);
	CommandListRef->endMarker();
}

void FSRPass::ComputeReactivity(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Reactivity");

	const int32_t dispatchShadingChangePassX = (int32_t(sceneInfo.RenderWidth * 0.5f) + (8 - 1)) / 8;
	const int32_t dispatchShadingChangePassY = (int32_t(sceneInfo.RenderHeight * 0.5f) + (8 - 1)) / 8;

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->InputReactiveMask),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->InputTCMask),
		nvrhi::BindingSetItem::Texture_SRV(5, targets->PrevAccumulation),
		nvrhi::BindingSetItem::Texture_SRV(6, targets->ShadingChange),
		nvrhi::BindingSetItem::Texture_SRV(7, targets->CurrLuminance),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->RecontDepth),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->DilatedMotionVectors),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->DilatedDepth),
		nvrhi::BindingSetItem::Texture_SRV(8, targets->FrameInfo),
		nvrhi::BindingSetItem::Texture_UAV(2, targets->CurrAccumulation),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->NewLocks),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->DilatedReactiveMask),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrPrepareReactivity.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchSrcX, DispatchSrcY, 1);
	CommandListRef->endMarker();
}

void FSRPass::ComputeLumaInstability(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Luma Instability");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::Texture_SRV(4, targets->PrevLuminanceHistory),
		nvrhi::BindingSetItem::Texture_SRV(6, targets->CurrLuminance),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->DilatedMotionVectors),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->FrameInfo),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->DilatedReactiveMask),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->CurrLuminanceHistory),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->LumaInstability),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrLumaInstability.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchSrcX, DispatchSrcY, 1);
	CommandListRef->endMarker();
}

void FSRPass::Accumulate(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("Accumulate");

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, ConstantBuffer),
		nvrhi::BindingSetItem::Texture_SRV(8, targets->FinalColor),
		nvrhi::BindingSetItem::Texture_SRV(3, targets->PrevUpscaledBuffer),
		nvrhi::BindingSetItem::Texture_SRV(5, targets->FarthestDepthMip),
		nvrhi::BindingSetItem::Texture_SRV(7, targets->LumaInstability),
		nvrhi::BindingSetItem::Texture_SRV(2, targets->DilatedMotionVectors),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->FrameInfo),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->DilatedReactiveMask),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->CurrUpscaledBuffer),
		nvrhi::BindingSetItem::Texture_UAV(1, targets->PresentationBuffer),
		nvrhi::BindingSetItem::Texture_UAV(2, targets->NewLocks),
		nvrhi::BindingSetItem::Sampler(1, AssetManager::GetInstance()->Samplers[(int)SamplerTypes::Linear_Clamp])
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrAccumulate.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchDstX, DispatchDstY, 1);
	CommandListRef->endMarker();
}

void FSRPass::RCAS(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets, nvrhi::BufferHandle ConstantBuffer)
{
	CommandListRef->beginMarker("RCAS");

	nvrhi::BufferDesc CBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(Fsr3UpscalerRcasConstants), "RCAS CBuffer", 1);
	nvrhi::BufferHandle ConstantBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(CBufDesc);
	CommandListRef->writeBuffer(ConstantBufferHandle, &RcasConstants, sizeof(Fsr3UpscalerRcasConstants));

	nvrhi::BindingSetDesc setDesc;
	setDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(1, ConstantBufferHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, targets->CurrUpscaledBuffer),
		nvrhi::BindingSetItem::Texture_SRV(1, targets->FrameInfo),
		nvrhi::BindingSetItem::Texture_UAV(0, targets->PresentationBuffer),
	};
	nvrhi::BindingLayoutHandle layoutHandle = AssetManager::GetInstance()->GetBindingLayout(setDesc);
	nvrhi::BindingSetHandle setHandle = DirectXDevice::GetInstance()->CreateBindingSet(setDesc, layoutHandle);

	nvrhi::ComputePipelineDesc PipelineDesc;
	PipelineDesc.bindingLayouts = { layoutHandle };
	nvrhi::ShaderHandle shader = AssetManager::GetInstance()->GetShader("fsrRcas.cs.cso");
	PipelineDesc.CS = shader;

	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(PipelineDesc);
	state.bindings = { setHandle };
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(DispatchSrcX, DispatchSrcY, 1);
	CommandListRef->endMarker();
}

void FSRPass::UpdateConstants(const ragdoll::SceneInformation& sceneInfo, float _dt)
{
	Constants.previousFrameJitterOffset[0] = Constants.jitterOffset[0];
	Constants.previousFrameJitterOffset[1] = Constants.jitterOffset[1];
	Constants.jitterOffset[0] = sceneInfo.JitterX;
	Constants.jitterOffset[1] = sceneInfo.JitterY;

	Constants.previousFrameRenderSize[0] = Constants.renderSize[0];
	Constants.previousFrameRenderSize[1] = Constants.renderSize[1];
	Constants.renderSize[0] = sceneInfo.RenderWidth;
	Constants.renderSize[1] = sceneInfo.RenderHeight;
	Constants.maxRenderSize[0] = sceneInfo.TargetWidth;
	Constants.maxRenderSize[1] = sceneInfo.TargetHeight;
	Constants.previousFrameUpscaleSize[0] = Constants.upscaleSize[0];
	Constants.previousFrameUpscaleSize[1] = Constants.upscaleSize[1];
	Constants.upscaleSize[0] = sceneInfo.TargetWidth;
	Constants.upscaleSize[1] = sceneInfo.TargetHeight;
	Constants.maxUpscaleSize[0] = sceneInfo.TargetWidth;
	Constants.maxUpscaleSize[1] = sceneInfo.TargetHeight;
	Constants.velocityFactor = 1.f;

	Constants.tanHalfFOV = tanf(DirectX::XMConvertToRadians(sceneInfo.CameraFov) * 0.5f);
	Constants.viewSpaceToMetersFactor = 1.0f;
	Constants.deltaTime = _dt;

	// calculate pre-exposure relevant factors
	Constants.deltaPreExposure = 1.0f;
	previousFramePreExposure = preExposure;
	preExposure = 1.0f;

	if (previousFramePreExposure > 0.0f) {
		Constants.deltaPreExposure = preExposure / previousFramePreExposure;
	}

	static uint32_t frameIndex{};
	Constants.frameIndex = frameIndex;
	frameIndex++;

	const bool bInverted = true;
	const bool bInfinite = true;

	float far_p = FLT_MAX;
	// make sure it has no impact if near and far plane values are swapped in dispatch params
	// the flags "inverted" and "infinite" will decide what transform to use
	float fMin = std::min(sceneInfo.CameraNear, far_p);
	float fMax = std::max(sceneInfo.CameraNear, far_p);

	if (bInverted) {
		float tmp = fMin;
		fMin = fMax;
		fMax = tmp;
	}

	// a 0 0 0   x
	// 0 b 0 0   y
	// 0 0 c d   z
	// 0 0 e 0   1

	const float fQ = fMax / (fMin - fMax);
	const float d = -1.0f; // for clarity

	const float matrix_elem_c[2][2] = {
		fQ,                     // non reversed, non infinite
		-1.0f - FLT_EPSILON,    // non reversed, infinite
		fQ,                     // reversed, non infinite
		0.0f + FLT_EPSILON      // reversed, infinite
	};

	const float matrix_elem_e[2][2] = {
		fQ * fMin,             // non reversed, non infinite
		-fMin - FLT_EPSILON,    // non reversed, infinite
		fQ * fMin,             // reversed, non infinite
		fMax,                  // reversed, infinite
	};

	Constants.deviceToViewDepth[0] = d * matrix_elem_c[bInverted][bInfinite];
	Constants.deviceToViewDepth[1] = matrix_elem_e[bInverted][bInfinite];

	// revert x and y coords
	const float aspect = sceneInfo.RenderWidth / float(sceneInfo.RenderHeight);
	const float verticalFOVRadians = 2.0f * atan(tan(DirectX::XMConvertToRadians(sceneInfo.CameraFov) / 2.0f) / aspect);
	const float cotHalfFovY = cosf(0.5f * verticalFOVRadians) / sinf(0.5f * verticalFOVRadians);
	const float a = cotHalfFovY / aspect;
	const float b = cotHalfFovY;

	Constants.deviceToViewDepth[2] = (1.0f / a);
	Constants.deviceToViewDepth[3] = (1.0f / b);

	Constants.downscaleFactor[0] = float(Constants.renderSize[0]) / Constants.upscaleSize[0];
	Constants.downscaleFactor[1] = float(Constants.renderSize[1]) / Constants.upscaleSize[1];

	Constants.motionVectorScale[0] = 1.f;
	Constants.motionVectorScale[1] = 1.f;

	float paramSharpness = 1.f;
	float sharpenessRemapped = (-2.0f * paramSharpness) + 2.0f;
	// Transform from stops to linear value.
	float sharpness = exp2(-sharpenessRemapped);
	Vector2 hSharp = { sharpness, sharpness };

	RcasConstants.rcasConfig[0] = ffxAsUInt32(sharpness);
	RcasConstants.rcasConfig[1] = ffxPackHalf2x16(hSharp);
	RcasConstants.rcasConfig[2] = 0;
	RcasConstants.rcasConfig[3] = 0;
}
