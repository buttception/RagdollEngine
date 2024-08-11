/*!
\file		GLFWContext.cpp
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

#include "GLFWContext.h"

#include "Ragdoll/Core/Logger.h"
#include "GLFW/glfw3.h"
#include "Ragdoll/Core/Core.h"

namespace ragdoll
{
	bool GLFWContext::Init()
	{
		if (s_GLFWInitialized)
		{
			RD_CORE_WARN("GLFW already initialized, tried to initialize again");
			return true;
		}

		int success = glfwInit();
		RD_ASSERT(!success, "Initializing GLFW failed.");
		if(!success)
		{
			exit(EXIT_FAILURE);
		}
		glfwSetErrorCallback(GLFWErrorCallback);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		s_GLFWInitialized = true;
		RD_CORE_INFO("GLFW initialized successfully.");

		return s_GLFWInitialized;
	}

	void GLFWContext::Shutdown()
	{
		if(s_GLFWInitialized)
		{
			glfwTerminate();
			s_GLFWInitialized = false;
			RD_CORE_INFO("GLFW terminated successfully.");
		}
		else
		{
			RD_CORE_WARN("GLFW already terminated, tried to terminate again.");
		}
	}

	void GLFWContext::GLFWErrorCallback(int _error, const char* description)
	{
		RD_CORE_ERROR("GLFW Error ({0}): {1}", _error, description);
	}
}
