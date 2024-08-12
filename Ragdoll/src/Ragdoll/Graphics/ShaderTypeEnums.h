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

#include "Ragdoll/Core/Core.h"
#include "Ragdoll/Core/Logger.h"

namespace ragdoll
{
	// Types of shader
	enum class ShaderType
	{
		None = 0,
		VertexShader,
		FragmentShader,
		GeometryShader,
		TessellationControlShader,
		TessellationEvaluationShader,
		ComputeShader,

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
		static uint32_t ShaderDataTypeSize(const ShaderDataType& type)
		{
			switch (type)
			{
			case ShaderDataType::Float:         return 4;
			case ShaderDataType::Float2:        return 8;
			case ShaderDataType::Float3:        return 12;
			case ShaderDataType::Float4:        return 16;
			case ShaderDataType::Mat2:          return 16;  // 2x2 matrix
			case ShaderDataType::Mat3:          return 36;  // 3x3 matrix
			case ShaderDataType::Mat4:          return 64;  // 4x4 matrix
			case ShaderDataType::Int:           return 4;
			case ShaderDataType::Int2:          return 8;
			case ShaderDataType::Int3:          return 12;
			case ShaderDataType::Int4:          return 16;
			case ShaderDataType::UInt:          return 4;
			case ShaderDataType::UInt2:         return 8;
			case ShaderDataType::UInt3:         return 12;
			case ShaderDataType::UInt4:         return 16;
			case ShaderDataType::Bool:          return 1;
			case ShaderDataType::Sampler1D:
			case ShaderDataType::Sampler2D:
			case ShaderDataType::Sampler3D:
			case ShaderDataType::SamplerCube:
			case ShaderDataType::Sampler1DShadow:
			case ShaderDataType::Sampler2DShadow:
			case ShaderDataType::Image1D:
			case ShaderDataType::Image2D:
			case ShaderDataType::Image3D:
			case ShaderDataType::ImageCube:
			case ShaderDataType::AtomicUInt:    return 4;  // Typically an int or uint is used
			default: RD_ASSERT(true, "Invalid ShaderDataType"); return 0;
			}
		}

		static GLenum ShaderDataTypeToOpenGLType(ShaderDataType type)
		{
			switch (type)
			{
			case ShaderDataType::Float:
			case ShaderDataType::Float2:
			case ShaderDataType::Float3:
			case ShaderDataType::Float4:
			case ShaderDataType::Mat2:
			case ShaderDataType::Mat3:
			case ShaderDataType::Mat4:
			case ShaderDataType::FloatArr:      return GL_FLOAT;

			case ShaderDataType::Int:
			case ShaderDataType::Int2:
			case ShaderDataType::Int3:
			case ShaderDataType::Int4:
			case ShaderDataType::IntArr:        return GL_INT;

			case ShaderDataType::UInt:
			case ShaderDataType::UInt2:
			case ShaderDataType::UInt3:
			case ShaderDataType::UInt4:
			case ShaderDataType::UIntArr:
			case ShaderDataType::AtomicUInt:    return GL_UNSIGNED_INT;

			case ShaderDataType::Bool:
			case ShaderDataType::BoolArr:       return GL_BOOL;

			case ShaderDataType::Sampler1D:
			case ShaderDataType::Sampler2D:
			case ShaderDataType::Sampler3D:
			case ShaderDataType::SamplerCube:
			case ShaderDataType::Sampler2DArray:
			case ShaderDataType::Sampler1DShadow:
			case ShaderDataType::Sampler2DShadow:
			case ShaderDataType::Image1D:
			case ShaderDataType::Image2D:
			case ShaderDataType::Image3D:
			case ShaderDataType::ImageCube:
			case ShaderDataType::Image2DArray:  return GL_SAMPLER_2D;
			default: RD_ASSERT(true, "Invalid ShaderDataType"); return 0;
			}
		}

