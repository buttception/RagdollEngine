# Application and Entry Point
## Editor and Launcher
The difference between this 2 application is to seperate the editor and game functionality of the engine. Thus features needed by only the editor ie. ImGui or launcher will not be present in either applications. The main engine components such as the renderer and ECS will be present in the main static library compiled, which will be linked to both application for use instead.
## Application
The ragdoll library will provide a the base class for the application, with 3 primary functions, ```init```, ```run```, and ```shutdown```. It will also provide a declaration for a function to call to create the application

```cpp
//in Ragdoll/src/Ragdoll/Application.h
Application* CreateApplication();
```

This function will be defined by the projects that will create the executable. For instance, the editor project, where the function is defined as

```cpp
//in Editor/src/Editor.cpp
Ragdoll::Application* Ragdoll::CreateApplication()
{
	return new Editor();
}
```

In the same file, the application class is inherited in order to define special behaviors that the editor requires. This is how I seperate the behaviors that are exclusive to either the editor or launcher. In this case, I will be able to do editor specific initialization in the overridden init function, such as setting up the GUI for the editor.

```cpp
void Init(const ApplicationConfig& config) override
{
    Application::Init(config);
    // Do editor specific initialization here
}
```

## Entry Point
The entry point of a program is the function where the execution of the program begins. This is almost always the main function. When you run a C++ program, the operating system calls the main function to start the program. In my case, it will simply create the application and call its respective functions using the extern defined ```CreateApplication()``` function from the executable projects.

```cpp
//in Ragdoll/src/Ragdoll/EntryPoint.h
extern Ragdoll::Application* Ragdoll::CreateApplication();

int main()
{
	auto app = Ragdoll::CreateApplication();

	Ragdoll::Application::ApplicationConfig config;
	app->Init(config);
	app->Run();
	app->Shutdown();

	delete app;
}
```

Take note that checking for memory leak in debug mode is important as well, hence the preprocessor definitions to specify when the relevant code is compiled.

```cpp
#ifdef RAGDOLL_DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

//Code...

#ifdef RAGDOLL_DEBUG
	_CrtDumpMemoryLeaks();
#endif
```