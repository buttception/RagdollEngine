/*!
\file		RenderState.h
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
#pragma once

#include "glad/glad.h"

namespace ragdoll
{
	//state that is used to prepare a render pass
	struct RenderState
	{
		bool m_CullFaceEnabled{ false };		// Face culling enabled/disabled
		GLenum m_CullFaceMode{ GL_BACK };		// Face culling mode (e.g., GL_BACK, GL_FRONT)
		GLenum m_FrontFace{ GL_CCW };			// Front face winding order (e.g., GL_CCW, GL_CW)

		bool m_DepthTestEnabled{ false };		// Depth testing enabled/disabled
		GLenum m_DepthFunc{ GL_LESS };			// Depth function (e.g., GL_LESS, GL_EQUAL)

		bool m_BlendEnabled{ false };			// Blending enabled/disabled
		GLenum m_BlendSrcFactor{ GL_ONE };		// Source blend factor
		GLenum m_BlendDstFactor{ GL_ZERO };		// Destination blend factor
		GLenum m_BlendEquation{ GL_FUNC_ADD };	// Blend equation

		bool m_ScissorTestEnabled{ false };		// Scissor testing enabled/disabled

		bool m_WireframeEnabled{ false };

		void Execute();
	};
}
