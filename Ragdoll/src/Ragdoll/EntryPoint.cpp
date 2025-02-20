/*!
\file		EntryPoint.h
\date		05/08/2024

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
#include "ragdollpch.h"
#include "Application.h"
#include <cxxopts.hpp>

ragdoll::Application* ragdoll::CreateApplication()
{
	return new Application();
}

int main(int argc, char* argv[])
{
#ifdef RAGDOLL_DEBUG
	// Flag _CrtDumpMemoryLeak to be called AFTER program ends.
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	auto app = ragdoll::CreateApplication();

	cxxopts::Options options("Ragdoll Renderer", "Custom renderer using nvrhi for D3D12");
	options.add_options()
		("custom", "Create custom meshes") // a bool parameter
		("sample", "glTF sample scene to load", cxxopts::value<std::string>())
		("scene", "glTF scene to load", cxxopts::value<std::string>())
		("dbgOctree", "Draw Octree debug")
		("dbgBoxes", "Draw Bounding Box")
		("dlss", "Enable DLSS")
		;
	auto result = options.parse(argc, argv);
	ragdoll::Application::ApplicationConfig config;
	config.bCreateCustomMeshes = result["custom"].as_optional<bool>().value_or(false);
	config.bDrawDebugBoundingBoxes = result["dbgBoxes"].as_optional<bool>().value_or(false);
	config.bInitDLSS = result["dlss"].as_optional<bool>().value_or(false);
	config.glTfSampleSceneToLoad = result["sample"].as_optional<std::string>().value_or("");
	config.glTfSceneToLoad = result["scene"].as_optional<std::string>().value_or("");

	app->Init(config);
	app->Run();
	app->Shutdown();

	delete app;
}