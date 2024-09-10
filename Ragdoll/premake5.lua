include "../dependencies.lua"

project "Ragdoll"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
	warnings "Extra"
	externalwarnings "Off"

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
		"src/**.cpp"
	}

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

	libdirs
	{
		
	}

	links
	{
		"GLFW",
		"imgui",
		"d3d12",
		"dxgi",
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
    }

	prebuildcommands
	{
		"\"%{wks.location}Tools\\compileShader.bat\""
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "RAGDOLL_DEBUG"
		runtime "Debug"
		optimize "off"
		symbols "on"

	filter "configurations:Release"
		defines "RAGDOLL_RELEASE"
		runtime "Release"
		optimize "on"
		symbols "off"
