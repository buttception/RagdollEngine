#include "ragdollpch.h"

#include <winsock2.h>
#pragma comment(lib, "ws2_32")

#define MICROPROFILE_IMPL
#define MICROPROFILE_GPU_TIMERS 1
#define MICROPROFILE_GPU_TIMERS_D3D12 1
#include "microprofile.h"
#include "microprofile.cpp"