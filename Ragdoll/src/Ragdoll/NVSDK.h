#pragma once
#include <nvsdk_ngx.h>
#include <nvrhi/nvrhi.h>

class NVSDK
{
public:
	inline static nvrhi::CommandListHandle CommandList;

	static void Init(ID3D12Device* device);
	static void Evaluate(nvrhi::TextureHandle InColor, nvrhi::TextureHandle OutColor, nvrhi::TextureHandle InDepth, nvrhi::TextureHandle InMotionVector);
	static void Release();

private:
	static void LoggingCallback(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);

	inline static NVSDK_NGX_Parameter* Parameters;
	inline static NVSDK_NGX_Handle* FeatureHandle;
};