/*!
\file		Vector3.h
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
#include "Vector2.h"

namespace Ragdoll
{
	union Vector3
	{
		float m_Data[3]{};
		struct { float x, y, z; };

		Vector3() = default;
		Vector3(const Vector3& other) : x(other.x), y(other.y), z(other.z) {}
		Vector3(Vector3&&) = default;

		Vector3(const float _x, const float _y, const float _z) : x(_x), y(_y), z(_z) {}
		Vector3(const Vector2& vector, const float _z) : x(vector.x), y(vector.y), z(_z) {}
		Vector3(const float f) : x(f), y(f), z(f) {}
		~Vector3() = default;

		operator float* () { return m_Data; }
		operator const float* () const { return m_Data; }

		Vector3& operator=(const Vector3& rhs) = default;
		Vector3& operator=(Vector3&& rhs) = default;
	};
	inline std::ostream& operator<<(std::ostream& os, const Vector3& vector) { os << "(" << vector.x << ", " << vector.y << ", " << vector.z << ")"; return os; }
}
