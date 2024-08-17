/*!
\file		TransformLayer.cpp
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

#include "ragdollpch.h"

#include "TransformLayer.h"

#include "Transform.h"

#include "Ragdoll/Layer/Layer.h"
#include "Ragdoll/Entity/EntityManager.h"

namespace ragdoll
{
	TransformLayer::TransformLayer(std::shared_ptr<EntityManager> reg) : Layer{ "TransformLayer" }, m_EntityManager { reg }
	{

	}

	void TransformLayer::Init()
	{
	}

	void TransformLayer::Update(float _dt)
	{
		UNREFERENCED_PARAMETER(_dt);
		TraverseTreeAndUpdateTransforms();
	}

	void TransformLayer::Shutdown()
	{
	}

	void TransformLayer::TraverseTreeAndUpdateTransforms()
	{
		if(m_RootEntity)
		{
			TraverseNode(m_RootEntity);
			if(m_RootSibling)
				TraverseNode(m_RootSibling);
		}
	}

	void TransformLayer::TraverseNode(const Guid& guid)
	{
		auto transform = m_EntityManager->GetComponent<Transform>(guid);
		if(transform)
		{
			//check if state is dirty
			bool dirtyFromHere = false;
			if(!m_DirtyOnwards)
			{
				if (transform->m_Dirty)
				{
					m_DirtyOnwards = true;
					dirtyFromHere = true;
				}
			}
			if(m_DirtyOnwards)
			{
				//get local matrix
				auto localModel = GetLocalModelMatrix(*transform);
				//add to model stack
				if (m_ModelStack.empty())	//first matrix will be this guy local
					m_ModelStack.push(localModel);
				else
					m_ModelStack.push(m_ModelStack.top() * localModel);	//otherwise concatenate matrix in stacks
				transform->m_ModelToWorld = m_ModelStack.top();	//set model matrix to the top of the stack
			}
			else
				m_ModelStack.push(transform->m_ModelToWorld);	//if not dirty, just push the current model matrix
			//traverse children
			if(transform->m_Child)
				TraverseNode(transform->m_Child);

			//once done, pop the stack
			m_ModelStack.pop();
			//resets the dirty flags
			if (dirtyFromHere)
				m_DirtyOnwards = false;
			transform->m_Dirty = false;

			//traverse siblings
			if(transform->m_Sibling)
				TraverseNode(transform->m_Sibling);
		}
		else
		{
			RD_CORE_ERROR("Entity {} is a child despite not being a transform object", guid);
		}
	}

	glm::mat4 TransformLayer::GetLocalModelMatrix(const Transform& trans)
	{
		auto model = glm::translate(glm::mat4(1.0f), trans.m_LocalPosition);
		model *= glm::toMat4(trans.m_LocalRotation);
		model *= glm::scale(glm::mat4(1.0f), trans.m_LocalScale);
		return model;
	}
}
