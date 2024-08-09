# Window callbacks
## Functionality
GLFW requires callback functions to be provided, so we can determine what action to take based on the events. Whenever something happens to the window, GLFW will call the function that was set using the relevant set callback functions. For instance, I set a lambda function as a callback for GLFW when the close button is pressed. The lambda function will be executed, and it will raise the relevant events to get the application to close.

```cpp
//in Ragdoll/src/Ragdoll/Graphics/Window/Windows.cpp
glfwSetWindowCloseCallback(m_GlfwWindow, [](GLFWwindow* window)
{
    Window& data = *static_cast<Window*>(glfwGetWindowUserPointer(window));
    //raise the relevant events...
});
```

This function allows us to give the GLFW window a pointer to our user made wrapper, so we can access the needed variables/functions within our own Window class.

```cpp
//in Ragdoll/src/Ragdoll/Graphics/Window/Windows.cpp
glfwSetWindowUserPointer(m_GlfwWindow, this);
```

The lambda function will cast the pointer given from ```glfwGetWindowUserPointer```, and we can call our engine event callbacks from there, so we can propagate GLFW callbacks to our parts of our engine that will require it.

I have expanded on the window properties struct, to give additional control over the window's behaviours.

```cpp
//in Ragdoll/src/Ragdoll/Graphics/Window/Windows.h
struct WindowProperties
{
    std::string m_Title{ "Ragdoll Engine" };
    int m_Width{ 800 };
    int m_Height{ 600 };
    //new stuff
    //position of the window
    Vector2 m_Position{};
    //MSAA sample count
    int m_NumSamplesMSAA{ 0 };
    //background color of the window
    Vector3 m_BackgroundColor{};
    //opacity of the window
    float m_Opacity{ 1.f };
    //ability to resize
    bool m_Resizable{ true };
    //should be visible
    bool m_Visible{ true };
    //is it in focused
    bool m_Focused{ true };
    //is the window in fullscreen
    bool m_Fullscreen{ true };
    //specifies whether the windowed mode window will have window decorations 
    //such as a border, a close widget, etc.
    bool m_Decorated{ true };
    //whether the window is at the top
    bool m_Topmost{ false };
    //specifies whether window should be given focus on show
    bool m_FocusOnShow{ true };

    //From here is personal preference
    bool m_DisplayDetailsInTitle{ true };
    bool m_DisplayFpsInTitle{ true };
    bool m_DisplayFrameCountInTitle{ true };
};
```

This lets me configure the window based on the properties given by the user.

```cpp
//in Ragdoll/src/Ragdoll/Graphics/Window/Windows.cpp
glfwWindowHint(GLFW_RESIZABLE, m_Properties.m_Resizable);
glfwWindowHint(GLFW_VISIBLE, m_Properties.m_Visible);
glfwWindowHint(GLFW_FOCUSED, m_Properties.m_Focused);
glfwWindowHint(GLFW_DECORATED, m_Properties.m_Decorated);
glfwWindowHint(GLFW_FLOATING, m_Properties.m_Topmost);
glfwWindowHint(GLFW_FOCUS_ON_SHOW, m_Properties.m_FocusOnShow);
glfwWindowHint(GLFW_SAMPLES, m_Properties.m_NumSamplesMSAA);
```

## Events
This event system is lifted off from Cherno's Game Engine Series, specifically from https://www.youtube.com/watch?v=xnopUoZbMEk. It is splitted into 2 parts, the event itself and a event dispatcher that will call all the relevant callbacks based on the event. Events have a enum type such as ```WindowClose```, ```WindowResize``` and etc, and also have category flag 
```EventCategoryApplication = BIT(0)```, ```EventCategoryInput = BIT(1)``` and etc. The event base class will be inherited to create new events in the engine.

