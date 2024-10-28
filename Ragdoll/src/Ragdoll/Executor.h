#pragma once
#include "taskflow.hpp"

#define TF_THREAD_COUNT 8

struct SExecutor
{
	static tf::Executor Executor;
};