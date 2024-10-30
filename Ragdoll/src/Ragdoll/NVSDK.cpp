#include "ragdollpch.h"
#include "NVSDK.h";

#include <shlobj.h>

std::filesystem::path GetDocumentsPath() {
	wchar_t path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, path))) {
		return std::filesystem::path(path);
	}
	else {
		throw std::runtime_error("Unable to retrieve Documents path");
	}
}

void NVSDK::Init(ID3D12Device* device)
{
	NVSDK_NGX_FeatureCommonInfo feature{};
	feature.LoggingInfo.LoggingCallback = LoggingCallback;

	const std::wstring dir = (GetDocumentsPath() / "NVSDK_Logs");
	if (!std::filesystem::exists(dir))
		std::filesystem::create_directories(dir);

	NVSDK_NGX_Result res = NVSDK_NGX_D3D12_Init_with_ProjectID("a0f57b54-1daf-4934-90ae-c4035c19df04", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", dir.c_str(), device);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "Failed to initialize NGSDK");

	NVSDK_NGX_Parameter* params;
	res = NVSDK_NGX_D3D12_AllocateParameters(&params);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "Failed to allocatee params NGSDK");
	res = NVSDK_NGX_D3D12_GetCapabilityParameters(&params);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "Failed to get params NGSDK");

	//check if dlss is supported
	int32_t DLSS_Supported = 0;
	res = params->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSS_Supported);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "Failed to get dlss support NGSDK");

	RD_ASSERT(!DLSS_Supported, "DLSS is not supported on this system");
	RD_CORE_INFO("DLSS is supported by the current system");

	//free the params
	NVSDK_NGX_D3D12_DestroyParameters(params);
}

void NVSDK::LoggingCallback(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
{
	switch(loggingLevel)
	{
	case NVSDK_NGX_LOGGING_LEVEL_ON:
	case NVSDK_NGX_LOGGING_LEVEL_VERBOSE:
		RD_CORE_INFO("NVSDK Log from {}: {}", (int)sourceComponent, message);
		break;
	default:
		RD_CORE_INFO("idk what these level stands for");
	}
}