```cpp
//in Ragdoll/src/Ragdoll/Event/Event.h
class Event
{
public:
    //flag to control if the event has been handled, and will not be propagated further 
    //to other systems
	bool m_Handled = false;

    //force all child events to define these information based functions
	virtual EventType GetEventType() const = 0;
	virtual const char* GetName() const = 0;
	virtual int GetCategoryFlags() const = 0;
	virtual std::string ToString() const { return GetName(); }
	bool IsInCategory(EventCategory category) const
	{
		return GetCategoryFlags() & category;
	}
};
```

So to create a new event, in this example of a window resizing event, simply create a new class that inherits the event base class. The new class will then declare the items it needs, which is the new width and height of the window. The actual resizing of the window is handled by GLFW, but due to the change in the window size, perhaps some other system such as the renderer will need to update its framebuffer or some camera projection matrices. This is why an event system is needed to propagate through the engine and call the respective callbacks.

```cpp
//in Ragdoll/src/Ragdoll/Event/Event.h
//macros to help with defining the type and categories
#define EVENT_CLASS_TYPE(type)
    static EventType GetStaticType() { return EventType::type; }\
	virtual EventType GetEventType() const override { return GetStaticType(); }\
    virtual const char* GetName() const override { return #type; }
#define EVENT_CLASS_CATEGORY(category)
    virtual int GetCategoryFlags() const override { return category; }
```

```cpp
//in Ragdoll/src/Ragdoll/Event/WindowEvents.h
class WindowResizeEvent : public Event
{
public:
	WindowResizeEvent(int _width, int _height)
		: m_Width(_width), m_Height(_height) {}

	inline unsigned int GetWidth() const { return m_Width; }
	inline unsigned int GetHeight() const { return m_Height; }

    //in the case where the event need to be printed out
	std::string ToString() const override
	{
		std::stringstream ss;
		ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
		return ss.str();
	}

	//macros to help define the pure virtual functions in the event class
	EVENT_CLASS_TYPE(WindowResize)
	EVENT_CLASS_CATEGORY(EventCategoryApplication)
private:
	int m_Width, m_Height;
};
```

So now we have our events, and a easy way to distinguish them. Now we just need to call the appropriate callbacks based on the events raised. This is the job of the event dispatcher. This amalgamation ```*static_cast<T*>(&m_Event)``` is quite simple to deduce. It first references the event object ```&m_Event```, and cast the pointer to be of type T* ```static_cast<T*>```, since m_Event is a the event base class, we need to cast the pointer to the inherited type. Since we verified that the event type is correct, there is no worries with casting it to the wrong event. The last thing to do is to dereference the event and use it as the parameter for the event callback function.

```cpp
//in Ragdoll/src/Ragdoll/Event/Event.h
class EventDispatcher
{
public:
    //the event dispatcher will be created with a event.
	EventDispatcher(Event& event)
		: m_Event(event)
	{
	}

    //all event callbacks must have the same signature, in this case it will be
    //bool funcName(Event&)
	template<typename T>
	using EventFn = std::function<bool(T&)>;
    //when dispatch is called, it will check if the callback function is listening 
    //to the event it currently possess
	template<typename T>
	bool Dispatch(EventFn<T> func)
	{
        //checks if the event has been handled, otherwise there is no need to cont
        if (m_Event.m_Handled)
            return false;
        //here it checks if the enum the event it has is the same as event watched
        //by the callback. type T is the event class, and T::GetStaticType will
        //give the enum type given to it in the definition
		if (m_Event.GetEventType() == T::GetStaticType())
		{
            //if it matches, call the callback
			m_Event.m_Handled = func(*static_cast<T*>(&m_Event));
			return true;
		}
		return false;
	}
private:
	Event& m_Event;
};
```

For the event dispatcher and events to work, we will need a system at a high enough level so it can propagate the event downwards to all the system, and that would be the application. The application will define a ```OnEvent(Event& event)``` function, which will be called by whoever needs to raise an event. In this case, it will dispatch the ```WindowClose```, ```WindowResize``` and ```WindowMoved``` events. ```RD_BIND_EVENT_FN``` is just an easy way for me to bind a member function of the application class.

