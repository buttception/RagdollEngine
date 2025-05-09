include "../dependencies.lua"

project "Ragdoll"
	language "C++"
	cppdialect "C++20"
	staticruntime "On"

	targetdir ("%{wks.location}/build/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build-int/" .. outputdir .. "/%{prj.name}")

	pchheader "ragdollpch.h"
	pchsource "src/ragdollpch.cpp"

	flags
	{
		"MultiProcessorCompile"
	}

	files
	{
		"src/**.h",
		"src/**.hpp",
		"src/**.cpp",
		"../assets/**.hlsl",
		"../assets/**.hlsli",
	}

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
		"_ITERATOR_DEBUG_LEVEL=0"
    }

	libdirs
	{
		"%{LibDirs.dlss}",
		"%{LibDirs.meshoptimizer}"
	}

	links
	{
		"GLFW",
		"imgui",
		"d3d12",
		"dxgi",
		"meshoptimizer"
	}
	
	vpaths 
	{
		["shaders"] = { "**.hlsl", "**.hlsli" }
	}	

    includedirs
    {
        "src",
		"%{IncludesDir.spdlog}",
		"%{IncludesDir.glfw}",
		"%{IncludesDir.glm}",
		"%{IncludesDir.imgui}",
		"%{IncludesDir.entt}",
		"%{IncludesDir.nvrhi}",
		"%{IncludesDir.tinygltf}",
		"%{IncludesDir.microprofile}",
		"%{IncludesDir.cxxopts}",
		"%{IncludesDir.taskflow}",
		"%{IncludesDir.dlss}",
		"%{IncludesDir.directxtex}",
		"%{IncludesDir.meshoptimizer}",
    }

	prebuildcommands
	{
		"\"%{wks.location}Tools\\compileShader.bat\"",
	}

	postbuildcommands
	{
		"xcopy /Y /E /I \"%{LibDirs.dlss}\\Windows_x86_64\\dev\\nvngx_dlss.dll\" \"%{cfg.targetdir}\"",
		"xcopy /Y /E /I \"%{LibDirs.meshoptimizer}\\meshoptimizer.lib\" \"%{cfg.targetdir}\"",
		"xcopy /Y /E /I \"%{wks.location}\\assets\\cso\" \"%{cfg.targetdir}\\..\\assets\\cso\"",
	}
	
	filter "files:**.hlsl"
		buildaction "None"
	filter "files:**.hlsli"
		buildaction "None"

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "RAGDOLL_DEBUG"
		runtime "Debug"
		optimize "off"
		symbols "on"
		kind "ConsoleApp"
		links
		{
			"Windows_x86_64/x86_64/nvsdk_ngx_s_dbg_iterator0.lib",
		}

	filter "configurations:Release"
		defines "RAGDOLL_RELEASE"
		runtime "Release"
		optimize "on"
		symbols "off"
		kind "WindowedApp"
		entrypoint "mainCRTStartup"
		links
		{
			"Windows_x86_64/x86_64/nvsdk_ngx_s.lib",
		}
	