#pragma once
#include <nvsdk_ngx.h>

class NVSDK
{
public:
	static void Init(ID3D12Device*);

private:
	static void LoggingCallback(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);
};