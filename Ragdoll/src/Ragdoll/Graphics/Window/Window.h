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

#include "Ragdoll/Math/RagdollMath.h"
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
			iVec2 m_Position{};
			int m_NumSamplesMSAA{ 0 };
			Vec3 m_BackgroundColor{};
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

		// Getters
		const std::string& getTitle() const { return m_Properties.m_Title; }
		int getWidth() const { return m_Properties.m_Width; }
		int getHeight() const { return m_Properties.m_Height; }
		const iVec2& getPosition() const { return m_Properties.m_Position; }
		int getNumSamplesMSAA() const { return m_Properties.m_NumSamplesMSAA; }
		const Vec3& getBackgroundColor() const { return m_Properties.m_BackgroundColor; }
		float getOpacity() const { return m_Properties.m_Opacity; }
		bool isResizable() const { return m_Properties.m_Resizable; }
		bool isVisible() const { return m_Properties.m_Visible; }
		bool isFocused() const { return m_Properties.m_Focused; }
		bool isFullscreen() const { return m_Properties.m_Fullscreen; }
		bool isDecorated() const { return m_Properties.m_Decorated; }
		bool isTopmost() const { return m_Properties.m_Topmost; }
		bool isFocusOnShow() const { return m_Properties.m_FocusOnShow; }

		// Personal preference getters
		bool isDisplayDetailsInTitle() const { return m_Properties.m_DisplayDetailsInTitle; }
		bool isDisplayFpsInTitle() const { return m_Properties.m_DisplayFpsInTitle; }
		bool isDisplayFrameCountInTitle() const { return m_Properties.m_DisplayFrameCountInTitle; }
		bool isDisplayFrameTimeInTitle() const { return m_Properties.m_DisplayFrameTimeInTitle; }

		// Getters for static members
		static GLFWmonitor* getPrimaryMonitor() { return m_PrimaryMonitor; }
		static GLFWvidmode* getPrimaryMonitorInfo() { return m_PrimaryMonitorInfo; }

		// Getters for non-static members
		int getBufferWidth() const { return m_BufferWidth; }
		int getBufferHeight() const { return m_BufferHeight; }

		bool isInitialized() const { return m_Initialized; }

		unsigned long getFrame() const { return m_Frame; }
		int getFps() const { return m_Fps; }
		int getFpsCounter() const { return m_FpsCounter; }
		std::chrono::time_point<std::chrono::steady_clock> getLastFrameTime() const { return m_LastFrameTime; }
		double getTimer() const { return m_Timer; }
		double getDeltaTime() const { return m_DeltaTime; }

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
