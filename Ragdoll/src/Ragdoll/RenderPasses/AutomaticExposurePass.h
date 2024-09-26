#pragma once
#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class AutomaticExposurePass {
	struct LuminanceHistogramCBuffer {
		float MinLogLuminance;
		float InvLogLuminanceRange;
		uint32_t Width;
		uint32_t Height;
	}LuminanceHistogramCBuffer;
	struct LuminanceAverageCBuffer {
		float MinLogLuminance;
		float InvLogLuminanceRange;
		float TimeCoeff;
		float NumPixels;
	}LuminanceAverageCBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };
	nvrhi::DeviceHandle NvrhiDeviceRef{ nullptr };

	nvrhi::TextureHandle SceneColor;

	nvrhi::ShaderHandle LuminanceHistogramShader;
	nvrhi::ShaderHandle LuminanceAverageShader;
	nvrhi::BindingLayoutHandle LuminanceHistogramBindingLayoutHandle;
	nvrhi::BindingLayoutHandle LuminanceAverageBindingLayoutHandle;
	nvrhi::BindingSetHandle LuminanceHistogramBindingSetHandle;
	nvrhi::BindingSetHandle LuminanceAverageBindingSetHandle;

	nvrhi::ComputePipelineHandle LuminanceHistogramPipeline;
	nvrhi::ComputePipelineHandle LuminanceAveragePipeline;

	nvrhi::BufferHandle LuminanceHistogramCBufferHandle;
	nvrhi::BufferHandle LuminanceAverageCBufferHandle;
	nvrhi::BufferHandle LuminanceHistogramHandle;
	nvrhi::BufferHandle AdaptedLuminanceHandle;

public:
	nvrhi::BufferHandle ReadbackBuffer;

	void Init(nvrhi::DeviceHandle nvrhiDevice, nvrhi::CommandListHandle cmdList);
	void SetDependencies(nvrhi::TextureHandle sceneColor);
	nvrhi::BufferHandle GetAdaptedLuminance(float _dt);
};