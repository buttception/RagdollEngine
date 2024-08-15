/*!
\file		RenderPass.cpp
\date		11/08/2024

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

#include "RenderPass.h"

#include "RenderData.h"
#include "Ragdoll/Resource/ResourceManager.h"
#include "Ragdoll/Graphics/Shader/ShaderProgram.h"
#include "Ragdoll/Math/RagdollMath.h"
#include "Ragdoll/Graphics/Containers/Framebuffer.h"
#include "Ragdoll/Graphics/Containers/VertexArray.h"
#include "Ragdoll/Graphics/Window/Window.h"
#include "Ragdoll/Graphics/Transform/Transform.h"
#include "Ragdoll/Entity/EntityManager.h"

namespace ragdoll
{
	void TestRenderPass::Execute()
	{
		m_RenderState.Execute();
		auto& sp = m_ResourceManager->GetShaderProgram("test shader program");
		auto& fb = m_ResourceManager->GetFramebuffer("test");
		auto& vao = m_ResourceManager->GetPrimitiveMesh("cube");
		fb.Bind();
		vao.Bind();
		sp.Bind();
		//draw all the render data
		for(const auto& it : m_ResourceManager->GetRenderData("test"))
		{
			const auto& trans = *m_EntityManager->GetComponent<Transform>(it.m_DrawMesh.m_EntityGuid);
			//get the transform
			sp.UploadUniform("model", ShaderDataType::Mat4, glm::value_ptr(trans.m_ModelToWorld));
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
		}
		sp.Unbind();
		vao.Unbind();

		glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.GetRendererId());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(
			0, 0, fb.GetColorAttachment(0).m_Specs.m_Width, fb.GetColorAttachment(0).m_Specs.m_Height, // Source rectangle
			0, 0, m_PrimaryWindow->GetBufferWidth(), m_PrimaryWindow->GetBufferHeight(), // Destination rectangle
			GL_COLOR_BUFFER_BIT, // Bitmask of buffers to copy
			GL_NEAREST // Filtering method
		);
		fb.Unbind();
	}
}
