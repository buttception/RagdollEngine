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
#include "Event/WindowEvents.h"
#include "Event/KeyEvents.h"
#include "Event/MouseEvent.h"
#include "Input/InputHandler.h"
#include "Components/TransformSystem.h"
#include "File/FileManager.h"

#include "DirectXDevice.h"
#include "microprofile.h"

MICROPROFILE_DEFINE(MAIN, "MAIN", "Main", MP_AUTO);

//testing
#include "Components/RenderableComp.h"
#include "Components/TransformComp.h"
#include "AssetManager.h"

namespace ragdoll
{
	void Application::Init(const ApplicationConfig& config)
	{
		UNREFERENCED_PARAMETER(config);
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

			//adding the transform layer
			m_TransformSystem = std::make_shared<TransformSystem>(m_EntityManager);

			Renderer.Init(m_PrimaryWindow, m_FileManager, m_EntityManager);
			m_ImguiInterface.Init(Renderer.Device.get(), Renderer.ImguiVertexShader, Renderer.ImguiPixelShader);
		}
		MICROPROFILE_TIMELINE_LEAVE_STATIC("Application Initialization");


		MICROPROFILE_TIMELINE_ENTER_STATIC(MP_DARKGOLDENROD, "GLTF Load");
		{
			//GLTFLoader loader;
			//loader.Init(m_FileManager->GetRoot(), &Renderer, m_FileManager, m_EntityManager, m_TransformLayer);
			//std::string sceneName{ "Sponza" };
			//std::filesystem::path fp = "gltf/2.0/";
			//fp = fp / sceneName / "glTF" / (sceneName + ".gltf");
			//loader.LoadAndCreateModel(fp.string());

			//loader.LoadAndCreateModel("Instancing Test/FlyingWorld-BattleOfTheTrashGod.gltf");
		}
		MICROPROFILE_TIMELINE_LEAVE_STATIC("GLTF Load");
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
			m_TransformSystem->UpdateTransforms();
			m_ImguiInterface.BeginFrame();
			Renderer.Draw();

			//spawn more shits temp
			const static Vector3 t_range{ 10.f, 10.f, 10.f };
			const static Vector3 t_min = { -5.f, -5.f, -5.f };
			const static Vector3 s_range{ 0.2f, 0.2f, 0.2f };
			const static Vector3 s_min{ 0.3f, 0.3f, 0.3f };
			static int32_t currentGeomCount{};

			ImGui::Begin("Spawn");
			ImGui::Text("Current Geom Count: %d", currentGeomCount);
			if (ImGui::Button("Spawn 100 geometry")) {
				for (int i = 0; i < 100; ++i) {
					currentGeomCount++;
					Vector3 pos{
						t_min.x + (std::rand() / (float)RAND_MAX) * t_range.x,
						t_min.y + (std::rand() / (float)RAND_MAX) * t_range.y,
						t_min.z + (std::rand() / (float)RAND_MAX) * t_range.z,
					};
					Vector3 scale{
						s_min.x + (std::rand() / (float)RAND_MAX) * s_range.x,
						s_min.y + (std::rand() / (float)RAND_MAX) * s_range.y,
						s_min.z + (std::rand() / (float)RAND_MAX) * s_range.z,
					};
					entt::entity ent = m_EntityManager->CreateEntity();
					auto tcomp = m_EntityManager->AddComponent<TransformComp>(ent);
					tcomp->m_LocalPosition = pos;
					tcomp->m_LocalScale = scale;
					m_TransformSystem->AddEntityAtRootLevel(m_EntityManager->GetGuid(ent));
					auto rcomp = m_EntityManager->AddComponent<RenderableComp>(ent);
					rcomp->meshIndex = std::rand() / (float)RAND_MAX * AssetManager::GetInstance()->Meshes.size();
				}
			}
			ImGui::End();

			m_ImguiInterface.Render();
			Renderer.Device->Present();

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
		Renderer.Shutdown();
		m_FileManager->Shutdown();
		m_PrimaryWindow->Shutdown();
		GLFWContext::Shutdown();
		MicroProfileShutdown();
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