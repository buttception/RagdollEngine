include "dependencies.lua"

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

group "Application"
	include "Editor"
	include "Launcher"