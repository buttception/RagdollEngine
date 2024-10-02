#pragma once
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	struct InstanceGroupInfo;
	struct SceneInformation;
}
class FSunSkyTable;
class FSunSkyPreetham;
class SkyGeneratePass {
	struct ConstantBuffer {
		Vector3 sun;
        uint32_t width;
		Vector3 PerezInvDen;
        uint32_t height;
	}CBuffer;

	nvrhi::CommandListHandle CommandListRef{ nullptr };

    nvrhi::TextureHandle SkyTexture;
	nvrhi::TextureHandle ThetaGammaTable;

	std::shared_ptr<FSunSkyTable> Table;
	std::shared_ptr<FSunSkyPreetham> SunSky;

public:
	void Init(nvrhi::CommandListHandle cmdList);

    void SetDependencies(nvrhi::TextureHandle sky, nvrhi::TextureHandle table);
	void GenerateSky(const ragdoll::SceneInformation& sceneInfo);
};