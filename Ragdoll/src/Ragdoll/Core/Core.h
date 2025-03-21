﻿/*!
\file		Core.h
\date		05/08/2024

\author		Devin Tan
\email		devintrh@gmail.com

\copyright	MIT License

			Copyright © 2024 Tan Rui Hao Devin

			Permission is hereby granted, free of charge, to any person obtaining a copy
			of this software and associated documentation files (the "Software"), to deal
			in the Software without restriction, including without limitation the rights
			to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
			copies of the Software, and to permit persons to whom the Software is
			furnished to do so, subject to the following conditions:

			The above copyright notice and this permission notice shall be included in all
			copies or substantial portions of the Software.

			THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
			IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
			FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
			AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
			LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
			OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
			SOFTWARE.
__________________________________________________________________________________*/
#pragma once

//Platform detection
#ifdef _WIN32
	#ifdef _WIN64
		#define RAGDOLL_PLATFORM_WINDOWS
	#else
		#error "x86 Builds are not supported for ragdoll Engine."
	#endif
#elif defined(__APPLE__) || defined (__MACH__)
	#include <TargetConditionals.h>
	#if TARGET_IPHONE_EMULATOR == 1
		#error "IOS emulator is not supported."
	#elif TARGET_OS_PHONE == 1
		#define RAGDOLL_PLATFORM_IOS
		#error "IOS is not supported."
	#elif TARGET_OS_MAC == 1
		#define RAGDOLL_PLATFORM_IOS
		#error "MacOS is not supported."
	#else
		#error "Unknown Apple Platform."
	#endif
#elif defined(__ANDROID__) //check android before linux as android has linux kernel
	#define RAGDOLL_PLATFORM_ANDROID
	#error "Android is not supported."
#elif define(__LINUX__)
	#define RAGDOLL_PLATFORM_LINUX
	#error "Linux is not supported."
#else
	#error "Unknown Platform."
#endif

#ifdef RAGDOLL_DEBUG
	#define RAGDOLL_ENABLE_ASSERTS
	#define _CRTDBG_MAP_ALLOC
	#include <cstdlib>
	#include <crtdbg.h>
#endif

// Assert macros
#ifdef RAGDOLL_ENABLE_ASSERTS
	#define RD_ASSERT(x, ...) do { if(x) { RD_CORE_FATAL("Assertion failed!"); RD_CORE_ERROR(__VA_ARGS__); __debugbreak(); } } while (0)
	#define RD_CRITICAL_ASSERT(x, ...) RD_ASSERT(x, __VA_ARGS__)
#else
	#define RD_ASSERT(x, ...) do { if(x) { RD_CORE_FATAL("Assertion failed!"); RD_CORE_ERROR(__VA_ARGS__); } } while (0)
	#define RD_CRITICAL_ASSERT(x, ...) RD_ASSERT(x, __VA_ARGS__); if(x) { RD_CORE_FATAL("Fatal error occured, please consult the logs"); exit(EXIT_FAILURE); }
#endif

// Bit macro helper
#define BIT(x) (1 << x)

// Stringify macro
#define STRINGIFY(x) #x

#define CONCAT(a, b) a##b

#define DIVIDE_ROUNDING_UP(x, y) ((x + y - 1) / y)

// Function pointer binding with std::functions
#define RD_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)
namespace ragdoll
{
	class Event;
}
using EventCallbackFn = std::function<void(ragdoll::Event&)>;

#define RD_LOG_EVENT false
#define RD_LOG_INPUT false

//ignore warnings
#pragma warning(push)
#pragma warning(disable : 4201) // Disable warning C4201: nonstandard extension used: nameless struct/union