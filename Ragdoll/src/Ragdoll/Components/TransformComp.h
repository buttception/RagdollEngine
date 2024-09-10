/*!
\file		Transform.h
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
#pragma once
#include "Ragdoll/Math/RagdollMath.h"
#include "Ragdoll/Entity/Component.h"
#include "Ragdoll/Core/Guid.h"

struct TransformComp
{
	Vector3 m_LocalPosition = Vector3::Zero;
	Vector3 m_LocalScale = Vector3::One;
	Quaternion m_LocalRotation = Quaternion::Identity;

	//cached as shaders need this always
	Matrix m_ModelToWorld;

	//let child right sibling system
	ragdoll::Guid m_Parent{};
	ragdoll::Guid m_Child{};
	ragdoll::Guid m_Sibling{};

	int32_t glTFId{};
	bool m_Dirty{ true };
};
