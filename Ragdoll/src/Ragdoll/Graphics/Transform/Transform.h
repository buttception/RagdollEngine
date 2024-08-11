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
#include "glm/gtc/quaternion.hpp"
#include "Ragdoll/Math/RagdollMath.h"
#include "Ragdoll/Entity/Component.h"
#include "Ragdoll/Core/Guid.h"

namespace ragdoll
{
	struct Transform : Component
	{
		glm::vec3 m_LocalPosition{};
		glm::vec3 m_LocalScale{ 1.f,1.f,1.f };
		glm::quat m_LocalRotation{ 1.f, 0.f,0.f,0.f };

		//cached as shaders need this always
		glm::mat4 m_ModelToWorld;

		//let child right sibling system
		Guid m_Parent{};
		Guid m_Child{};
		Guid m_Sibling{};

		bool m_Dirty{ true };
	};
}
