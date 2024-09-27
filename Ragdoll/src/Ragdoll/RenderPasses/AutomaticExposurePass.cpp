#include "ragdollpch.h"
#include "AutomaticExposurePass.h"

#include <nvrhi/utils.h>
#include <microprofile.h>

#include "Ragdoll/AssetManager.h"
#include "Ragdoll/Scene.h"

void AutomaticExposurePass::Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList)
{
	CommandListRef = cmdList;
	NvrhiDeviceRef = nvrhiDevice;

	//create the pipeline
	//load outputted shaders objects
	LuminanceHistogramShader = AssetManager::GetInstance()->GetShader("LuminanceHistogram.cs.cso");
	LuminanceAverageShader = AssetManager::GetInstance()->GetShader("LuminanceAverage.cs.cso");

	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::TypedBuffer_UAV(0),	//histo buffer
		nvrhi::BindingLayoutItem::Texture_SRV(0),	//scene color
	};
	LuminanceHistogramBindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
		nvrhi::BindingLayoutItem::TypedBuffer_UAV(0),	//histo buffer
		nvrhi::BindingLayoutItem::TypedBuffer_UAV(1),	//result
	};
	LuminanceAverageBindingLayoutHandle = NvrhiDeviceRef->createBindingLayout(layoutDesc);

	//create a constant buffer here
	nvrhi::BufferDesc cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(class LuminanceHistogramCBuffer), "Luminance Histo CBuffer", 1);
	LuminanceHistogramCBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);
	//create a constant buffer here
	cBufDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(sizeof(class LuminanceAverageCBuffer), "Luminance Avg CBuffer", 1);
	LuminanceAverageCBufferHandle = NvrhiDeviceRef->createBuffer(cBufDesc);

	nvrhi::BufferDesc histoDesc;
	histoDesc.byteSize = 256 * sizeof(uint32_t);
	histoDesc.debugName = "Luminance Histo Buffer";
	histoDesc.format = nvrhi::Format::R32_UINT;
	histoDesc.canHaveTypedViews = true;
	histoDesc.canHaveUAVs = true;
	histoDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	histoDesc.keepInitialState = true;
	LuminanceHistogramHandle = NvrhiDeviceRef->createBuffer(histoDesc);

	nvrhi::BufferDesc resultDesc;
	resultDesc.byteSize = sizeof(float);
	resultDesc.debugName = "Adapted Luminance";
	resultDesc.format = nvrhi::Format::R32_FLOAT;
	resultDesc.canHaveTypedViews = true;
	resultDesc.canHaveUAVs = true;
	resultDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
	resultDesc.keepInitialState = true;
	AdaptedLuminanceHandle = NvrhiDeviceRef->createBuffer(resultDesc);

	nvrhi::BufferDesc readbackBufferDesc;
	readbackBufferDesc.byteSize = sizeof(float); // Size should match the size of the UAV buffer
	readbackBufferDesc.initialState = nvrhi::ResourceStates::CopyDest; // Set to CopyDestination for readback
	readbackBufferDesc.cpuAccess = nvrhi::CpuAccessMode::Read; // Enable CPU read access
	readbackBufferDesc.debugName = "ReadbackBuffer";

	ReadbackBuffer = NvrhiDeviceRef->createBuffer(readbackBufferDesc);

	nvrhi::BindingSetDesc histoSetDesc;
	histoSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, LuminanceHistogramCBufferHandle),
		nvrhi::BindingSetItem::TypedBuffer_UAV(0, LuminanceHistogramHandle),
		nvrhi::BindingSetItem::Texture_SRV(0, SceneColor)
	};
	LuminanceHistogramBindingSetHandle = NvrhiDeviceRef->createBindingSet(histoSetDesc, LuminanceHistogramBindingLayoutHandle);

	nvrhi::BindingSetDesc averageSetDesc;
	averageSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, LuminanceAverageCBufferHandle),
		nvrhi::BindingSetItem::TypedBuffer_UAV(0, AdaptedLuminanceHandle),
		nvrhi::BindingSetItem::TypedBuffer_UAV(1, LuminanceHistogramHandle),
	};
	LuminanceAverageBindingSetHandle = NvrhiDeviceRef->createBindingSet(averageSetDesc, LuminanceAverageBindingLayoutHandle);

	auto pipelineDesc = nvrhi::ComputePipelineDesc();

	pipelineDesc.bindingLayouts = { LuminanceHistogramBindingLayoutHandle };
	pipelineDesc.CS = LuminanceHistogramShader;
	LuminanceHistogramPipeline = AssetManager::GetInstance()->GetComputePipeline(pipelineDesc);
	pipelineDesc.bindingLayouts = { LuminanceAverageBindingLayoutHandle };
	pipelineDesc.CS = LuminanceAverageShader;
	LuminanceAveragePipeline = AssetManager::GetInstance()->GetComputePipeline(pipelineDesc);
}

void AutomaticExposurePass::SetDependencies(nvrhi::TextureHandle sceneColor)
{
	SceneColor = sceneColor;
}

nvrhi::BufferHandle AutomaticExposurePass::GetAdaptedLuminance(float _dt)
{
	float minLuminance = -8.f;
	float maxLuminance = 3.5;

	CommandListRef->beginMarker("AutomaticExposure");
	nvrhi::ComputeState state;
	state.pipeline = LuminanceHistogramPipeline;
	state.bindings = { LuminanceHistogramBindingSetHandle };
	LuminanceHistogramCBuffer.Width = SceneColor->getDesc().width;
	LuminanceHistogramCBuffer.Height = SceneColor->getDesc().height;
	LuminanceHistogramCBuffer.MinLogLuminance = minLuminance;
	LuminanceHistogramCBuffer.InvLogLuminanceRange = 1.f / (maxLuminance - minLuminance);
	CommandListRef->writeBuffer(LuminanceHistogramCBufferHandle, &LuminanceHistogramCBuffer, sizeof(class LuminanceHistogramCBuffer));
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(SceneColor->getDesc().width / 16 + 1, SceneColor->getDesc().height / 16 + 1, 1);

	state.pipeline = LuminanceAveragePipeline;
	state.bindings = { LuminanceAverageBindingSetHandle };
	LuminanceAverageCBuffer.MinLogLuminance = minLuminance;
	LuminanceAverageCBuffer.LogLuminanceRange = maxLuminance - minLuminance;
	LuminanceAverageCBuffer.NumPixels = SceneColor->getDesc().width * SceneColor->getDesc().height;
	LuminanceAverageCBuffer.TimeCoeff = 1.f - exp(-_dt * 1.1f);
	CommandListRef->writeBuffer(LuminanceAverageCBufferHandle, &LuminanceAverageCBuffer, sizeof(class LuminanceAverageCBuffer));
	CommandListRef->setComputeState(state);
	CommandListRef->dispatch(1, 1, 1);
	CommandListRef->copyBuffer(ReadbackBuffer, 0, AdaptedLuminanceHandle, 0, sizeof(float));

	CommandListRef->endMarker();
	return AdaptedLuminanceHandle;
}