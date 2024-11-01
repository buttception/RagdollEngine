#pragma once
#include <nvsdk_ngx.h>
#include <nvrhi/nvrhi.h>

class NVSDK
{
public:
	static void Init(ID3D12Device* device);
	static void Evaluate();
	static void Release();

private:
	static void LoggingCallback(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);

	inline static nvrhi::CommandListHandle CommandList;
	inline static NVSDK_NGX_Parameter* Parameters;
	inline static NVSDK_NGX_Handle* FeatureHandle;
};