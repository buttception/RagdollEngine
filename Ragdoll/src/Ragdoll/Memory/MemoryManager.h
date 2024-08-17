/*!
\file		MemoryManager.h
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

#include "MemoryPool.h"

namespace ragdoll
{
	class MemoryManager
	{
		static constexpr size_t c_BlockSize{ 4096 };
		static constexpr size_t c_BlockCount{ 500000 };
	public:
		static MemoryManager& GetInstance()
		{
			if(!s_Instance)
				s_Instance = std::make_unique<MemoryManager>();
			return *s_Instance;
		}

		template<typename T>
		[[nodiscard]] T* Allocate(const size_t n = 1)
		{
			RD_CRITICAL_ASSERT(!m_MemoryPool.m_FreeBlock, "Memory Pool is full!");
			MemoryBlock<c_BlockSize>* curr{ m_MemoryPool.m_FreeBlock };
			MemoryBlock<c_BlockSize>* start{ m_MemoryPool.m_FreeBlock };
			size_t size = n * sizeof(T) + sizeof(uint64_t);
			size_t count = size / c_BlockSize + (size % c_BlockSize ? 1 : 0);
			bool enoughSpace{ count == 1 };	//its enough if we need 1 block as free list is not null
			size_t i{ 1 };
			//search for a set of free blocks big enough to fit all the data
			while(!enoughSpace)
			{
				if (!curr)
					RD_CRITICAL_ASSERT(true, "Not enough contiguous free blocks, needed {} for {} bytes", count, size);
				if(CheckContiguous(curr, curr->m_Next))
				{
					i++;
					if(i == count)
						enoughSpace = true;
				}
				else
				{
					i = 1;
					start = curr->m_Next;
				}
				curr = curr->m_Next;
			}
			m_MemoryPool.m_FreeBlock = curr->m_Next;
			start->m_Data.m_ContiguousBlocksInUse = count;
			//RD_CORE_TRACE("Allocating memory at {0:x}", reinterpret_cast<uint64_t>(start));
			return reinterpret_cast<T*>(start->m_Data.m_Payload);
		}

		template<typename T>
		void Deallocate(void* p)
		{
			//RD_CORE_TRACE("Deallocating memory at {0:x}", reinterpret_cast<uint64_t>(p));
			if (!m_MemoryPool.CheckValid(p))
				RD_CRITICAL_ASSERT(true, "Pointer is not valid");
			MemoryBlock<c_BlockSize>* block{ reinterpret_cast<MemoryBlock<c_BlockSize>*>(static_cast<uint8_t*>(p) - sizeof(uint64_t))};
			//set the contiguous blocks to free list
			SetFree(block, block->m_Data.m_ContiguousBlocksInUse);
			m_MemoryPool.m_FreeBlock = block;
		}

	private:
		inline static std::unique_ptr<MemoryManager> s_Instance;
		MemoryPool<c_BlockSize, c_BlockCount> m_MemoryPool;

		bool CheckContiguous(MemoryBlock<c_BlockSize>* first, MemoryBlock<c_BlockSize>* second)
		{
			if (!first || !second)
				return false;
			return first + 1 == second;
		}

		void SetFree(MemoryBlock<c_BlockSize>* block, uint64_t count)
		{
			if (count <= 1)
			{
				block->m_Next = m_MemoryPool.m_FreeBlock;
				return;
			}
			SetFree(block + 1, --count);
			block->m_Next = block + 1;
		}
	};
}
