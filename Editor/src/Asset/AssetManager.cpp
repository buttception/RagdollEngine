/*!
\file		AssetManager.cpp
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
#include "ragdollpch.h"
#include "AssetManager.h"

#include "Ragdoll/File/FileManager.h"

namespace ragdoll
{
	void AssetManager::Init(std::shared_ptr<FileManager> fileManager)
	{
		m_FileManager = fileManager;
		LoadDatabase();
	}

	void AssetManager::LoadDatabase()
	{
		//load all shaders first
		Guid id{ GuidGenerator::Generate() };
		std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();
		std::future<void> future = promise->get_future();
		m_FileManager->QueueRequest({id, "vertexshader.vert", [](Guid id, const uint8_t* data, uint32_t size)
		{
				//remember to null terminate the string since it is loaded in binary
				RD_CORE_TRACE("size:{} -> {}", size, std::string(reinterpret_cast<const char*>(data), size));
		}, promise});
		future.wait();
		promise = std::make_shared<std::promise<void>>();
		future = promise->get_future();
		m_FileManager->QueueRequest({ id, "fragmentshader.frag", [](Guid id, const uint8_t* data, uint32_t size)
		{
				//remember to null terminate the string since it is loaded in binary
				RD_CORE_TRACE("size:{} -> {}", size, std::string(reinterpret_cast<const char*>(data), size));
		}, promise });
		future.wait();
		promise = std::make_shared<std::promise<void>>();
		future = promise->get_future();
		m_FileManager->QueueRequest({ id, "testprogram.shdrprgm", [](Guid id, const uint8_t* data, uint32_t size)
		{
				//remember to null terminate the string since it is loaded in binary
				RD_CORE_TRACE("size:{} -> {}", size, std::string(reinterpret_cast<const char*>(data), size));
		}, promise });
		future.wait();
	}
}
