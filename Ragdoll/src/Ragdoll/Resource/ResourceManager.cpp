/*!
\file		ResourceManager.cpp
\date		12/08/2024

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

#include "ResourceManager.h"

#include "Resource.h"
#include "Ragdoll/Graphics/Window/Window.h"

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

namespace ragdoll
{
	void ResourceManager::Init(std::shared_ptr<Window> window)
	{
		m_PrimaryWindow = window;
	}
}
