/*!
\file		RenderGraph.cpp
\date		08/08/2024

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

#include "RenderGraph.h"

#include "RenderPass.h"
#include "RenderData.h"
#include "Ragdoll/Graphics/Containers/Framebuffer.h"
#include "Ragdoll/Graphics/Window/Window.h"
#include "Ragdoll/Resource/ResourceManager.h"
#include "Ragdoll/Entity/EntityManager.h"

namespace ragdoll
{
	void RenderGraph::Init(std::shared_ptr<Window> window, std::shared_ptr<ResourceManager> resManager, std::shared_ptr<EntityManager> entManager)
	{
		m_PrimaryWindow = window;
		m_ResourceManager = resManager;
		m_EntityManager = entManager;

		//create all the render passes here
		CreateRenderPasses();
		//sort so we can just iterate through the passes
		std::vector<std::shared_ptr<RenderPass>> sorted;
		TopologicalSort(sorted);
		m_RenderPasses = std::move(sorted);
		//we will not allow for render passes to be added after the init
	}

	void RenderGraph::Execute()
	{
		//clear all framebuffers
		for(const auto& [name, fb] : m_ResourceManager->GetFramebuffers())
		{
			fb->Clear();
		}
		for(const auto& pass : m_RenderPasses)
		{
			pass->Execute();
		}
	}

	void RenderGraph::CreateRenderPasses()
	{
		auto testPass = AddRenderPass<TestRenderPass>(true);
		testPass->Init(m_PrimaryWindow, m_ResourceManager, m_EntityManager);
	}

	void RenderGraph::CreateDependency(std::shared_ptr<RenderPass> from, std::shared_ptr<RenderPass> to)
	{
		m_DependencyGraph[from].emplace_back(to);
	}

	void RenderGraph::TopologicalSort(std::vector<std::shared_ptr<RenderPass>>& sorted)
	{
		std::unordered_map<std::shared_ptr<RenderPass>, bool> visited;

		// Depth-first search for topological sorting
		std::function<void(std::shared_ptr<RenderPass>)> visit = [&](std::shared_ptr<RenderPass> pass) {
			if (visited[pass]) return;
			visited[pass] = true;

			for (const auto& dep : m_DependencyGraph[pass])
			{
				visit(dep);
			}

			sorted.push_back(pass);
			};

		// Perform DFS from each source pass
		for (const auto& source : m_Sources)
		{
			visit(source);
		}

		// Reverse the order to get the topological sort
		std::reverse(sorted.begin(), sorted.end());
	}
}
