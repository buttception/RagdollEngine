include "Tools/dependencies.lua"

workspace "Ragdoll"
	architecture "x64"
	startproject "Editor"

	configurations
	{
		"Debug",
		"Release"
	}

outputdir = "%{cfg.buildcfg}"

group ""
	include "Ragdoll"

group "Dependencies"
	include "Ragdoll/dependencies/glfw"
	include "Ragdoll/dependencies/glad"

group "Application"
	include "Editor"
	include "Launcher"
group "Application/Dependencies"
	include "Editor/dependencies/imgui"