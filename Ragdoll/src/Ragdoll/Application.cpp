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
#include <microprofile.h>

#include "Core/Logger.h"
#include "Core/Core.h"
#include "Entity/EntityManager.h"
#include "Graphics/Window/Window.h"
#include "Graphics/GLFWContext.h"
#include "Event/WindowEvents.h"
#include "Event/KeyEvents.h"
#include "Event/MouseEvent.h"
#include "Input/InputHandler.h"
#include "File/FileManager.h"
#include "Scene.h"

#include "DirectXDevice.h"
#include "GLTFLoader.h"

MICROPROFILE_DEFINE(MAIN, "MAIN", "Main", MP_AUTO);

//testing
#include "Components/RenderableComp.h"
#include "Components/TransformComp.h"
#include "AssetManager.h"

namespace ragdoll
{
	void Application::Init(const ApplicationConfig& config)
	{
		Config = config;
		Logger::Init();
		RD_CORE_INFO("spdlog initialized for use.");

		//create the profiling threads
		MicroProfileInit();
		MicroProfileOnThreadCreate("Main");
		//turn on profiling
		MicroProfileSetEnableAllGroups(true);
		MicroProfileSetForceMetaCounters(true);
		RD_CORE_INFO("Open localhost:{} in chrome to capture profile data", MicroProfileWebServerPort());

		MICROPROFILE_TIMELINE_ENTER_STATIC(MP_DARKGOLDENROD, "Application Initialization");
		{
			GLFWContext::Init();

			m_PrimaryWindow = std::make_shared<Window>();
			m_PrimaryWindow->Init();
			//bind the application callback to the window
			m_PrimaryWindow->SetEventCallback(RD_BIND_EVENT_FN(Application::OnEvent));
			//setup input handler
			m_InputHandler = std::make_shared<InputHandler>();
			m_InputHandler->Init();
			//create the entity manager
			m_EntityManager = std::make_shared<EntityManager>();
			//create the file manager
			m_FileManager = std::make_shared<FileManager>();
			m_FileManager->Init();

			m_Scene = std::make_shared<Scene>(this);
		}
		MICROPROFILE_TIMELINE_LEAVE_STATIC("Application Initialization");


		MICROPROFILE_TIMELINE_ENTER_STATIC(MP_DARKGOLDENROD, "GLTF Load");
		{
			GLTFLoader loader;
			loader.Init(m_FileManager->GetRoot(), m_Scene->Renderer.get(), m_FileManager, m_EntityManager, m_Scene);
			if (!Config.glTfSampleSceneToLoad.empty())
			{
				std::string sceneName = Config.glTfSampleSceneToLoad;
				std::filesystem::path fp = "gltf/2.0/";
				fp = fp / sceneName / "glTF" / (sceneName + ".gltf");
				loader.LoadAndCreateModel(fp.string());
			}
			else if (!Config.glTfSceneToLoad.empty())
			{
				loader.LoadAndCreateModel(Config.glTfSceneToLoad);
			}
		}
		MICROPROFILE_TIMELINE_LEAVE_STATIC("GLTF Load");

		m_Scene->UpdateTransforms();
		m_Scene->PopulateStaticProxies();
		m_Scene->ResetTransformDirtyFlags();
	}

	void Application::Run()
	{
		while (m_Running)
		{
			MICROPROFILE_SCOPE(MAIN);
			//m_InputHandler->Update(m_PrimaryWindow->GetDeltaTime());
			m_FileManager->Update();
			{
				MICROPROFILE_SCOPEI("Update", "Wait", MP_YELLOW);
				while (m_Frametime < m_TargetFrametime) {
					m_PrimaryWindow->Update();
					m_Frametime += m_PrimaryWindow->GetDeltaTime();
					YieldProcessor();
				}
			}

			m_Scene->Update(m_Frametime);
			m_PrimaryWindow->SetFrametime(m_Frametime);
			m_PrimaryWindow->IncFpsCounter();
			m_Frametime = 0;
			MicroProfileFlip(nullptr);

			if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
				MicroProfileDumpFileImmediately("test", "test", nullptr);
			}
		}
	}

	void Application::Shutdown()
	{
		AssetManager::GetInstance()->Release();
		m_Scene->Shutdown();
		m_FileManager->Shutdown();
		m_PrimaryWindow->Shutdown();
#ifdef _DEBUG
		// to prevent the tzdb allocations from being reported as memory leaks
		std::chrono::get_tzdb_list().~tzdb_list();
#endif
		GLFWContext::Shutdown();
		MicroProfileShutdown();
		RD_CORE_INFO("ragdoll Engine application shut down successfull");
		Logger::Shutdown();
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