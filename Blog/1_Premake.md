# Premake
## What is Premake?
Premake is a build configuration tool that helps automate the generation of project files for different development environments. It is designed to make it easier to manage the build process for complex software projects by providing a way to define the build configuration in a simple, scriptable, and portable format.

Here are some key features of Premake:

*Scriptable*: Premake uses Lua scripts to define the build configuration. This allows for a high level of customization and automation.

*Cross-Platform*: It supports multiple platforms, including Windows, macOS, and Linux. This makes it easier to manage projects that need to be built on different operating systems.

*Multiple Project Formats*: Premake can generate project files for a variety of IDEs and build systems, including Visual Studio, Xcode, Code::Blocks, GNU Make, and others.

*Flexible and Extensible*: It allows developers to define custom build rules and extend the functionality to suit specific project needs.

*Open Source*: Premake is an open-source project, which means it is free to use and modify.

Here is the link to premake: https://premake.github.io/

## Why Premake

Using premake to generate the solution and all the projects required for the solution, it is easy to ensure that no merge conflicts regarding configurations files in the repositories. It is also much easier to maintain Lua scripts instead of manually editting the build configurations such as include directories or library linking. This prevents team members working on the same project from having differing solution configuration, saving you time from having debug what is wrong with their solutions.
### But why not CMake?
Personally I prefer using Premake as it possesses enough functionality for me to get the job done, as well as being in Lua and hence easier for me to read, write and maintain. CMake uses their own custom scripting language, that poses an additional learning curve for me.

## How
Premake uses lua scripts in your directories to generate the solution and projects.
```lua
--In premake5.lua
workspace "Ragdoll"
	architecture "x64"
	startproject "Editor"

	configurations
	{
		"Debug",
		"Release"
	}
    
    outputdir = "%{cfg.buildcfg}"
```
This creates a x64 solution named Ragdoll, where the start up project will be the Editor. This solution will also have a Debug and Release mode. The outputdir variable is assigned with the token ```%{cfg.buildcfg}```, which specify if it is in "Debug" or "Release" mode. This variable will be used by the other premake scripts to easily determine what the output directory should be. From this, you can see how easy and readable it is to use premake to generate everything you need. Another example is this:
```lua
--In Ragdoll/premake5.lua
project "Ragdoll"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
	warnings "Extra"
	externalwarnings "Off"

	targetdir ("%{wks.location}/build/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build-int/" .. outputdir .. "/%{prj.name}")

	flags
	{
		"MultiProcessorCompile"
	}
```
This creates a project inside of the solution called Ragdoll, where the main engine components will reside. It is a static library in C++20, where warnings is set to "Extra". It is also easy to specify where the output of the project will go, where the ```%{wks.location}``` is the directory of where the solution is in. ```.. outputdir ..``` appeneds the previously specified ```%{cfg.buildcfg}```, and hence would set the target directory to 
```
{solution directory}/build/{Debug or Release}/{Name of the project}
```
You can even specify this such as ```MultiProcessorCompile```, which enables your solution to utilize the multi core compilation, making your solution compile much faster.

A simple batch file is then used to call the premake executable so the solution generation can be done with running that file. In this specific case, it generates a Visual Studio 2022 solution.
```batch
set "ver=vs2022"
call Tools\premake\premake5.exe %ver%
```

## Conclusion
In conclusion, using something to generate the solution for you is always the better alternative compared to pushing the solution configurations into the repository. This saves you the hassle of dealing with incorrect settings due to human errors when working in a team, especially as the project progresses and new dependencies or settings are required. It may take additional time to write all these scripts, but trust me, it will save you a lot more time in the long run.

# Solution Setup
## Editor and Launcher
The difference between this 2 application is to seperate the editor and game functionality of the engine. Thus features needed by only the editor ie. ImGui or launcher will not be present in either applications. The main engine components such as the renderer and ECS will be present in the main static library compiled, which will be linked to both application for use instead.