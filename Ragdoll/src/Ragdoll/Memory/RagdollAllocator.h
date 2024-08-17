/*!
\file		MemoryAllocator.h
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
#include "MemoryManager.h"

namespace ragdoll
{
	template<typename T>
	struct RagdollAllocator
	{
		typedef T value_type;

		RagdollAllocator() noexcept = default;
		template <class U> constexpr RagdollAllocator(const RagdollAllocator <U>&) noexcept {}

		[[nodiscard]] T* allocate(size_t n)
		{
			if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();
			if (T* p = static_cast<T*>(MemoryManager::GetInstance().Allocate<T>(n))) {
				return p;
			}

			throw std::bad_alloc();
		}
		void deallocate(T* p, size_t n) const
		{
			UNREFERENCED_PARAMETER(n);
			MemoryManager::GetInstance().Deallocate<T>(p);
		}
	};
}
