/*!
\file		Vector2.h
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
#include "ragdollpch.h"
#pragma warning (disable: 4201) // nonstandard extension used: nameless struct/union

namespace Ragdoll
{
	union Vector2
	{
		float m_Data[2]{};
		struct { float x, y; };

		Vector2() = default;
		Vector2(const Vector2& other) : x(other.x), y(other.y) {}
		Vector2(Vector2&&) = default;

		Vector2(const float _x, const float _y) : x(_x), y(_y) {}
		Vector2(const float f) : x(f), y(f) {}
		~Vector2() = default;

		operator float* () { return m_Data; }
		operator const float* () const { return m_Data; }

		Vector2& operator=(const Vector2& rhs) = default;
		Vector2& operator=(Vector2&& rhs) = default;

	};
	inline std::ostream& operator<<(std::ostream& os, const Vector2& vector) { os << "(" << vector.x << ", " << vector.y << ")"; return os; }
}
