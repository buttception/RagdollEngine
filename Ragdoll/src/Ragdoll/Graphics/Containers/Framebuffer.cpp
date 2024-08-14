/*!
\file		Framebuffer.cpp
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

#include "Framebuffer.h"

namespace ragdoll
{
	Framebuffer::Framebuffer(const char* name) : m_Name{ name }
	{
		glGenFramebuffers(1, &m_RendererId);
	}

	Framebuffer::~Framebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererId);
		//delete all attachments
		for(auto& attachment : m_ColorAttachments)
		{
			glDeleteTextures(1, &attachment.m_RendererId);
		}
		m_ColorAttachments.clear();
		if(m_DepthAttachment.m_RendererId)
		{
			glDeleteTextures(1, &m_DepthAttachment.m_RendererId);
		}
		if(m_StencilAttachment.m_RendererId)
		{
			glDeleteTextures(1, &m_StencilAttachment.m_RendererId);
		}
		if(m_DepthStencilAttachment.m_RendererId)
		{
			glDeleteTextures(1, &m_DepthStencilAttachment.m_RendererId);
		}
		m_DepthAttachment.m_RendererId = m_StencilAttachment.m_RendererId = m_DepthStencilAttachment.m_RendererId = 0;
	}

	bool Framebuffer::CreateColorAttachment(const ColorAttachmentSpecification& specs)
	{
		//check if hit max attachment count
		if(m_ColorAttachments.size() > GL_MAX_COLOR_ATTACHMENTS)
		{
			RD_ASSERT(true, "Too many color attachments in framebuffer {}", m_Name);
		}
		GLuint colorAttachment;
		glGenTextures(1, &colorAttachment);

		if(specs.m_TextureTarget == GL_TEXTURE_2D)
		{
			glBindTexture(GL_TEXTURE_2D, colorAttachment);

			glTexImage2D(GL_TEXTURE_2D, 0, specs.m_InternalFormat, specs.m_Width, specs.m_Height, 0, specs.m_Format, specs.m_Type, nullptr);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, specs.m_MinFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, specs.m_MagFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, specs.m_WrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, specs.m_WrapT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, specs.m_WrapR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, specs.m_Anisotropy);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, specs.m_MipmapBase);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, specs.m_MipmapMax);

			// Mipmaps (if needed)
			if (specs.m_MipmapBase > 0 || specs.m_MipmapMax > 0)
			{
				glGenerateMipmap(GL_TEXTURE_2D);
			}
		}
		else if(specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
		{
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorAttachment);

			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, specs.m_Samples, specs.m_InternalFormat, specs.m_Width, specs.m_Height, specs.m_FixedSampleLocations);
			// Configure texture parameters (some parameters are not applicable to multisample textures)
			// Note: Not all parameters are valid for GL_TEXTURE_2D_MULTISAMPLE
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, specs.m_MinFilter);
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, specs.m_MagFilter);
			// GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R are not applicable for GL_TEXTURE_2D_MULTISAMPLE

			// Note: GL_TEXTURE_MAX_ANISOTROPY_EXT is not applicable for GL_TEXTURE_2D_MULTISAMPLE
			// Note: Mipmaps are not applicable for GL_TEXTURE_2D_MULTISAMPLE
			// glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, specs.m_MipmapBase);
			// glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, specs.m_MipmapMax);
		}
		else if(specs.m_TextureTarget == GL_TEXTURE_CUBE_MAP)
		{
			glBindTexture(GL_TEXTURE_CUBE_MAP, colorAttachment);

			// Initialize each face of the cube map
			for (GLuint face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++)
			{
				glTexImage2D(face, 0, specs.m_InternalFormat, specs.m_Width, specs.m_Height, 0, specs.m_Format, specs.m_Type, nullptr);
			}

			// Configure texture parameters
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, specs.m_MinFilter);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, specs.m_MagFilter);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, specs.m_WrapS);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, specs.m_WrapT);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, specs.m_WrapR);

			// Note: Anisotropy is not applicable to cube maps

			// Mipmaps (if needed) - Optionally generate mipmaps for the cube map
			if (specs.m_MipmapBase > 0 || specs.m_MipmapMax > 0)
			{
				glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
			}
		}
		else
		{
			RD_ASSERT(true, "Invalid texture target for color attachment in framebuffer {}", m_Name);
			glDeleteTextures(1, &colorAttachment);
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererId);
		if (specs.m_TextureTarget == GL_TEXTURE_CUBE_MAP)
			for (GLuint i = 0; i < 6; ++i)
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_ColorAttachments.size(), GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, colorAttachment, 0);
		else
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_ColorAttachments.size(), specs.m_TextureTarget, colorAttachment, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			RD_CORE_ERROR("Framebuffer {} is not completed", specs.m_Name);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_ColorAttachments.size(), specs.m_TextureTarget, 0, 0);
			glDeleteTextures(1, &colorAttachment);
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_ColorAttachments.push_back({ colorAttachment , specs });
		return true;
	}

	bool Framebuffer::CreateDepthAttachment(const DepthAttachmentSpecification& specs)
	{
		if(m_DepthAttachment.m_RendererId)
		{
			RD_CORE_WARN("Depth attachment already exists in framebuffer {}", m_Name);
			return false;
		}
		GLuint depthAttachment;
		glGenTextures(1, &depthAttachment);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);

		if (specs.m_TextureTarget == GL_TEXTURE_2D)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, specs.m_InternalFormat, specs.m_Width, specs.m_Height, 0, specs.m_Format, specs.m_Type, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			if (specs.m_CompareMode != GL_NONE)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, specs.m_CompareMode);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, specs.m_CompareFunc);
			}
		}
		else if (specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
		{
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, specs.m_Samples, specs.m_InternalFormat, specs.m_Width, specs.m_Height, specs.m_FixedSampleLocations);
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, specs.m_MinFilter);
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, specs.m_MagFilter);
		}
		else
		{
			RD_CORE_ERROR("Invalid texture target for depth attachment in framebuffer {}", m_Name);
			glDeleteTextures(1, &depthAttachment);
			return false;
		}

		// Bind the texture to the framebuffer as a depth attachment
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, specs.m_TextureTarget, depthAttachment, 0);

		// Check if the framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			RD_CORE_ERROR("Framebuffer {} is not complete after setting up depth attachment", m_Name);
			// Cleanup
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glDeleteTextures(1, &depthAttachment);
			return false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Add the depth attachment to the list of attachments
		m_DepthAttachment = { {depthAttachment}, specs };
		return true;
	}

	bool Framebuffer::CreateStencilAttachment(const StencilAttachmentSpecification& specs)
	{
		if (m_StencilAttachment.m_RendererId)
		{
			RD_CORE_WARN("Stencil attachment already exists in framebuffer {}", m_Name);
			return false;
		}

		GLuint stencilAttachment;
		glGenTextures(1, &stencilAttachment);

		glBindTexture(specs.m_TextureTarget, stencilAttachment);

		if (specs.m_TextureTarget == GL_TEXTURE_2D)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, specs.m_InternalFormat, specs.m_Width, specs.m_Height, 0, specs.m_Format, specs.m_Type, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, specs.m_MinFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, specs.m_MagFilter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, specs.m_WrapS);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, specs.m_WrapT);
		}
		else if (specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
		{
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, specs.m_Samples, specs.m_InternalFormat, specs.m_Width, specs.m_Height, specs.m_FixedSampleLocations);
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MIN_FILTER, specs.m_MinFilter);
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAG_FILTER, specs.m_MagFilter);
		}
		else
		{
			RD_CORE_ERROR("Invalid texture target for stencil attachment in framebuffer {}", m_Name);
			glDeleteTextures(1, &stencilAttachment);
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, specs.m_TextureTarget, stencilAttachment, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			RD_CORE_ERROR("Framebuffer {} is not complete after setting up stencil attachment", m_Name);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, specs.m_TextureTarget, 0, 0);
			glDeleteTextures(1, &stencilAttachment);
			return false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Store the stencil attachment
		m_StencilAttachment = { stencilAttachment, specs };
		return true;
	}

	bool Framebuffer::CreateDepthStencilAttachment(const DepthStencilAttachmentSpecification& specs)
	{
		if (m_DepthStencilAttachment.m_RendererId)
		{
			RD_CORE_WARN("Depth-Stencil attachment already exists in framebuffer {}", m_Name);
			return false;
		}

		GLuint depthStencilAttachment;
		glGenTextures(1, &depthStencilAttachment);

		glBindTexture(specs.m_TextureTarget, depthStencilAttachment);

		if (specs.m_TextureTarget == GL_TEXTURE_2D)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, specs.m_InternalFormat, specs.m_Width, specs.m_Height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		else if (specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
		{
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, specs.m_Samples, specs.m_InternalFormat, specs.m_Width, specs.m_Height, specs.m_FixedSampleLocations);
		}
		else
		{
			RD_CORE_ERROR("Invalid texture target for depth-stencil attachment in framebuffer {}", m_Name);
			glDeleteTextures(1, &depthStencilAttachment);
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, specs.m_TextureTarget, depthStencilAttachment, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			RD_CORE_ERROR("Framebuffer {} is not complete after setting up depth-stencil attachment", m_Name);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, specs.m_TextureTarget, 0, 0);
			glDeleteTextures(1, &depthStencilAttachment);
			return false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Store the depth-stencil attachment
		m_DepthStencilAttachment = { depthStencilAttachment, specs };
		return true;
	}

	void Framebuffer::Bind() const
	{
		// Bind this framebuffer as both the read and draw framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererId);
	}

	void Framebuffer::Unbind() const
	{
		// Unbind the currently bound framebuffer (revert to default framebuffer)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::Clear()
	{
		Bind();
		if(GL_COLOR_BUFFER_BIT & m_ClearSetting)
			for(const auto& it : m_ColorAttachments)
				if(it.m_RendererId)
					glClearTexImage(it.m_RendererId, 0, it.m_Specs.m_Format, it.m_Specs.m_Type, &it.m_Specs.m_ClearColor.m_Data);
		glClear(m_ClearSetting & ~GL_COLOR_BUFFER_BIT);
		Unbind();
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		// Update the size of all color attachments
		for (auto& attachment : m_ColorAttachments)
		{
			glBindTexture(attachment.m_Specs.m_TextureTarget, attachment.m_RendererId);
			if (attachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, attachment.m_Specs.m_InternalFormat, width, height, 0, attachment.m_Specs.m_Format, attachment.m_Specs.m_Type, nullptr);
			}
			else if (attachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, attachment.m_Specs.m_Samples, attachment.m_Specs.m_InternalFormat, width, height, attachment.m_Specs.m_FixedSampleLocations);
			}
			else if (attachment.m_Specs.m_TextureTarget == GL_TEXTURE_CUBE_MAP)
			{
				for (GLuint face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; ++face)
				{
					glTexImage2D(face, 0, attachment.m_Specs.m_InternalFormat, width, height, 0, attachment.m_Specs.m_Format, attachment.m_Specs.m_Type, nullptr);
				}
			}
			attachment.m_Specs.m_Width = width;
			attachment.m_Specs.m_Height = height;
			glBindTexture(attachment.m_Specs.m_TextureTarget, 0);
		}

		// Update the size of the depth attachment
		if (m_DepthAttachment.m_RendererId)
		{
			glBindTexture(m_DepthAttachment.m_Specs.m_TextureTarget, m_DepthAttachment.m_RendererId);
			if (m_DepthAttachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, m_DepthAttachment.m_Specs.m_InternalFormat, width, height, 0, m_DepthAttachment.m_Specs.m_Format, m_DepthAttachment.m_Specs.m_Type, nullptr);
			}
			else if (m_DepthAttachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_DepthAttachment.m_Specs.m_Samples, m_DepthAttachment.m_Specs.m_InternalFormat, width, height, m_DepthAttachment.m_Specs.m_FixedSampleLocations);
			}
			m_DepthAttachment.m_Specs.m_Width = width;
			m_DepthAttachment.m_Specs.m_Height = height;
			glBindTexture(m_DepthAttachment.m_Specs.m_TextureTarget, 0);
		}

		// Update the size of the stencil attachment
		if (m_StencilAttachment.m_RendererId)
		{
			glBindTexture(m_StencilAttachment.m_Specs.m_TextureTarget, m_StencilAttachment.m_RendererId);
			if (m_StencilAttachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, m_StencilAttachment.m_Specs.m_InternalFormat, width, height, 0, m_StencilAttachment.m_Specs.m_Format, m_StencilAttachment.m_Specs.m_Type, nullptr);
			}
			else if (m_StencilAttachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_StencilAttachment.m_Specs.m_Samples, m_StencilAttachment.m_Specs.m_InternalFormat, width, height, m_StencilAttachment.m_Specs.m_FixedSampleLocations);
			}
			m_StencilAttachment.m_Specs.m_Width = width;
			m_StencilAttachment.m_Specs.m_Height = height;
			glBindTexture(m_StencilAttachment.m_Specs.m_TextureTarget, 0);
		}

		// Update the size of the depth-stencil attachment
		if (m_DepthStencilAttachment.m_RendererId)
		{
			glBindTexture(m_DepthStencilAttachment.m_Specs.m_TextureTarget, m_DepthStencilAttachment.m_RendererId);
			if (m_DepthStencilAttachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, m_DepthStencilAttachment.m_Specs.m_InternalFormat, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
			}
			else if (m_DepthStencilAttachment.m_Specs.m_TextureTarget == GL_TEXTURE_2D_MULTISAMPLE)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_DepthStencilAttachment.m_Specs.m_Samples, m_DepthStencilAttachment.m_Specs.m_InternalFormat, width, height, m_DepthStencilAttachment.m_Specs.m_FixedSampleLocations);
			}
			m_DepthStencilAttachment.m_Specs.m_Width = width;
			m_DepthStencilAttachment.m_Specs.m_Height = height;
			glBindTexture(m_DepthStencilAttachment.m_Specs.m_TextureTarget, 0);
		}
	}
}
