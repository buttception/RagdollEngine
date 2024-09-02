/*!
\file		Resource.h
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
	enum class ResourceType
	{
		Shader,

		TYPE_COUNT
	};

	struct IResource	//interface to share common functionality between different resource types
	{
		Guid m_Guid;
		//the dependencies it has
		std::vector<Guid> m_Dependencies;
	};

	template<typename T>
	struct Resource : IResource
	{
		inline const static std::shared_ptr<T> null{ nullptr };
		//pointer to the actual resource type
		std::shared_ptr<T> m_Data;

		std::shared_ptr<T> operator->() const { return m_Data; }

		//this will only take bytes to load into the resource
		void Load(const char* data, uint32_t size)
		{
			//behavior on what to do will be up to constructors
			m_Data = std::make_shared<T>(data, size);
		}
		//this will free the resource`
		void Unload()
		{
			//behavior on what to do will be up to destructors
			m_Data.reset();
		}
	};
}
