/*!
\file		ShaderTypeEnums.h
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

#include <glad/glad.h>

namespace ragdoll
{
	// Types of shader
	enum class ShaderType
	{
		None = 0,
		Vertex,
		Fragment,
		Geometry,
		TessellationControl,
		TessellationEvaluation,
		Compute,

		SHADER_COUNT
	};

	enum class ShaderDataType
	{
		None = 0,
		Float,
		Float2,
		Float3,
		Float4,
		Int,
		Int2,
		Int3,
		Int4,
		UInt,
		UInt2,
		UInt3,
		UInt4,
		Bool,
		Mat2,
		Mat3,
		Mat4,
		FloatArr,
		IntArr,
		UIntArr,
		BoolArr,
		Sampler1D,
		Sampler2D,
		Sampler3D,
		SamplerCube,
		Sampler2DArray,
		Sampler1DShadow,
		Sampler2DShadow,
		Image1D,
		Image2D,
		Image3D,
		ImageCube,
		Image2DArray,
		AtomicUInt,

		DATA_TYPE_COUNT
	};

	namespace ShaderUtils
	{
		const char* ShaderTypeToString(ShaderType type);
		const char* ShaderDataTypeToString(ShaderDataType type);
		uint32_t ShaderDataTypeSize(const ShaderDataType& type);
		GLenum ShaderDataTypeToOpenGLType(ShaderDataType type);
		GLenum ShaderTypeToOpenGLType(ShaderType type);
		ShaderDataType OpenGLTypeToShaderType(GLenum type);
	}
}
