#include "ragdollpch.h"
#include "Profiler.h"

#include "microprofile.cpp"

std::vector<MicroProfileThreadLogGpu*> EnterCommandListSectionGpu::Logs;
std::vector<bool> EnterCommandListSectionGpu::LogsInUse;
std::mutex EnterCommandListSectionGpu::LogsMutex;
uint32_t EnterCommandListSectionGpu::Queue = MicroProfileInitGpuQueue("Gpu Queue");

EnterCommandListSectionGpu::EnterCommandListSectionGpu(const char* name, nvrhi::CommandListHandle cmdList)
{
	CommandList = cmdList;
	MicroProfileThreadLogGpu* log = nullptr;
	{
		std::lock_guard<std::mutex> lock(LogsMutex);	//this place confirm got race
		//search for the free one
		for (int i = 0; i < LogsInUse.size(); ++i) {
			if (!LogsInUse[i])
			{
				log = Logs[i];
				LogsInUse[i] = true;
				LogIndex = i;
				break;
			}
		}
		if (!log)
		{
			//not enuf logs get a new one
			log = Logs.emplace_back(MicroProfileThreadLogGpuAlloc());
			LogsInUse.emplace_back(true);
			LogIndex = (int32_t)LogsInUse.size() - 1;
		}
	}

	CommandList->open();
	MICROPROFILE_GPU_BEGIN(CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer, log);

	Token = MicroProfileGetToken("Gpu", name, MP_AUTO, MicroProfileTokenTypeGpu, 0);
	nTick = MicroProfileGpuEnterInternal(log, Token);
}

EnterCommandListSectionGpu::~EnterCommandListSectionGpu()
{
	//submit the logs
	MicroProfileGpuLeaveInternal(Logs[LogIndex], Token, nTick);
	CommandList->Work = MICROPROFILE_GPU_END(Logs[LogIndex]);
	//close the command list
	CommandList->close();
	{
		std::lock_guard<std::mutex> lock(LogsMutex); //could be setting to false while others are searching
		//free up the currently used log
		LogsInUse[LogIndex] = false;
	}
}
