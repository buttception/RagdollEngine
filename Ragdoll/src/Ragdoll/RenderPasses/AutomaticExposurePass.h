#pragma once
#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
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
		float LogLuminanceRange;
		float TimeCoeff;
		float NumPixels;
	}LuminanceAverageCBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

	nvrhi::BufferHandle LuminanceHistogramHandle;

public:
	nvrhi::BufferHandle ReadbackBuffer;
	nvrhi::BufferHandle AdaptedLuminanceHandle;

	void Init(nvrhi::CommandListHandle cmdList);
	nvrhi::BufferHandle GetAdaptedLuminance(float _dt, ragdoll::SceneRenderTargets* targets);
};