		static GLenum ShaderTypeToOpenGLType(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::None: RD_ASSERT(true, "ShaderType::None is not supported."); return 0;
			case ShaderType::VertexShader:          return GL_VERTEX_SHADER;
			case ShaderType::FragmentShader:        return GL_FRAGMENT_SHADER;
			case ShaderType::GeometryShader:        return GL_GEOMETRY_SHADER;
			case ShaderType::TessellationControlShader: return GL_TESS_CONTROL_SHADER;
			case ShaderType::TessellationEvaluationShader: return GL_TESS_EVALUATION_SHADER;
			case ShaderType::ComputeShader:         return GL_COMPUTE_SHADER;
			default: RD_ASSERT(true, "Invalid shader type chosen."); return 0;
			}
		}

		static ShaderDataType OpenGLTypeToShaderType(GLenum type)
		{
			switch (type)
			{
			case GL_FLOAT:                  return ShaderDataType::Float;
			case GL_FLOAT_VEC2:             return ShaderDataType::Float2;
			case GL_FLOAT_VEC3:             return ShaderDataType::Float3;
			case GL_FLOAT_VEC4:             return ShaderDataType::Float4;
			case GL_FLOAT_MAT2:             return ShaderDataType::Mat2;
			case GL_FLOAT_MAT3:             return ShaderDataType::Mat3;
			case GL_FLOAT_MAT4:             return ShaderDataType::Mat4;
			case GL_INT:                    return ShaderDataType::Int;
			case GL_INT_VEC2:               return ShaderDataType::Int2;
			case GL_INT_VEC3:               return ShaderDataType::Int3;
			case GL_INT_VEC4:               return ShaderDataType::Int4;
			case GL_UNSIGNED_INT:           return ShaderDataType::UInt;
			case GL_UNSIGNED_INT_VEC2:      return ShaderDataType::UInt2;
			case GL_UNSIGNED_INT_VEC3:      return ShaderDataType::UInt3;
			case GL_UNSIGNED_INT_VEC4:      return ShaderDataType::UInt4;
			case GL_BOOL:                   return ShaderDataType::Bool;
			case GL_SAMPLER_1D:             return ShaderDataType::Sampler1D;
			case GL_SAMPLER_2D:             return ShaderDataType::Sampler2D;
			case GL_SAMPLER_3D:             return ShaderDataType::Sampler3D;
			case GL_SAMPLER_CUBE:           return ShaderDataType::SamplerCube;
			case GL_SAMPLER_2D_ARRAY:       return ShaderDataType::Sampler2DArray;
			case GL_SAMPLER_1D_SHADOW:      return ShaderDataType::Sampler1DShadow;
			case GL_SAMPLER_2D_SHADOW:      return ShaderDataType::Sampler2DShadow;
			case GL_IMAGE_1D:               return ShaderDataType::Image1D;
			case GL_IMAGE_2D:               return ShaderDataType::Image2D;
			case GL_IMAGE_3D:               return ShaderDataType::Image3D;
			case GL_IMAGE_CUBE:             return ShaderDataType::ImageCube;
			case GL_IMAGE_2D_ARRAY:         return ShaderDataType::Image2DArray;
			case GL_UNSIGNED_INT_ATOMIC_COUNTER: return ShaderDataType::AtomicUInt;
			case GL_UNSIGNED_INT_IMAGE_1D:  return ShaderDataType::Image1D;
			case GL_UNSIGNED_INT_IMAGE_2D:  return ShaderDataType::Image2D;
			case GL_UNSIGNED_INT_IMAGE_3D:  return ShaderDataType::Image3D;
			case GL_UNSIGNED_INT_IMAGE_CUBE: return ShaderDataType::ImageCube;
			case GL_UNSIGNED_INT_IMAGE_2D_ARRAY: return ShaderDataType::Image2DArray;
			default:
				return ShaderDataType::None;
			}
		}
	}
}
