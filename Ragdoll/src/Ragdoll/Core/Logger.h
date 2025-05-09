﻿/*!
\file		Logger.h
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

#include "Guid.h"

#define SPDLOG_WCHAR_TO_UTF8_SUPPORT
#include "spdlog/spdlog.h"

namespace ragdoll
{
	class Logger
	{
	public:
		/**
		* \brief Initializes spdlog
		*/
		static void Init();
		/**
		 * \brief Shutdown spdlog
		 */
		static void Shutdown();

		/**
		 * \brief Getter for Meow Core logger
		 * \return Returns a spdlog logger
		 */
		static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }

		/**
		 * \brief Getter for Meow Client logger
		 * \return Returns a spdlog logger
		 */
		static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		/**
		 * \brief The core logger used for in-engine logging
		 */
		static inline std::shared_ptr<spdlog::logger> s_CoreLogger;
		/**
		 * \brief The client used for outside logging, such as C#
		 */
		static inline std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}
#define RD_CORE_TRACE(...) ::ragdoll::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define RD_CORE_INFO(...) ::ragdoll::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define RD_CORE_WARN(...) ::ragdoll::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define RD_CORE_ERROR(...) ::ragdoll::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define RD_CORE_FATAL(...) ::ragdoll::Logger::GetCoreLogger()->critical(__VA_ARGS__)

#define RD_CLIENT_TRACE(...) ::ragdoll::Logger::GetClientLogger()->trace(__VA_ARGS__)
#define RD_CLIENT_INFO(...) ::ragdoll::Logger::GetClientLogger()->info(__VA_ARGS__)
#define RD_CLIENT_WARN(...) ::ragdoll::Logger::GetClientLogger()->warn(__VA_ARGS__)
#define RD_CLIENT_ERROR(...) ::ragdoll::Logger::GetClientLogger()->error(__VA_ARGS__)
#define RD_CLIENT_FATAL(...) ::ragdoll::Logger::GetClientLogger()->critical(__VA_ARGS__)

#include "spdlog/fmt/bundled/ostream.h"
#include "Ragdoll/Math/RagdollMath.h"
#define RD_LOG_OVERLOAD_USERTYPE(type, var, format)\
inline std::ostream& operator<<(std::ostream& os, const type& var){\
	return os << format;\
}\
template <> struct fmt::formatter<type> : ostream_formatter {}

RD_LOG_OVERLOAD_USERTYPE(Vector2, vec, "(" << vec.x << ", " << vec.y << ")");
RD_LOG_OVERLOAD_USERTYPE(Vector3, vec, "(" << vec.x << ", " << vec.y << ", " << vec.z << ")");
RD_LOG_OVERLOAD_USERTYPE(Vector4, vec, "(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ")");
RD_LOG_OVERLOAD_USERTYPE(Quaternion, vec, "(" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ")");
RD_LOG_OVERLOAD_USERTYPE(ragdoll::Guid, id, id.m_RawId);