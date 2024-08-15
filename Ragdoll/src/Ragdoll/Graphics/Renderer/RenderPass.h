/*!
\file		RenderPass.h
\date		11/08/2024

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

#include "RenderState.h"

namespace ragdoll
{
	class Framebuffer;
	class ResourceManager;
	class EntityManager;
	class Window;
	class RenderGraph;
	class RenderPass
	{
	public:
		RenderPass(const char* name) : m_Name{ name } {}

		const char* GetName() const { return m_Name; }

		void Init(const std::shared_ptr<Window>& win,
			const std::shared_ptr<ResourceManager>& resourceManager,
			const std::shared_ptr<EntityManager>& entityManager)
		{
			m_PrimaryWindow = win;
			m_ResourceManager = resourceManager;
			m_EntityManager = entityManager;
		}
		void SetRenderState(const RenderState& renderState) { m_RenderState = renderState; }
		virtual void Execute() = 0;

	protected:
		const char* m_Name{};
		RenderState m_RenderState;
		std::shared_ptr<Window> m_PrimaryWindow;
		std::shared_ptr<ResourceManager> m_ResourceManager;
		std::shared_ptr<EntityManager> m_EntityManager;
		std::shared_ptr<RenderGraph> m_RenderGraph;
	};

	class TestRenderPass : public RenderPass
	{
	public:
		TestRenderPass() : RenderPass("test") {}
		void Execute() override;
	};
}
