/*!
\file		ShaderTypeEnums.cpp
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

#include "ShaderTypeEnums.h"

#include "Ragdoll/Core/Core.h"
#include "Ragdoll/Core/Logger.h"

namespace ragdoll
{
	namespace ShaderUtils
	{
		const char* ShaderUtils::ShaderTypeToString(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::None:                  return "None";
			case ShaderType::Vertex:          return "Vertex";
			case ShaderType::Fragment:        return "Fragment";
			case ShaderType::Geometry:        return "Geometry";
			case ShaderType::TessellationControl: return "Tessellation Control";
			case ShaderType::TessellationEvaluation: return "Tessellation Evaluation";
			case ShaderType::Compute:         return "Compute";
			default: RD_ASSERT(true, "Invalid shader type chosen."); return "Invalid";
			}
		}

		const char* ShaderUtils::ShaderDataTypeToString(ShaderDataType type)
		{
			switch (type)
			{
			case ShaderDataType::None:              return "None";
			case ShaderDataType::Float:             return "Float";
			case ShaderDataType::Float2:            return "Float2";
			case ShaderDataType::Float3:            return "Float3";
			case ShaderDataType::Float4:            return "Float4";
			case ShaderDataType::Int:               return "Int";
			case ShaderDataType::Int2:              return "Int2";
			case ShaderDataType::Int3:              return "Int3";
			case ShaderDataType::Int4:              return "Int4";
			case ShaderDataType::UInt:              return "UInt";
			case ShaderDataType::UInt2:             return "UInt2";
			case ShaderDataType::UInt3:             return "UInt3";
			case ShaderDataType::UInt4:             return "UInt4";
			case ShaderDataType::Bool:              return "Bool";
			case ShaderDataType::Mat2:              return "Mat2";
			case ShaderDataType::Mat3:              return "Mat3";
			case ShaderDataType::Mat4:              return "Mat4";
			case ShaderDataType::FloatArr:          return "FloatArr";
			case ShaderDataType::IntArr:            return "IntArr";
			case ShaderDataType::UIntArr:           return "UIntArr";
			case ShaderDataType::BoolArr:           return "BoolArr";
			case ShaderDataType::Sampler1D:         return "Sampler1D";
			case ShaderDataType::Sampler2D:         return "Sampler2D";
			case ShaderDataType::Sampler3D:         return "Sampler3D";
			case ShaderDataType::SamplerCube:       return "SamplerCube";
			case ShaderDataType::Sampler2DArray:    return "Sampler2DArray";
			case ShaderDataType::Sampler1DShadow:   return "Sampler1DShadow";
			case ShaderDataType::Sampler2DShadow:   return "Sampler2DShadow";
			case ShaderDataType::Image1D:           return "Image1D";
			case ShaderDataType::Image2D:           return "Image2D";
			case ShaderDataType::Image3D:           return "Image3D";
			case ShaderDataType::ImageCube:         return "ImageCube";
			case ShaderDataType::Image2DArray:      return "Image2DArray";
			case ShaderDataType::AtomicUInt:        return "AtomicUInt";
			case ShaderDataType::DATA_TYPE_COUNT:   return "DataTypeCount";
			default: RD_ASSERT(true, "Unknown shader data type chosen."); return "Invalid";
			}
		}

		uint32_t ShaderUtils::ShaderDataTypeSize(const ShaderDataType& type)
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

		GLenum ShaderUtils::ShaderDataTypeToOpenGLType(ShaderDataType type)
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

		GLenum ShaderUtils::ShaderTypeToOpenGLType(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::None: RD_ASSERT(true, "ShaderType::None is not supported."); return 0;
			case ShaderType::Vertex:          return GL_VERTEX_SHADER;
			case ShaderType::Fragment:        return GL_FRAGMENT_SHADER;
			case ShaderType::Geometry:        return GL_GEOMETRY_SHADER;
			case ShaderType::TessellationControl: return GL_TESS_CONTROL_SHADER;
			case ShaderType::TessellationEvaluation: return GL_TESS_EVALUATION_SHADER;
			case ShaderType::Compute:         return GL_COMPUTE_SHADER;
			default: RD_ASSERT(true, "Invalid shader type chosen."); return 0;
			}
		}

		ShaderDataType ShaderUtils::OpenGLTypeToShaderType(GLenum type)
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
