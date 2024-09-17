include "Tools/dependencies.lua"

workspace "Ragdoll"
	architecture "x64"
	startproject "Ragdoll"

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
	include "Ragdoll/dependencies/imgui"