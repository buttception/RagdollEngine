#pragma once
#include <winsock2.h>
#pragma comment(lib, "ws2_32")

#define MICROPROFILE_IMPL
#define MICROPROFILE_GPU_TIMERS 1
#define MICROPROFILE_GPU_TIMERS_D3D12 1
#include "microprofile.h"
#include <nvrhi/nvrhi.h>
#include "Core/Core.h"

#define RD_SCOPE(group, name) auto CONCAT(group, __LINE__) = ProfileScope(STRINGIFY(group), STRINGIFY(name));
struct ProfileScope {
	ProfileScope(const char* group, const char* name) {
		// Register or get the profiling group token
		Token = MicroProfileGetToken(group, name, MP_AUTO, MicroProfileTokenTypeCpu, 0);

		// Manually start profiling using the token
		MicroProfileEnter(Token);
	}

	~ProfileScope() {
		// Manually end profiling when the object is destructed
		MicroProfileLeave();
	}

	MicroProfileToken Token;
};

#define RD_GPU_SCOPE(name, cmdList) auto CONCAT(rdGpu, __LINE__) = EnterCommandListSectionGpu(name, cmdList);
#define RD_GPU_SUBMIT(cmdLists)\
	for(auto& it : cmdLists){ MICROPROFILE_GPU_SUBMIT(EnterCommandListSectionGpu::Queue, it->Work);	}

class EnterCommandListSectionGpu {
	static std::vector<MicroProfileThreadLogGpu*> Logs;
	static std::vector<bool> LogsInUse;
	static std::mutex LogsMutex;

	nvrhi::CommandListHandle CommandList;
	int32_t LogIndex = -1;
	MicroProfileToken Token;
	uint64_t nTick;
public:
	static uint32_t Queue;

	EnterCommandListSectionGpu(const char* name, nvrhi::CommandListHandle cmdList);
	~EnterCommandListSectionGpu();
};