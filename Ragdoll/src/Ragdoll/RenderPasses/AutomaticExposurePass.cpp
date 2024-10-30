#include "ragdollpch.h"
#include "AutomaticExposurePass.h"

#include <nvrhi/utils.h>
#include "Ragdoll/Profiler.h"

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"
#include "Ragdoll/DirectXDevice.h"

void AutomaticExposurePass::Init(nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;

	nvrhi::BufferDesc histoDesc;
	histoDesc.byteSize = 256 * sizeof(uint32_t);
	histoDesc.debugName = "Luminance Histo Buffer";
	histoDesc.format = nvrhi::Format::R32_UINT;
	histoDesc.canHaveTypedViews = true;
	histoDesc.canHaveUAVs = true;
	histoDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	histoDesc.keepInitialState = true;
	LuminanceHistogramHandle = DirectXDevice::GetNativeDevice()->createBuffer(histoDesc);

	nvrhi::BufferDesc resultDesc;
	resultDesc.byteSize = sizeof(float);
	resultDesc.debugName = "Adapted Luminance";
	resultDesc.format = nvrhi::Format::R32_FLOAT;
	resultDesc.canHaveTypedViews = true;
	resultDesc.canHaveUAVs = true;
	resultDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	resultDesc.keepInitialState = true;
	AdaptedLuminanceHandle = DirectXDevice::GetNativeDevice()->createBuffer(resultDesc);

	nvrhi::BufferDesc readbackBufferDesc;
	readbackBufferDesc.byteSize = sizeof(float); // Size should match the size of the UAV buffer
	readbackBufferDesc.initialState = nvrhi::ResourceStates::CopyDest; // Set to CopyDestination for readback
	readbackBufferDesc.cpuAccess = nvrhi::CpuAccessMode::Read; // Enable CPU read access
	readbackBufferDesc.debugName = "ReadbackBuffer";

	ReadbackBuffer = DirectXDevice::GetNativeDevice()->createBuffer(readbackBufferDesc);

}

void AutomaticExposurePass::SetDependencies(nvrhi::TextureHandle sceneColor)
{
	SceneColor = sceneColor;
}

nvrhi::BufferHandle AutomaticExposurePass::GetAdaptedLuminance(float _dt)
{
	RD_SCOPE(Render, AutomaticExposure);
	RD_GPU_SCOPE("AutomaticExposure", CommandListRef);
	float minLuminance = -8.f;
	float maxLuminance = 8.5;

	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(struct LuminanceHistogramCBuffer), "Luminance Histo CBuffer", 1);
	nvrhi::BufferHandle LuminanceHistogramCBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(cBufDesc);
	cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(struct LuminanceAverageCBuffer), "Luminance Avg CBuffer", 1);
	nvrhi::BufferHandle LuminanceAverageCBufferHandle = DirectXDevice::GetNativeDevice()->createBuffer(cBufDesc);

	//binding layout and sets
	nvrhi::BindingSetDesc histoSetDesc;
	histoSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, LuminanceHistogramCBufferHandle),
		nvrhi::BindingSetItem::TypedBuffer_UAV(0, LuminanceHistogramHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, SceneColor)
	};
	nvrhi::BindingLayoutHandle LuminanceHistogramBindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(histoSetDesc);
	nvrhi::BindingSetHandle LuminanceHistogramBindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(histoSetDesc, LuminanceHistogramBindingLayoutHandle);

	nvrhi::BindingSetDesc averageSetDesc;
	averageSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, LuminanceAverageCBufferHandle),
		nvrhi::BindingSetItem::TypedBuffer_UAV(0, AdaptedLuminanceHandle),
		nvrhi::BindingSetItem::TypedBuffer_UAV(1, LuminanceHistogramHandle),
	};
	nvrhi::BindingLayoutHandle LuminanceAverageBindingLayoutHandle = AssetManager::GetInstance()->GetBindingLayout(averageSetDesc);
	nvrhi::BindingSetHandle LuminanceAverageBindingSetHandle = DirectXDevice::GetInstance()->CreateBindingSet(averageSetDesc, LuminanceAverageBindingLayoutHandle);

	//pipeline descs
	nvrhi::ComputePipelineDesc LuminanceHistogramPipelineDesc;
	LuminanceHistogramPipelineDesc.bindingLayouts = { LuminanceHistogramBindingLayoutHandle };
	nvrhi::ShaderHandle LuminanceHistogramShader = AssetManager::GetInstance()->GetShader("LuminanceHistogram.cs.cso");
	LuminanceHistogramPipelineDesc.CS = LuminanceHistogramShader;

	nvrhi::ComputePipelineDesc LuminanceAveragePipelineDesc;
	LuminanceAveragePipelineDesc.bindingLayouts = { LuminanceAverageBindingLayoutHandle };
	nvrhi::ShaderHandle LuminanceAverageShader = AssetManager::GetInstance()->GetShader("LuminanceAverage.cs.cso");
	LuminanceAveragePipelineDesc.CS = LuminanceAverageShader;

	CommandListRef->beginMarker("AutomaticExposure");
	nvrhi::ComputeState state;
	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(LuminanceHistogramPipelineDesc);
	state.bindings = { LuminanceHistogramBindingSetHandle };
	LuminanceHistogramCBuffer.Width = SceneColor->getDesc().width;
	LuminanceHistogramCBuffer.Height = SceneColor->getDesc().height;
	LuminanceHistogramCBuffer.MinLogLuminance = minLuminance;
	LuminanceHistogramCBuffer.InvLogLuminanceRange = 1.f / (maxLuminance - minLuminance);
	CommandListRef->writeBuffer(LuminanceHistogramCBufferHandle, &LuminanceHistogramCBuffer, sizeof(struct LuminanceHistogramCBuffer));
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(SceneColor->getDesc().width / 16 + 1, SceneColor->getDesc().height / 16 + 1, 1);

	state.pipeline = AssetManager::GetInstance()->GetComputePipeline(LuminanceAveragePipelineDesc);
	state.bindings = { LuminanceAverageBindingSetHandle };
	LuminanceAverageCBuffer.MinLogLuminance = minLuminance;
	LuminanceAverageCBuffer.LogLuminanceRange = maxLuminance - minLuminance;
	LuminanceAverageCBuffer.NumPixels = (float)SceneColor->getDesc().width * SceneColor->getDesc().height;
	LuminanceAverageCBuffer.TimeCoeff = 1.f - exp(-_dt * 1.1f);
	CommandListRef->writeBuffer(LuminanceAverageCBufferHandle, &LuminanceAverageCBuffer, sizeof(struct LuminanceAverageCBuffer));
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(1, 1, 1);

	//copy for the readback
	CommandListRef->copyBuffer(ReadbackBuffer, 0, AdaptedLuminanceHandle, 0, sizeof(float));

	CommandListRef->endMarker();
	return AdaptedLuminanceHandle;
}