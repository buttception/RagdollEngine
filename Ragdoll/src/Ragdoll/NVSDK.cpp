#include "ragdollpch.h"
#include "NVSDK.h"

#include <nvsdk_ngx_helpers.h>
#include <shlobj.h>

#include "DirectXDevice.h"

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

	NVSDK_NGX_Result res = NVSDK_NGX_D3D12_Init_with_ProjectID("a0f57b54-1daf-4934-90ae-c4035c19df04", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0", dir.c_str(), device, &feature);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to initialize");

	res = NVSDK_NGX_D3D12_AllocateParameters(&Parameters);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to allocatee params");
	res = NVSDK_NGX_D3D12_GetCapabilityParameters(&Parameters);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to get params");

	//check if dlss is supported
	int32_t DLSS_Supported = 0;
	res = Parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSS_Supported);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to get dlss support");
	RD_ASSERT(!DLSS_Supported, "DLSS is not supported on this system");
	RD_CORE_INFO("DLSS is supported by the current system");

	//get optimal settings for dlss
	unsigned int RenderWidth, RenderHeight, MaxWidth, MaxHeight, MinWidth, MinHeight;
	float Sharpness = 0.f;
	res = NGX_DLSS_GET_OPTIMAL_SETTINGS(Parameters, 1920, 1080, NVSDK_NGX_PerfQuality_Value_Balanced, &RenderWidth, &RenderHeight, &MaxWidth, &MaxHeight, &MinWidth, &MinHeight, &Sharpness);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to get optimal settings");

	//create the dlss feature
	//for now hard coded window size is 1280x720, then dlss it to 1920x1080 for easier resolution comp
	NVSDK_NGX_DLSS_Create_Params createParams;
	createParams.Feature.InWidth = 1280;
	createParams.Feature.InHeight = 720;
	createParams.Feature.InTargetWidth = 1920;
	createParams.Feature.InTargetHeight = 1080;
	createParams.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_Balanced;
	CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	CommandList->open();
	ID3D12GraphicsCommandList* raw = CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
	res = NGX_D3D12_CREATE_DLSS_EXT(raw, 1, 1, &FeatureHandle, Parameters, &createParams);
	CommandList->close();
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to create DLSS feature");
	RD_CORE_INFO("NVSDK: DLSS feature created");
}

void NVSDK::Release()
{
	//free the feature
	NVSDK_NGX_Result res = NVSDK_NGX_D3D12_ReleaseFeature(FeatureHandle);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to release DLSS feature");
	//free the params
	res = NVSDK_NGX_D3D12_DestroyParameters(Parameters);
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to release parameters");

	RD_CORE_INFO("NVSDK: Shutdown successful");
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
