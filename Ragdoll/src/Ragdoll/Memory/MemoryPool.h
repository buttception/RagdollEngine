/*!
\file		MemoryPool.h
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

namespace ragdoll
{
	template<size_t sz>
	struct MemoryBlock
	{
		static_assert(sz > sizeof(uint64_t));
		union
		{
			MemoryBlock* m_Next{ nullptr };	//if unused, it will be used as a pointer to the next block
			struct
			{
				uint64_t m_ContiguousBlocksInUse;	//if used, it will point to how many blocks in use
				uint8_t m_Payload[sz - sizeof(uint64_t)];			//data
			} m_Data;
		};
	};

	template<size_t blockSize, size_t blockCount>
	struct MemoryPool
	{
		MemoryBlock<blockSize>* m_MemoryBlocks{ nullptr };
		MemoryBlock<blockSize>* m_FreeBlock{ nullptr };

		MemoryPool()
		{
			m_MemoryBlocks = new MemoryBlock<blockSize>[blockCount];
			m_FreeBlock = m_MemoryBlocks;
			for (size_t i = 0; i < blockCount - 1; i++)
			{
				m_MemoryBlocks[i].m_Next = &m_MemoryBlocks[i + 1];
				//RD_CORE_TRACE("{}", reinterpret_cast<uint8_t*>(m_MemoryBlocks[i].m_Next) - reinterpret_cast<uint8_t*>(&m_MemoryBlocks[i]));
			}
		}

		~MemoryPool()
		{
			delete[] m_MemoryBlocks;
		}

		bool CheckValid(void* p) const
		{
			if (!p)
				return false;
			int64_t offset = static_cast<uint8_t*>(p) - reinterpret_cast<uint8_t*>(m_MemoryBlocks);
			if (offset < 0 || offset >= blockSize * blockCount)
				return false;
			offset -= sizeof(uint64_t);
			if (offset % blockSize == 0)
				return true;
			return false;
		}
	};
}
