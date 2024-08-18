/*!
\file		Application.cpp
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

#include "ragdollpch.h"

#include "Application.h"

#include "Core/Logger.h"
#include "Core/Core.h"
#include "Entity/EntityManager.h"
#include "Graphics/Window/Window.h"
#include "Graphics/GLFWContext.h"
#include "Graphics/OpenGLContext.h"
#include "Event/WindowEvents.h"
#include "Event/KeyEvents.h"
#include "Event/MouseEvent.h"
#include "glad/glad.h"
#include "Input/InputHandler.h"
#include "Graphics/Renderer/RenderGraph.h"
#include "Graphics/Transform/Transform.h"
#include "Layer/LayerStack.h"
#include "Graphics/Transform/TransformLayer.h"
#include "Resource/ResourceManager.h"
#include "File/FileManager.h"

#include "Ragdoll/Graphics/Renderer/RenderData.h"
ragdoll::Transform* t1, *t2, *t3, *t4, *t5;

namespace ragdoll
{
	void Application::Init(const ApplicationConfig& config)
	{
		UNREFERENCED_PARAMETER(config);
		Logger::Init();
		RD_CORE_INFO("spdlog initialized for use.");

		GLFWContext::Init();

		m_PrimaryWindow = std::make_shared<Window>();
		m_PrimaryWindow->Init();
		//bind the application callback to the window
		m_PrimaryWindow->SetEventCallback(RD_BIND_EVENT_FN(Application::OnEvent));
		//setup opengl context
		m_Context = std::make_shared<OpenGLContext>();
		m_Context->Init(m_PrimaryWindow);
		//setup input handler
		m_InputHandler = std::make_shared<InputHandler>();
		m_InputHandler->Init();
		//create the entity manager
		m_EntityManager = std::make_shared<EntityManager>();
		//create the resource manager
		m_ResourceManager = std::make_shared<ResourceManager>();
		m_ResourceManager->Init(m_PrimaryWindow);
		//create the render graph
		m_RenderGraph = std::make_shared<RenderGraph>();
		m_RenderGraph->Init(m_PrimaryWindow, m_ResourceManager, m_EntityManager);
		//create the file manager
		m_FileManager = std::make_shared<FileManager>();
		m_FileManager->Init();
		//RD_CORE_INFO("{}", m_FileManager->GetRoot().string());
		//Guid guid = GuidGenerator::Generate();
		//m_FileManager->QueueRequest(FileIORequest{ guid, "vertexshader.vtx", [](Guid id, const uint8_t* data, uint32_t size)
		//{
		//	//remember to null terminate the string since it is loaded in binary
		//	RD_CORE_TRACE("size:{} -> {}", size, std::string(reinterpret_cast<const char*>(data), size));
		//}});

		//layers stuff
		m_LayerStack = std::make_shared<LayerStack>();
		//adding the transform layer
		m_TransformLayer = std::make_shared<TransformLayer>(m_EntityManager);
		m_LayerStack->PushLayer(m_TransformLayer);

		//init all layers
		m_LayerStack->Init();

		//test the transform layer
		auto e1 = m_EntityManager->CreateEntity();
		auto e2 = m_EntityManager->CreateEntity();
		auto e3 = m_EntityManager->CreateEntity();
		auto e4 = m_EntityManager->CreateEntity();
		auto e5 = m_EntityManager->CreateEntity();
		t1 = m_EntityManager->AddComponent<Transform>(e1);
		t1->m_LocalPosition = { 0.3f, 0.f, 0.f };
		m_TransformLayer->SetEntityAsRoot(m_EntityManager->GetGuid(e1));
		t2 = m_EntityManager->AddComponent<Transform>(e2);
		t2->m_LocalPosition = { 0.3f, 0.f, 0.f };
		t2->m_Parent = m_EntityManager->GetGuid(e1);
		t1->m_Child = m_EntityManager->GetGuid(e2);
		t3 = m_EntityManager->AddComponent<Transform>(e3);
		t3->m_LocalPosition = { 0.3f, 0.f, 0.f };
		t3->m_Parent = m_EntityManager->GetGuid(e2);
		t2->m_Child = m_EntityManager->GetGuid(e3);
		t4 = m_EntityManager->AddComponent<Transform>(e4);
		t4->m_LocalPosition = { 0.f, 0.3f, 0.f };
		t4->m_Parent = m_EntityManager->GetGuid(e1);
		t2->m_Sibling = m_EntityManager->GetGuid(e4);
		t5 = m_EntityManager->AddComponent<Transform>(e5);
		t5->m_LocalPosition = { 0.f, 0.3f, 0.f };
		t5->m_Parent = m_EntityManager->GetGuid(e4);
		t4->m_Child = m_EntityManager->GetGuid(e5);

		//add all of this into the render graph data
		std::vector<RenderData> renderData;
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e1), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e2), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e3), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e4), 0 }});
		renderData.push_back({.m_DrawMesh{ m_EntityManager->GetGuid(e5), 0 }});
		m_ResourceManager->AddRenderData("test", std::move(renderData));
	}

	void Application::Run()
	{
		while(m_Running)
		{
			//input update must be called before window polls for inputs
			m_InputHandler->Update(m_PrimaryWindow->GetDeltaTime());
			m_FileManager->Update();
			m_PrimaryWindow->StartRender();

			for(auto& layer : *m_LayerStack)
			{
				layer->Update(static_cast<float>(m_PrimaryWindow->GetDeltaTime()));
			}

			m_RenderGraph->Execute();

			m_PrimaryWindow->EndRender();
		}
	}

	void Application::Shutdown()
	{
		m_FileManager->Shutdown();
		m_PrimaryWindow->Shutdown();
		GLFWContext::Shutdown();
		RD_CORE_INFO("ragdoll Engine application shut down successfull");
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		RD_DISPATCH_EVENT(dispatcher, WindowCloseEvent, event, Application::OnWindowClose);
		RD_DISPATCH_EVENT(dispatcher, WindowResizeEvent, event, Application::OnWindowResize);
		RD_DISPATCH_EVENT(dispatcher, WindowMoveEvent, event, Application::OnWindowMove);
		RD_DISPATCH_EVENT(dispatcher, KeyPressedEvent, event, Application::OnKeyPressed);
		RD_DISPATCH_EVENT(dispatcher, KeyReleasedEvent, event, Application::OnKeyReleased);
		RD_DISPATCH_EVENT(dispatcher, KeyTypedEvent, event, Application::OnKeyTyped);
		RD_DISPATCH_EVENT(dispatcher, MouseMovedEvent, event, Application::OnMouseMove);
		RD_DISPATCH_EVENT(dispatcher, MouseButtonPressedEvent, event, Application::OnMouseButtonPressed);
		RD_DISPATCH_EVENT(dispatcher, MouseButtonReleasedEvent, event, Application::OnMouseButtonReleased);
		RD_DISPATCH_EVENT(dispatcher, MouseScrolledEvent, event, Application::OnMouseScrolled);

		//run the event on all layers
		for(auto& layer : *m_LayerStack)
		{
			layer->OnEvent(event);
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		return false;
	}

	bool Application::OnWindowMove(WindowMoveEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		return false;
	}

	bool Application::OnKeyPressed(KeyPressedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnKeyPressed(event);
		return false;
	}

	bool Application::OnKeyReleased(KeyReleasedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnKeyReleased(event);
		return false;
	}

	bool Application::OnKeyTyped(KeyTypedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnKeyTyped(event);
		return false;
	}

	bool Application::OnMouseMove(MouseMovedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseMove(event);
		return false;
	}

	bool Application::OnMouseButtonPressed(MouseButtonPressedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseButtonPressed(event);
		return false;
	}

	bool Application::OnMouseButtonReleased(MouseButtonReleasedEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseButtonReleased(event);
		return false;
	}

	bool Application::OnMouseScrolled(MouseScrolledEvent& event)
	{
#if RD_LOG_EVENT
		RD_CORE_TRACE("Event: {}", event.ToString());
#endif
		m_InputHandler->OnMouseScrolled(event);
		return false;
	}
}
