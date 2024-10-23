#include "ragdollpch.h"
#include "Executor.h"

tf::Executor SExecutor::Executor = tf::Executor(TF_THREAD_COUNT);