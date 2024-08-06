/*!
\file		Window.h
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
#pragma once

#include "Ragdoll/Math/Vector2.h"
#include "Ragdoll/Math/Vector3.h"
#include "Ragdoll/Math/IVector2.h"
#include "Ragdoll/Event/Event.h"

struct GLFWwindow;
struct GLFWmonitor;
struct GLFWvidmode;

namespace Ragdoll
{
	class Window
	{
	public:
		struct WindowProperties
		{
			std::string m_Title{ "Ragdoll Engine" };
			int m_Width{ 800 };
			int m_Height{ 600 };
			IVector2 m_Position{};
			int m_NumSamplesMSAA{ 0 };
			Vector3 m_BackgroundColor{};
			float m_Opacity{ 1.f };
			bool m_Resizable{ true };
			bool m_Visible{ true };
			bool m_Focused{ true };
			bool m_Fullscreen{ true };
			bool m_Decorated{ true };
			bool m_Topmost{ false };
			bool m_FocusOnShow{ true };

			//From here is personal preference
			bool m_DisplayDetailsInTitle{ true };
			bool m_DisplayFpsInTitle{ true };
			bool m_DisplayFrameCountInTitle{ true };
			bool m_DisplayFrameTimeInTitle{ true };
		};

		Window();
		Window(const WindowProperties& properties);

		bool Init();
		void StartRender();
		void EndRender();
		void Close();
		void Shutdown();

		void SetEventCallback(const EventCallbackFn& fn);

	private:
		WindowProperties m_Properties{};

		GLFWwindow* m_GlfwWindow{};
		EventCallbackFn m_Callback{ nullptr };
		inline static GLFWmonitor* m_PrimaryMonitor{};
		inline static GLFWvidmode* m_PrimaryMonitorInfo{};
		int m_BufferWidth{}, m_BufferHeight{};

		bool m_Initialized{ false };

		unsigned long m_Frame{};
		int m_Fps{};
		int m_FpsCounter{};
		std::chrono::time_point<std::chrono::steady_clock> m_LastFrameTime{ std::chrono::steady_clock::now() };
		double m_Timer{};
		double m_DeltaTime{};
	};
}
