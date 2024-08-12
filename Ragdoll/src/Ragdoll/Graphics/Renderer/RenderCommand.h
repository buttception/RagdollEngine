/*!
\file		RenderCommand.h
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
#include "Ragdoll/Math/RagdollMath.h"

namespace ragdoll
{
	enum class RenderCommandType
	{
        SetState,

        ClearAll,
		ClearColor,
		ClearDepth,
		ClearStencil,

		DrawVAO,
		DrawElements,
		DrawArrays,

		BindTexture,
		UnbindTexture,
        BindFramebuffer,
        UnbindFramebuffer,

		SetViewport,
		SetBlendFunction,
		SetDepthFunction,
		SetCullFace,
	};

	union RenderCommand
	{
        struct SetState
        {
	        //will set the pass according to the render state
        } setState;

        struct ClearAll
        {
            //calls glclear on all flags
        } clearAll;

        struct ClearColor
        {
            glm::vec4 m_Color;
        } clearColor;

        struct ClearDepth
        {
            GLfloat m_Depth;
        } clearDepth;

        struct ClearStencil
        {
            GLint m_Stencil;
        } clearStencil;

        struct DrawMeshes
        {
            const char* m_Buffer; //buffer containing all the meshes needed to be drawn
        } drawMeshes;

        struct DrawElements
        {
            GLenum m_Mode;
            GLsizei m_Count;
            GLenum m_Type;
            const void* m_Indices;
        } drawElements;

        struct DrawArrays
        {
            GLenum m_Mode;
            GLint m_First;
            GLsizei m_Count;
        } drawArrays;

        struct BindTexture
        {
            GLenum m_Target;
            GLuint m_Texture;
        } bindTexture;

        struct UnbindTexture
        {
            GLenum m_Target;
        } unbindTexture;

        struct BindFramebuffer
        {
            GLenum m_Target;
            GLuint m_Framebuffer;
        } bindFramebuffer;

        struct UnbindFramebuffer
        {
	        GLenum m_Target;
        } unbindFramebuffer;

        struct SetViewport
        {
            GLint m_X, m_Y;
            GLsizei m_Width, m_Height;
        } setViewport;

        struct SetBlendFunction
        {
            GLenum m_SrcFactor;
            GLenum m_DstFactor;
        } setBlendFunction;

        struct SetDepthFunction
        {
            GLenum m_DepthFunc;
        } setDepthFunction;

        struct SetCullFace
        {
            GLenum m_Face;
        } setCullFace;
	};
}
