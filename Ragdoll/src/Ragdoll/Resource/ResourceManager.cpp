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
#include "glad/glad.h"
#include "Ragdoll/Graphics/Containers/Framebuffer.h"
#include "Ragdoll/Graphics/Containers/IndexBuffer.h"
#include "Ragdoll/Graphics/Containers/VertexArray.h"
#include "Ragdoll/Graphics/Containers/VertexBuffer.h"
#include "Ragdoll/Graphics/Renderer/RenderData.h"
#include "Ragdoll/Graphics/Shader/ShaderProgram.h"
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
		//create all the resources
		//framebuffers
		auto fb = std::make_shared<Framebuffer>("test");
		fb->CreateColorAttachment({ {"Color", m_PrimaryWindow->GetWidth(), m_PrimaryWindow->GetHeight(), GL_TEXTURE_2D, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE } });
		fb->CreateDepthAttachment({ {"Depth", m_PrimaryWindow->GetWidth(), m_PrimaryWindow->GetHeight(), GL_TEXTURE_2D, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE } });
		m_Framebuffers.insert({ fb->GetName(), fb });

		//shader programs
		std::shared_ptr<Shader> vs = std::make_shared<Shader>("test vertex shader", vertexShaderSource, ShaderType::Vertex);
		std::shared_ptr<Shader> fs = std::make_shared<Shader>("test fragment shader", fragmentShaderSource, ShaderType::Fragment);
		auto sp = std::make_shared<ShaderProgram>("test shader program");
		sp->AttachShaders({ vs, fs });
		sp->PrintAttributes();
		sp->PrintUniforms();
		m_ShaderPrograms.insert({ sp->m_Name, sp });

		//meshes
		auto vao = std::make_shared<VertexArray>();
		std::shared_ptr<VertexBuffer> vbo = std::make_shared<VertexBuffer>(vertices, sizeof(vertices), GL_STATIC_DRAW);
		vbo->SetLayout({
			{ ShaderDataType::Float3, "aPos" },
			{ ShaderDataType::Float3, "aColor" }
			});
		std::shared_ptr<IndexBuffer> ibo = std::make_shared<IndexBuffer>(indices, sizeof(indices));
		vao->AddVertexBuffer(vbo);
		vao->SetIndexBuffer(ibo);
		m_PrimitiveMeshes.insert({ "cube", vao });
	}

	void ResourceManager::AddRenderData(const char* datasetName, std::vector<RenderData>&& data)
	{
		m_RenderData[datasetName] = std::move(data);
	}

	std::vector<RenderData>& ResourceManager::GetRenderData(const char* datasetName)
	{
		RD_ASSERT(!m_RenderData.contains(datasetName), "Render data set {} not found", datasetName);
		return m_RenderData[datasetName];
	}

	Framebuffer& ResourceManager::GetFramebuffer(const char* name)
	{
		RD_ASSERT(!m_Framebuffers.contains(name), "Framebuffer {} not found", name);
		return *m_Framebuffers[name];
	}

	ShaderProgram& ResourceManager::GetShaderProgram(const char* name)
	{
		RD_ASSERT(!m_ShaderPrograms.contains(name), "Shader program {} not found", name);
		return *m_ShaderPrograms[name];
	}

	VertexArray& ResourceManager::GetPrimitiveMesh(const char* name)
	{
		RD_ASSERT(!m_PrimitiveMeshes.contains(name), "Primitive mesh {} not found", name);
		return *m_PrimitiveMeshes[name];
	}
}
