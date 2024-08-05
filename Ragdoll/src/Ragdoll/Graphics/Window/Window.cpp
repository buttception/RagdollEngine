/*!
\file		Window.cpp
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

#include "Window.h"

#include "GLFW/glfw3.h"

#include "Ragdoll/Core/Core.h"
#include "Ragdoll/Core/Logger.h"

namespace Ragdoll
{
	Window::Window()
	{
	}

	Window::Window(const WindowProperties& properties)
	{
	}

	bool Window::Init()
	{
		m_GlfwWindow = glfwCreateWindow(m_Properties.m_Width, m_Properties.m_Height, m_Properties.m_Title.c_str(), nullptr, nullptr);
		RD_ASSERT(m_GlfwWindow == nullptr, "Window failed to initialize");

		return true;
	}

	void Window::StartRender()
	{
		glfwPollEvents();
	}

	void Window::EndRender()
	{
		glfwSwapBuffers(m_GlfwWindow);
	}

	void Window::Close()
	{
		glfwSetWindowShouldClose(m_GlfwWindow, GLFW_TRUE);
		glfwPollEvents();
	}

	void Window::Shutdown()
	{
		if(m_Initialized)
		{
			glfwDestroyWindow(m_GlfwWindow);
			glfwTerminate();
		}
		m_Initialized = false;
		m_GlfwWindow = nullptr;
	}
}
