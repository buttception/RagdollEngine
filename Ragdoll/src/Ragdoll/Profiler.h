#pragma once
#include <winsock2.h>
#pragma comment(lib, "ws2_32")

#define MICROPROFILE_IMPL
#define MICROPROFILE_GPU_TIMERS 1
#define MICROPROFILE_GPU_TIMERS_D3D12 1
#include "microprofile.h"
#include "Core/Core.h"

#define PROFILE_SCOPE(group, name) auto CONCAT(group, __LINE__) = ProfileScope(STRINGIFY(group), STRINGIFY(name));
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

struct ProfileGPUScope {
	//temp for testing
	inline static std::mutex Mutex;
};