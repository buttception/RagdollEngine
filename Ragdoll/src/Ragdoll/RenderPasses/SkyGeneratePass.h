#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
	struct SceneRenderTargets;
}
class FSunSkyTable;
class FSunSkyPreetham;
class SkyGeneratePass {
	struct ConstantBuffer {
		Vector3 sun;
        uint32_t width;
		Vector3 PerezInvDen;
        uint32_t height;
		float maxTheta;
		float maxGamma;
		float scalar;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

	std::shared_ptr<FSunSkyTable> Table;
	std::shared_ptr<FSunSkyPreetham> SunSky;

public:
	void Init(nvrhi::CommandListHandle cmdList);

	void GenerateSky(const ragdoll::SceneInformation& sceneInfo, ragdoll::SceneRenderTargets* targets);
};