/*!
\file		VertexArray.cpp
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

#include "VertexArray.h"

#include "IndexBuffer.h"
#include "VertexBuffer.h"

namespace ragdoll
{
	VertexArray::VertexArray()
	{
		glCreateVertexArrays(1, &m_RendererId);
	}

	VertexArray::~VertexArray()
	{
		glDeleteVertexArrays(1, &m_RendererId);
		m_RendererId = 0;
	}

	void VertexArray::Bind() const
	{
		glBindVertexArray(m_RendererId);
	}

	void VertexArray::Unbind() const
	{
		glBindVertexArray(0);
	}

	void VertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
	{
		//bind the vbo first
		glVertexArrayVertexBuffer(m_RendererId, m_VertexBuffers.size(), vertexBuffer->GetRendererId(), 0, vertexBuffer->GetLayout().m_Stride);

		const auto& layout = vertexBuffer->GetLayout();
		uint32_t index = 0;
		for(auto& element : layout)
		{
			GLenum type = ShaderUtils::ShaderDataTypeToOpenGLType(element.m_Type);
			GLuint componentCount = element.GetComponentCount();
			GLboolean normalized = element.m_Normalized;

			//set up the attribs format
			glEnableVertexArrayAttrib(m_RendererId, index);
			if (type == GL_FLOAT)
			{
				glVertexArrayAttribFormat(m_RendererId, index, componentCount, type, normalized, element.m_Offset);
			}
			else if (type == GL_INT || type == GL_UNSIGNED_INT)
			{
				glVertexArrayAttribIFormat(m_RendererId, index, componentCount, type, element.m_Offset);
			}
			else if (type == GL_DOUBLE)
			{
				glVertexArrayAttribLFormat(m_RendererId, index, componentCount, type, element.m_Offset);
			}
			//bind the attrib to the vbo
			glVertexArrayAttribBinding(m_RendererId, index, m_VertexBuffers.size());
			index++;
		}
		m_VertexBuffers.emplace_back(vertexBuffer);
	}

	void VertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
	{
		glVertexArrayElementBuffer(m_RendererId, indexBuffer->GetRendererId());
		m_IndexBuffer = indexBuffer;
	}
}
