/*!
\file		ImguiLayer.cpp
\date		17/08/2024

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

#include "GuiLayer.h"


namespace ragdoll
{
	GuiLayer::GuiLayer(std::shared_ptr<EntityManager> reg) : Layer{ "GuiLayer" }, m_EntityManager { reg }
	{

	}

	void GuiLayer::Init()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		m_ImGuiIO = &ImGui::GetIO(); (void)m_ImGuiIO;
		m_ImGuiIO->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls
		m_ImGuiIO->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls
		m_ImGuiIO->ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// Enable Docking
		//m_ImGuiIO->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// Enable Multi-Viewport / Platform Windows

		// ImGui style
		ImGuiStyle& style = ImGui::GetStyle();
		if (m_ImGuiIO->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.f;
			style.Colors[ImGuiCol_WindowBg].w = 1.f;
		}
	}

	void GuiLayer::Update(float _dt)
	{
	}

	void GuiLayer::Shutdown()
	{
	}
}
