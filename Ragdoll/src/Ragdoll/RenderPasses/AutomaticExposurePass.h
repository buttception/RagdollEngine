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
		float LogLuminanceRange;
		float TimeCoeff;
		float NumPixels;
	}LuminanceAverageCBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

	nvrhi::TextureHandle SceneColor;

	nvrhi::BufferHandle LuminanceHistogramHandle;

public:
	nvrhi::BufferHandle ReadbackBuffer;
	nvrhi::BufferHandle AdaptedLuminanceHandle;

	void Init(nvrhi::CommandListHandle cmdList);
	void SetDependencies(nvrhi::TextureHandle sceneColor);
	nvrhi::BufferHandle GetAdaptedLuminance(float _dt);
};