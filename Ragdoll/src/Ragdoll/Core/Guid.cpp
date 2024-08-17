/*!
\file		Guid.cpp
\date		10/08/2024

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

#include "ragdollpch.h"

#include "Guid.h"

#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include "Core.h"
#include "Logger.h"

namespace ragdoll
{
	const Guid Guid::null{ 0 };

	GuidGenerator::GuidGenerator()
	{
		auto now = std::chrono::system_clock::now();
		auto now_time = floor<std::chrono::days>(now);
		m_Settings.m_Date = std::chrono::year_month_day(now_time);
		m_Settings.m_StartTime = std::chrono::utc_clock::now();
		auto macAddress = GetMacAddress();
		if(macAddress == "unknown")
		{
			m_Settings.m_MachineId = 0;
			RD_CORE_WARN("Failed to get mac address, setting machine id to 0");
		}
		else
		{
			m_Settings.m_MachineId = ConvertMacAddressToNumber(macAddress);
		}
		m_Initialized = true;
	}

	uint64_t GuidGenerator::GenerateGuid()
	{
		RD_ASSERT(!m_Initialized, "Guid generator not initialized");
		const auto currentTime = ConvertTime(std::chrono::utc_clock::now()) - m_StartTime;

		if (m_ElapsedTime < currentTime)
		{
			m_ElapsedTime = currentTime;
			m_Sequence = 0;
		}
		else
		{
			m_Sequence = (m_Sequence + 1) & MaskSequence;
			if (m_Sequence == 0)
			{
				m_ElapsedTime++;
				const auto overtime = m_ElapsedTime - currentTime;
				std::this_thread::sleep_for(std::chrono::nanoseconds(static_cast<int>(static_cast<double>(overtime) * 1e7)));
			}
		}
		RD_ASSERT(m_ElapsedTime > (1ULL << BitLenTime), "Over time limit");

		return m_ElapsedTime << (BitLenSequence + BitLenMachineId) | static_cast<uint64_t>(m_Sequence) << BitLenMachineId | static_cast<uint64_t>(m_MachineID);
	}

	uint64_t GuidGenerator::GenerateGuid(const std::chrono::utc_clock::time_point& timestamp)
	{
		RD_ASSERT(!m_Initialized, "Guid generator not initialized");
		const auto timestampElapsed = ConvertTime(timestamp) - m_StartTime;

		m_Sequence = (m_Sequence + 1) & MaskSequence;

		if (m_Sequence == 0)
		{
			m_ElapsedTime++;
		}

		return static_cast<uint64_t>(timestampElapsed) << (BitLenSequence + BitLenMachineId) | static_cast<uint64_t>(m_Sequence) << BitLenMachineId | static_cast<uint64_t>(m_MachineID);
	}

	uint64_t GuidGenerator::GetTime(const uint64_t& uuid)
	{
		RD_ASSERT(!m_Initialized, "Guid generator not initialized");
		return uuid >> (BitLenSequence + BitLenMachineId);
	}

	uint16_t GuidGenerator::GetSequence(const uint64_t& uuid)
	{
		RD_ASSERT(!m_Initialized, "Guid generator not initialized");
		constexpr uint64_t sequenceMask{ MaskSequence << BitLenMachineId };
		return static_cast<uint16_t>(uuid & sequenceMask >> BitLenMachineId);
	}

	uint16_t GuidGenerator::GetMachineId(const uint64_t& uuid)
	{
		RD_ASSERT(!m_Initialized, "Guid generator not initialized");
		constexpr uint64_t machineIdMask{ (1ULL << BitLenMachineId) - 1 };
		return static_cast<uint16_t>(uuid & machineIdMask);
	}

	int64_t GuidGenerator::ConvertTime(const std::chrono::utc_clock::time_point& time)
	{
		RD_ASSERT(!m_Initialized, "Guid generator not initialized");
		return static_cast<int64_t>(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(
			time.time_since_epoch()).count()) / 1e7);
	}

	std::string GuidGenerator::GetMacAddress()
	{
		DWORD dwSize = 0;
		GetAdaptersInfo(nullptr, &dwSize); // Get required buffer size
		std::vector<char> buffer(dwSize);
		IP_ADAPTER_INFO* pAdapterInfo = reinterpret_cast<IP_ADAPTER_INFO*>(buffer.data());

		if (GetAdaptersInfo(pAdapterInfo, &dwSize) == ERROR_SUCCESS) {
			for (IP_ADAPTER_INFO* pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
				if (pAdapter->AddressLength != 0) {
					std::stringstream macAddress;
					for (UINT i = 0; i < pAdapter->AddressLength; ++i) {
						if (i != 0) macAddress << '-';
						macAddress << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(pAdapter->Address[i]);
					}
					return macAddress.str();
				}
			}
		}
		return "unknown";
	}

	uint64_t GuidGenerator::ConvertMacAddressToNumber(const std::string& macAddress)
	{
		uint64_t numericId = 0;
		std::stringstream ss(macAddress);
		std::string byte;
		while (std::getline(ss, byte, '-')) {
			numericId = (numericId << 8) | std::stoul(byte, nullptr, 16);
		}
		return numericId;
	}
}
