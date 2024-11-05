#include "ragdollpch.h"
#include "NVSDK.h"

#include <nvsdk_ngx_helpers.h>
#include <shlobj.h>

#include "DirectXDevice.h"
#include "Profiler.h"
#include "Scene.h"

std::filesystem::path GetDocumentsPath() {
	wchar_t path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, path))) {
		return std::filesystem::path(path);
	}
	else {
		throw std::runtime_error("Unable to retrieve Documents path");
	}
}

void NVSDK::Init(ID3D12Device* device, Vector2 RenderRes, Vector2 TargetRes)
{
	NVSDK_NGX_FeatureCommonInfo feature{};
	//feature.LoggingInfo.LoggingCallback = LoggingCallback;
	feature.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;

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
	float Sharpness = 0.f;
	res = NGX_DLSS_GET_OPTIMAL_SETTINGS(Parameters, 1920, 1080, NVSDK_NGX_PerfQuality_Value_MaxQuality, &OptimalRenderWidth, &OptimalRenderHeight, &MaxWidth, &MaxHeight, &MinWidth, &MinHeight, &Sharpness);
	RenderWidth = (uint32_t)RenderRes.x;
	RenderHeight = (uint32_t)RenderRes.y;
	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to get optimal settings");

	//create the dlss feature
	//for now hard coded window size is 1280x720, then dlss it to 1920x1080 for easier resolution comp
	NVSDK_NGX_DLSS_Create_Params createParams{};
	createParams.Feature.InWidth = RenderWidth;
	createParams.Feature.InHeight = RenderHeight;
	createParams.Feature.InTargetWidth = (uint32_t)TargetRes.x;
	createParams.Feature.InTargetHeight = (uint32_t)TargetRes.y;
	if (RenderWidth == (uint32_t)TargetRes.x && RenderHeight == (uint32_t)TargetRes.y)
	{
		createParams.InFeatureCreateFlags |= NVSDK_NGX_PerfQuality_Value_DLAA;
		bIsDLAA = true;
	}
	else
	{
		createParams.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_MaxQuality;
		bIsDLAA = false;

		createParams.InFeatureCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
		createParams.InFeatureCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
		createParams.InFeatureCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
	}
	if(!CommandList)
		CommandList = DirectXDevice::GetNativeDevice()->createCommandList();
	{
		RD_SCOPE(Init, DLSS);
		RD_GPU_SCOPE("DLSS", CommandList);
		ID3D12GraphicsCommandList* raw = CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
		if (FeatureHandle)
			NVSDK_NGX_D3D12_ReleaseFeature(FeatureHandle);
		FeatureHandle = nullptr;
		res = NGX_D3D12_CREATE_DLSS_EXT(raw, 1, 1, &FeatureHandle, Parameters, &createParams);
	}
	MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, CommandList->Work);
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);

	RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to create DLSS feature");
	RD_CORE_INFO("NVSDK: DLSS feature created");
}

void NVSDK::Evaluate(nvrhi::TextureHandle InColor, nvrhi::TextureHandle OutColor, nvrhi::TextureHandle InDepth, nvrhi::TextureHandle InMotionVector, ragdoll::Scene* scene)
{
	{
		RD_SCOPE(Render, DLSS);
		RD_GPU_SCOPE("DLSS", CommandList);

		NVSDK_NGX_D3D12_DLSS_Eval_Params evalParams{};
		evalParams.Feature.pInColor = InColor->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
		evalParams.Feature.pInOutput = OutColor->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
		evalParams.Feature.InSharpness = Sharpness;
		evalParams.pInDepth = InDepth->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);
		evalParams.pInMotionVectors = InMotionVector->getNativeObject(nvrhi::ObjectTypes::D3D12_Resource);

		if (scene->SceneInfo.bEnableJitter)
		{
			evalParams.InJitterOffsetX = scene->JitterOffsetsX[scene->PhaseIndex];
			evalParams.InJitterOffsetY = -scene->JitterOffsetsY[scene->PhaseIndex];
		}
		evalParams.InRenderSubrectDimensions.Width = RenderWidth;
		evalParams.InRenderSubrectDimensions.Height = RenderHeight;

		NVSDK_NGX_Result res = NGX_D3D12_EVALUATE_DLSS_EXT(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList), FeatureHandle, Parameters, &evalParams);
		RD_ASSERT(res != NVSDK_NGX_Result_Success, "NGSDK: Failed to evaluate DLSS feature");
	}

	MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, CommandList->Work);
	DirectXDevice::GetNativeDevice()->executeCommandList(CommandList);
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
	case NVSDK_NGX_LOGGING_LEVEL_OFF:
	case NVSDK_NGX_LOGGING_LEVEL_ON:
	case NVSDK_NGX_LOGGING_LEVEL_VERBOSE:
		RD_CORE_TRACE("{}: {}", (int)sourceComponent, message);
		break;
	default:
		RD_CORE_WARN("NVSDK logged a unknown logging level message");
	}
}
