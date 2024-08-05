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
	pchsource "src/ragdoll.cpp"

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
		
	}

    includedirs
    {
        "src"
    }

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "RAGDOLL_DEBUG"
		runtime "Debug"
		optimize "off"
		symbols "on"

	filter "configurations:Release"
		defines "RAGDOLL_DEBUG`"
		runtime "Release"
		optimize "on"
		symbols "off"
