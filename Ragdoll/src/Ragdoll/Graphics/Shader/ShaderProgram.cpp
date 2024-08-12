/*!
\file		ShaderProgram.cpp
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

#include "ShaderProgram.h"

namespace ragdoll
{
	Shader::Shader(const char* name) :
		m_Name{ name },
		m_Type{ ShaderType::None }
	{
		//shouldn't create a shader here because if it is not compiled it's not used
	}

	Shader::Shader(const char* name, const char* source, ShaderType type) : Shader(name)
	{
		Compile(source, type);
	}

	Shader::~Shader()
	{
		if(m_RendererId > 0)
			glDeleteShader(m_RendererId);
		m_RendererId = 0;
	}

	bool Shader::Compile(const char* source, ShaderType type)
	{
		GLuint shaderId = glCreateShader(ShaderUtils::ShaderTypeToOpenGLType(type));
		RD_ASSERT(!shaderId, "Failed to create shader");

		//compile the shader
		GLchar const* shaderCode[]{ source };
		glShaderSource(shaderId, 1, shaderCode, NULL);
		glCompileShader(shaderId);

		//check if the shader compiled successfully
		bool status = CheckCompilationStatus(shaderId, type);
		if (status)
		{
			m_RendererId = shaderId;
			m_Type = type;
			return true;
		}
		glDeleteShader(shaderId);
		return false;
	}

	bool Shader::CheckCompilationStatus(GLuint shaderId, ShaderType type) const
	{
		//check compilation status
		GLint res;
		glGetShaderiv(shaderId, GL_COMPILE_STATUS, &res);
		if (res == GL_FALSE)
		{
			GLint logLength;
			glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
			auto typeName = ShaderUtils::ShaderTypeToString(type);
			if (logLength > 0)
			{
				GLchar* log = new GLchar[logLength];
				GLsizei writtenLogLength;
				glGetShaderInfoLog(shaderId, logLength, &writtenLogLength, log);
				RD_ASSERT(true, "Shader compilation in [{}] of type [{}] failed with:\n{}", m_Name, typeName, log);

				delete[] log;
			}
			else
				RD_ASSERT(true, "Shader compilation in [{}] of type [{}] failed with unknown error", m_Name, typeName);
			return false;
		}
		return true;
	}

	ShaderProgram::ShaderProgram(const char* name) : m_Name{ name }
	{
		m_RendererId = glCreateProgram();
		RD_ASSERT(!m_RendererId, "Failed to create shader program");
	}

	ShaderProgram::~ShaderProgram()
	{
		if(m_RendererId > 0)
		{
			DetachShaders();
			glDeleteProgram(m_RendererId);
		}
	}

	void ShaderProgram::Bind() const
	{
		glUseProgram(m_RendererId);
	}

	void ShaderProgram::Unbind() const
	{
		glUseProgram(0);
	}

	bool ShaderProgram::AttachShaders(std::initializer_list<std::shared_ptr<Shader>> shaders)
	{
		m_Ready = false;
		m_Shaders = shaders;
		for (auto& shader : shaders)
		{
			glAttachShader(m_RendererId, shader->m_RendererId);
		}
		if(Link() && Validate())
		{
			m_Ready = true;
			LoadUniforms();
			return true;
		}
		//clean up if failed
		DetachShaders();
		m_Shaders.clear();
		return false;
	}

	void ShaderProgram::DetachShaders()
	{
		for (const auto& it : m_Shaders)
			glDetachShader(m_RendererId, it->m_RendererId);
	}

	bool ShaderProgram::Link() const
	{
		glLinkProgram(m_RendererId);
		//check link status
		GLint res;
		glGetProgramiv(m_RendererId, GL_LINK_STATUS, &res);
		if (res == GL_FALSE)
		{
			GLint logLength;
			glGetProgramiv(m_RendererId, GL_INFO_LOG_LENGTH, &logLength);
			if (logLength > 0)
			{
				GLchar* log = new GLchar[logLength];
				GLsizei writtenLogLength;
				glGetProgramInfoLog(m_RendererId, logLength, &writtenLogLength, log);
				RD_ASSERT(true, "Shader linking of [{}] failed with: {}", m_Name, log);

				delete[] log;
			}
			else
				RD_ASSERT(true, "Shader linking of [{}] failed with unknown error", m_Name);
			return false;
		}
		return true;
	}

	bool ShaderProgram::Validate() const
	{
		glValidateProgram(m_RendererId);
		//check validation status
		GLint res;
		glGetProgramiv(m_RendererId, GL_VALIDATE_STATUS, &res);
		if (res == GL_FALSE)
		{
			GLint logLength;
			glGetProgramiv(m_RendererId, GL_INFO_LOG_LENGTH, &logLength);
			if (logLength > 0)
			{
				GLchar* log = new GLchar[logLength];
				GLsizei writtenLogLength;
				glGetProgramInfoLog(m_RendererId, logLength, &writtenLogLength, log);
				RD_ASSERT(true, "Shader linking of [{}] failed with: {}", m_Name, log);

				delete[] log;
			}
			else
				RD_ASSERT(true, "Shader linking of [{}] failed with unknown error", m_Name);
			return false;
		}
		return true;
	}

	bool ShaderProgram::UploadUniform(const char* name, ShaderDataType type, const void* data) const
	{
		const GLchar* uniformName{ name };
		const GLint uniformID = glGetUniformLocation(m_RendererId, uniformName);
		if (uniformID >= 0)
		{
			switch (type)
			{
			case ShaderDataType::None:		RD_CORE_ERROR("ShaderDataType::None is not supported."); break;
			case ShaderDataType::Float:		glUniform1f(uniformID, *static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::Float2:	glUniform2fv(uniformID, 1, static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::Float3:	glUniform3fv(uniformID, 1, static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::Float4:	glUniform4fv(uniformID, 1, static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::Mat3:		glUniformMatrix3fv(uniformID, 1, false, static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::Mat4:  	glUniformMatrix4fv(uniformID, 1, false, static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::Int:  		glUniform1i(uniformID, *static_cast<const GLint*>(data)); return true;
			case ShaderDataType::Int2:  	glUniform2iv(uniformID, 1, static_cast<const GLint*>(data)); return true;
			case ShaderDataType::Int3: 		glUniform3iv(uniformID, 1, static_cast<const GLint*>(data)); return true;
			case ShaderDataType::Int4:  	glUniform4iv(uniformID, 1, static_cast<const GLint*>(data)); return true;
			case ShaderDataType::UInt:  	glUniform1ui(uniformID, *static_cast<const GLuint*>(data)); return true;
			case ShaderDataType::UInt2:  	glUniform2uiv(uniformID, 1, static_cast<const GLuint*>(data)); return true;
			case ShaderDataType::UInt3: 	glUniform3uiv(uniformID, 1, static_cast<const GLuint*>(data)); return true;
			case ShaderDataType::UInt4:  	glUniform4uiv(uniformID, 1, static_cast<const GLuint*>(data)); return true;
			case ShaderDataType::Bool:  	glUniform1i(uniformID, *static_cast<const GLint*>(data)); return true;
			case ShaderDataType::Sampler1D:
			case ShaderDataType::Sampler2D:
			case ShaderDataType::Sampler3D:
			case ShaderDataType::SamplerCube:
			case ShaderDataType::Sampler2DArray:
			case ShaderDataType::Sampler1DShadow:
			case ShaderDataType::Sampler2DShadow:	RD_CORE_ERROR("Sampler types are not directly handled by glUniform calls."); break;
			case ShaderDataType::Image1D:
			case ShaderDataType::Image2D:
			case ShaderDataType::Image3D:
			case ShaderDataType::ImageCube:
			case ShaderDataType::Image2DArray:		RD_CORE_ERROR("Image types are not directly handled by glUniform calls."); break;
			case ShaderDataType::AtomicUInt:		RD_CORE_ERROR("AtomicUInt type is not directly handled by glUniform calls."); break;
			default:	RD_CORE_ERROR("Invalid Shader type given.");
			}
		}
		RD_ASSERT(true, "Uniform [{}] does not exist in the current shader.", name);
		return false;
	}

	bool ShaderProgram::UploadUniforms(const char* name, ShaderDataType type, const void* data, uint32_t count) const
	{
		if (count == 0)
		{
			RD_CORE_WARN("Attempted to upload 0 uniforms, please check your implementation");
			return false;
		}
		const GLchar* uniformName{ name };
		const GLint uniformID = glGetUniformLocation(m_RendererId, uniformName);
		if (uniformID >= 0)
		{
			switch (type)
			{
			case ShaderDataType::None:		RD_CORE_ERROR("ShaderDataType::None is not supported."); break;
			case ShaderDataType::FloatArr:	glUniform1fv(uniformID, count, static_cast<const GLfloat*>(data)); return true;
			case ShaderDataType::IntArr:	glUniform1iv(uniformID, count, static_cast<const GLint*>(data)); return true;
			case ShaderDataType::UIntArr:	glUniform1uiv(uniformID, count, static_cast<const GLuint*>(data)); return true;
			case ShaderDataType::BoolArr:	glUniform1iv(uniformID, count, static_cast<const GLint*>(data)); return true;
			default:						RD_CORE_ERROR("Invalid Shader type given.");
			}
		}
		RD_ASSERT(true, "Uniform [{}] does not exist in the current shader.", name);
		return false;
	}

	void ShaderProgram::PrintAttributes()
	{
		RD_CORE_TRACE("[{}] uniforms:", m_Name);
		GLint maxLength, attribsCount;
		glGetProgramiv(m_RendererId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
		glGetProgramiv(m_RendererId, GL_ACTIVE_ATTRIBUTES, &attribsCount);
		if (attribsCount == 0)
		{
			RD_CORE_TRACE("	No attributes in shader program.");
			return;
		}
		GLchar* name = new GLchar[maxLength];
		for (GLint i = 0; i < attribsCount; ++i) {
			GLsizei written;
			GLint size;
			GLenum type;
			glGetActiveAttrib(m_RendererId, i, maxLength, &written, &size, &type, name);
			GLint loc = glGetAttribLocation(m_RendererId, name);
			RD_CORE_TRACE("	{} [{}]: {}", loc, ShaderUtils::ShaderDataTypeToString(ShaderUtils::OpenGLTypeToShaderType(type)), name);
		}
		delete[] name;
	}

	void ShaderProgram::PrintUniforms()
	{
		RD_CORE_TRACE("[{}] uniforms:", m_Name);
		GLint maxLength, uniformsCount;
		glGetProgramiv(m_RendererId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
		glGetProgramiv(m_RendererId, GL_ACTIVE_UNIFORMS, &uniformsCount);
		if (uniformsCount == 0)
		{
			RD_CORE_TRACE("	No uniforms in shader program.");
			return;
		}
		GLchar* name = new GLchar[maxLength];
		for (GLint i = 0; i < uniformsCount; ++i) {
			GLsizei written;
			GLint size;
			GLenum type;
			glGetActiveUniform(m_RendererId, i, maxLength, &written, &size, &type, name);
			GLint loc = glGetUniformLocation(m_RendererId, name);
			RD_CORE_TRACE("	{} [{}]: {}", loc, ShaderUtils::ShaderDataTypeToString(ShaderUtils::OpenGLTypeToShaderType(type)), name);
		}
		delete[] name;
	}

	void ShaderProgram::LoadUniforms()
	{
		GLint maxLength, uniformsCount;
		glGetProgramiv(m_RendererId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
		glGetProgramiv(m_RendererId, GL_ACTIVE_UNIFORMS, &uniformsCount);
		if (uniformsCount == 0)
		{
			return;
		}
		GLchar* name = new GLchar[maxLength];
		for (GLint i = 0; i < uniformsCount; ++i) {
			GLsizei written;
			GLint size;
			GLenum type;
			glGetActiveUniform(m_RendererId, i, maxLength, &written, &size, &type, name);
			m_Uniforms.emplace_back(name, ShaderUtils::OpenGLTypeToShaderType(type));
		}
		delete[] name;
	}
}
