#pragma once
#include <nvsdk_ngx.h>
#include <nvrhi/nvrhi.h>

namespace ragdoll {
	class Scene;
}
class NVSDK
{
public:
	inline static nvrhi::CommandListHandle CommandList{};

	static void Init(ID3D12Device* device, Vector2 RenderRes, Vector2 TargetRes);
	static void Evaluate(nvrhi::TextureHandle InColor, nvrhi::TextureHandle OutColor, nvrhi::TextureHandle InDepth, nvrhi::TextureHandle InMotionVector, ragdoll::Scene* scene);
	static void Release();

	static inline uint32_t RenderWidth{}, RenderHeight{}, MaxWidth{}, MaxHeight{}, MinWidth{}, MinHeight{};
	static inline float Sharpness{};

private:
	static void LoggingCallback(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);

	inline static NVSDK_NGX_Parameter* Parameters;
	inline static NVSDK_NGX_Handle* FeatureHandle;
};