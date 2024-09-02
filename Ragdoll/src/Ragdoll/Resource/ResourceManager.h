/*!
\file		ResourceManager.h
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

namespace ragdoll
{
	class Framebuffer;
	struct ShaderProgram;
	class Window;
	class VertexArray;
	struct RenderData;
	struct IResource;
	class ResourceManager
	{
	public:
		void Init(std::shared_ptr<Window> window);

		void AddRenderData(const char* datasetName, std::vector<RenderData>&& data);
		std::vector<RenderData>& GetRenderData(const char* datasetName);

		std::unordered_map<const char*, std::shared_ptr<Framebuffer>>& GetFramebuffers() { return m_Framebuffers; }
		Framebuffer& GetFramebuffer(const char* name);
		ShaderProgram& GetShaderProgram(const char* name);
		VertexArray& GetPrimitiveMesh(const char* name);

	private:
		std::shared_ptr<Window> m_PrimaryWindow;

		// non-asset related resources, ie. render data, framebuffers, hardcoded primitive meshes
		std::unordered_map<const char*, std::vector<RenderData>> m_RenderData;
		std::unordered_map<const char*, std::shared_ptr<Framebuffer>> m_Framebuffers;
		std::unordered_map<const char*, std::shared_ptr<VertexArray>> m_PrimitiveMeshes;

		// asset related resources, ie. shaders, textures, models
		std::unordered_map<uint64_t, std::shared_ptr<IResource>> m_ResourceRegistry;	//keep tracks of all loaded assets
		std::unordered_map<const char*, std::shared_ptr<ShaderProgram>> m_ShaderPrograms;
	};
}