```cpp
//in Ragdoll/src/Ragdoll/Application.cpp
void Application::OnEvent(Event& event)
{
    //creates a dispatcher for the event
    EventDispatcher dispatcher(event);
    //calls all the relevant callback if event type matches
    static auto closedCallback = RD_BIND_EVENT_FN(Application::OnWindowClose);
    dispatcher.Dispatch<WindowCloseEvent>(closedCallback);
    static auto resizeCallback = RD_BIND_EVENT_FN(Application::OnWindowResize);
    dispatcher.Dispatch<WindowResizeEvent>(resizeCallback);
    static auto moveCallback = RD_BIND_EVENT_FN(Application::OnWindowMove);
    dispatcher.Dispatch<WindowMoveEvent>(moveCallback);
}
```

For the window to raise a event to application, it will need to also have a event callback for itself. When a GLFW event occurs, it will call this callback and propagate the event throughout the application.

```cpp
//in Ragdoll/src/Ragdoll/Application.cpp
m_PrimaryWindow->SetEventCallback(RD_BIND_EVENT_FN(Application::OnEvent));
```

For example, the window will register a callback function for when the window resizes. When it occurs, it will call the application ```OnEvent``` function, where the application will call the relevant callbacks that is listening to the event.

```cpp
//in Ragdoll/src/Ragdoll/Graphics/Window/Window.cpp
glfwSetWindowSizeCallback(m_GlfwWindow, [](GLFWwindow* window, int _width, int _height)
{
	Window& data = *static_cast<Window*>(glfwGetWindowUserPointer(window));
    //update information
	data.m_BufferWidth = _width;
	data.m_BufferHeight = _height;

    //create the event
	WindowResizeEvent event{_width, _height};
    //call the application OnEvent callback with this event
	data.m_Callback(event);
});
```

The application will have its ```OnEvent(Event& event)``` called, and a dispatcher for ```WindowResize``` will be created. The application is also currently listening to the resize event, where the callback is ```Application::OnWindowResize```. The callback will be executed and logs for the window new size will be printed.

```cpp
bool Application::OnWindowResize(WindowResizeEvent& event)
{
#ifdef RAGDOLL_DEBUG
    //the log
    RD_CORE_TRACE(event.ToString());
#endif
    //returning false means the event will not be handled, and thus won't block
    //other systems from receiving the event.
    return false;
}
```

<video src="https://github.com/buttception/RagdollEngine/blob/0fa38616d49e289b674606326282d1213201fada/Blog/resources/5_window_move.mp4" placeholder="" autoplay loop controls muted title="Window resize event"></video>

![Test Video](https://github.com/buttception/RagdollEngine/blob/642c15339005171610e814afe88122a1f881c7f1/Blog/resources/5_window_move.mp4)

## Framerate
I added a quick way to tell what the current framerate of the engine is, as well as some relevant information that maybe needed. ```std::chrono``` is used to be able to get the delta time between frames. ```Timestep``` is a helper class to easily get my time in seconds or milliseconds.

```cpp
//current time
auto now = std::chrono::steady_clock::now();
//get the difference between the last frame time and now, dividing by
//1000000000.0 as value is given in nano seconds
Timestep timestep{ std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_LastFrameTime).count() / 1000000000.0 };
m_LastFrameTime = now;
//delta time in seconds
m_DeltaTime = timestep.GetSeconds();
```

FPS is counted by actually counting the number of frames that was presented in 1s. This method is prefable for me as it is more accurate, and getting the FPS from the delta time is simply a theorectical number of the current frame.

```cpp
m_Timer += m_DeltaTime;
if(m_Timer >= 1.f)
{
	m_Timer -= 1.f;
	m_Fps = m_FpsCounter;
	m_FpsCounter = 0;
}
```

I then set the title of the window with these informations. Future debug information may go into the title bar as well.

<video src="resources/5_window_fps.mp4" placeholder="" autoplay loop controls muted title="Window title"></video>
