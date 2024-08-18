/*!
\file		FileManager.h
\date		16/08/2024

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

//#include "Ragdoll/Memory/RagdollAllocator.h"

namespace ragdoll
{
	struct FileIORequest
	{
		enum class Type
		{
			Read,
			Write
		};
		enum class Priority
		{
			High = 0,
			Normal,
			Low
		};

		FileIORequest()
		{
			if(m_Guid == Guid::null)
				m_Guid = GuidGenerator::Generate();
		}
		FileIORequest(Guid guid, std::filesystem::path path, std::function<void(Guid, const uint8_t*, uint32_t)> callback, uint64_t offset = 0, uint64_t size = 0, Type type = Type::Read, Priority priority = Priority::Normal)
			: m_Guid{ guid }, m_Path(path), m_ReadCallback(callback), m_Offset(offset), m_Size(size), m_Type(type), m_Priority(priority)
		{
			if (m_Guid == Guid::null)
				RD_ASSERT(true, "Please generate a guid for your request and keep track of it");
		}
		FileIORequest(Guid guid, std::filesystem::path path, std::function<void(Guid, const uint8_t*, uint32_t)> callback, std::shared_ptr<std::promise<void>> promise, uint64_t offset = 0, uint64_t size = 0, Type type = Type::Read, Priority priority = Priority::Normal)
			: m_Guid{ guid }, m_Path(path), m_ReadCallback(callback), m_Promise(promise), m_Offset(offset), m_Size(size), m_Type(type), m_Priority(priority)
		{
			if (m_Guid == Guid::null)
				RD_ASSERT(true, "Please generate a guid for your request and keep track of it");
		}
		FileIORequest(Guid guid, std::filesystem::path path, uint8_t* data, uint32_t size, Type type = Type::Write, Priority priority = Priority::Normal)
			: m_Guid{ guid }, m_Path(path), m_WriteData(data), m_WriteSize(size), m_Type(type), m_Priority(priority)
		{
			if (m_Guid == Guid::null)
				RD_ASSERT(true, "Please generate a guid for your request and keep track of it");
		}
		FileIORequest(FileIORequest&&) = default;
		FileIORequest(const FileIORequest&) = default;
		~FileIORequest() = default;

		Guid m_Guid;
		Type m_Type;
		Priority m_Priority{ 0 };
		std::filesystem::path m_Path;
		uint64_t m_Offset{ 0 };
		uint64_t m_Size{ 0 };
		//id of the request, data ptr, and size of data for read
		std::function<void(Guid, const uint8_t*, uint32_t)> m_ReadCallback;
		//data ptr and size for write
		uint8_t* m_WriteData{ nullptr };
		uint32_t m_WriteSize{ 0 };
		std::shared_ptr<std::promise<void>> m_Promise{ nullptr };

		FileIORequest& operator=(FileIORequest&& other) noexcept;
		FileIORequest& operator=(const FileIORequest& other);
	};

	class FileManager
	{
		struct Buffer
		{
			enum class Status
			{
				Idle,
				Executing,
				Callback,
			} m_Status{ Status::Idle };
			FileIORequest m_Request;
			std::vector<uint8_t> m_Data;

			void Load(std::filesystem::path root);
		};
	public:
		FileManager();

		void Init();
		//checks when the file manager is done loading then can call the callbacks
		void Update();
		//check queue status and will load async, callback will be called when done in main thread
		void ThreadUpdate();
		void QueueRequest(FileIORequest request);

		void Shutdown();

		std::filesystem::path GetRoot() const { return m_Root; }

	private:
		//root directory
		std::filesystem::path m_Root = std::filesystem::current_path();
		//stop thread bool
		bool m_Running{ true };
		//queue of files to load
		std::priority_queue<FileIORequest, std::vector<FileIORequest>,
			std::function<bool(const FileIORequest&, const FileIORequest&)>> m_RequestQueue{
				[](const FileIORequest& lhs, const FileIORequest& rhs) {
					return lhs.m_Priority < rhs.m_Priority;
				}
		};
		//mutex for the queue
		std::unique_ptr<std::mutex> m_QueueMutex{};
		//double buffering loading system with a loader thread
		Buffer m_Buffer[2]{};
		//thread to do IO
		std::thread m_IOThread;
	};
}
