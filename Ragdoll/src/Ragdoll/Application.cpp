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
#include "glad/glad.h"
#include "Input/InputHandler.h"
#include "Graphics/Renderer/RenderGraph.h"
#include "Graphics/Transform/Transform.h"
#include "Layer/LayerStack.h"
#include "Graphics/Transform/TransformLayer.h"

#include "glm/gtc/type_ptr.hpp"
#include "Ragdoll/Graphics/Shader/ShaderProgram.h"
#include "Ragdoll/Graphics/Containers/VertexArray.h"
#include "Ragdoll/Graphics/Containers/VertexBuffer.h"
#include "Ragdoll/Graphics/Containers/IndexBuffer.h"
#include "Ragdoll/Graphics/Containers/Framebuffer.h"
std::shared_ptr<ragdoll::VertexArray> vao;
std::shared_ptr<ragdoll::ShaderProgram> sp;
std::shared_ptr<ragdoll::Framebuffer> fb;
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
		//setup input handler
		m_InputHandler = std::make_shared<InputHandler>();
		m_InputHandler->Init();
		//create the entity manager
		m_EntityManager = std::make_shared<EntityManager>();
		//create the render graph
		m_RenderGraph = std::make_shared<RenderGraph>();
		m_RenderGraph->Init(m_PrimaryWindow);

		//layers stuff
		m_LayerStack = std::make_shared<LayerStack>();
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

		//testing opengl code
		// Vertex and fragment shader source code
		const char* vertexShaderSource = R"(
			#version 330 core
			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec3 aColor;
			out vec3 fragColor;
			uniform mat4 model;
			void main()
			{
			    gl_Position = model * vec4(aPos, 1.0);
			    fragColor = aColor;
			}
			)";

		const char* fragmentShaderSource = R"(
			#version 330 core
			in vec3 fragColor;
			out vec4 color;
			void main()
			{
			    color = vec4(fragColor, 1.0);
			}
			)";
		// Cube vertices
		float vertices[] = {
			// positions          // colors
			-0.1f, -0.1f, -0.1f, 1.f, 0.0f, 0.0f, // back face
			 0.1f, -0.1f, -0.1f, 0.0f, 1.f, 0.0f,
			 0.1f,  0.1f, -0.1f, 0.0f, 0.0f, 1.f,
			-0.1f,  0.1f, -0.1f, 1.f, 1.f, 0.0f,

			-0.1f, -0.1f,  0.1f, 1.f, 0.0f, 1.f, // front face
			 0.1f, -0.1f,  0.1f, 0.0f, 1.f, 1.f,
			 0.1f,  0.1f,  0.1f, 1.f, 1.f, 1.f,
			-0.1f,  0.1f,  0.1f, 0.1f, 0.1f, 0.1f
		};

		// Cube indices
		uint32_t indices[] = {
			0, 1, 2, 2, 3, 0, // back face
			4, 5, 6, 6, 7, 4, // front face
			0, 1, 5, 5, 4, 0, // bottom face
			2, 3, 7, 7, 6, 2, // top face
			0, 3, 7, 7, 4, 0, // left face
			1, 2, 6, 6, 5, 1  // right face
		};

		// Compile and link shaders
		std::shared_ptr<Shader> vs = std::make_shared<Shader>("test vertex shader", vertexShaderSource, ShaderType::Vertex);
		std::shared_ptr<Shader> fs = std::make_shared<Shader>("test fragment shader", fragmentShaderSource, ShaderType::Fragment);
		sp = std::make_shared<ShaderProgram>("test shader program");
		sp->AttachShaders({ vs, fs });
		sp->PrintAttributes();
		sp->PrintUniforms();

		// Generate and bind VAO, VBO, and EBO
		vao = std::make_shared<VertexArray>();
		std::shared_ptr<VertexBuffer> vbo = std::make_shared<VertexBuffer>(vertices, sizeof(vertices), GL_STATIC_DRAW);
		vbo->SetLayout({
			{ ShaderDataType::Float3, "aPos" },
			{ ShaderDataType::Float3, "aColor" }
			});
		std::shared_ptr<IndexBuffer> ibo = std::make_shared<IndexBuffer>(indices, sizeof(indices));
		vao->AddVertexBuffer(vbo);
		vao->SetIndexBuffer(ibo);

		//create the fb
		fb = std::make_shared<Framebuffer>("test");
		fb->CreateColorAttachment({ {"Color", m_PrimaryWindow->GetWidth(), m_PrimaryWindow->GetHeight(), GL_TEXTURE_2D, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE } });
		fb->CreateDepthAttachment({ {"Depth", m_PrimaryWindow->GetWidth(), m_PrimaryWindow->GetHeight(), GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE } });
	}

	void Application::Run()
	{
		while(m_Running)
		{
			//input update must be called before window polls for inputs
			m_InputHandler->Update(m_PrimaryWindow->GetDeltaTime());
			m_PrimaryWindow->StartRender();

			for(auto& layer : *m_LayerStack)
			{
				layer->Update(static_cast<float>(m_PrimaryWindow->GetDeltaTime()));
			}

			fb->Bind();
			vao->Bind();
			sp->Bind();
			sp->UploadUniform("model", ShaderDataType::Mat4, glm::value_ptr(t1->m_ModelToWorld));
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			sp->UploadUniform("model", ShaderDataType::Mat4, glm::value_ptr(t2->m_ModelToWorld));
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			sp->UploadUniform("model", ShaderDataType::Mat4, glm::value_ptr(t3->m_ModelToWorld));
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			sp->UploadUniform("model", ShaderDataType::Mat4, glm::value_ptr(t4->m_ModelToWorld));
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			sp->UploadUniform("model", ShaderDataType::Mat4, glm::value_ptr(t5->m_ModelToWorld));
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			sp->Unbind();
			vao->Unbind();

			glNamedFramebufferReadBuffer(fb->GetRendererId(), GL_COLOR_ATTACHMENT0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(
				0, 0, fb->GetColorAttachment(0).m_Specs.m_Width, fb->GetColorAttachment(0).m_Specs.m_Height, // Source rectangle
				0, 0, m_PrimaryWindow->GetBufferWidth(), m_PrimaryWindow->GetBufferHeight(), // Destination rectangle
				GL_COLOR_BUFFER_BIT, // Bitmask of buffers to copy
				GL_NEAREST // Filtering method
			);

			m_PrimaryWindow->EndRender();
		}
	}

	void Application::Shutdown()
	{
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
