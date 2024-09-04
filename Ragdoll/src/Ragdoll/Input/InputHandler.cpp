/*!
\file		InputHandler.cpp
\date		06/08/2024

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

#include "ragdollpch.h"

#include "InputHandler.h"

#include "Ragdoll/Core/Logger.h"
#include <imgui.h>

namespace ragdoll
{
	const std::unordered_map<char, Key> InputHandler::s_CharToKeyMap = {
	{ ' ', Key::Space },
	{ '\'', Key::Apostrophe },
	{ ',', Key::Comma },
	{ '-', Key::Minus },
	{ '.', Key::Period },
	{ '/', Key::Slash },
	{ '0', Key::Num0 },
	{ '1', Key::Num1 },
	{ '2', Key::Num2 },
	{ '3', Key::Num3 },
	{ '4', Key::Num4 },
	{ '5', Key::Num5 },
	{ '6', Key::Num6 },
	{ '7', Key::Num7 },
	{ '8', Key::Num8 },
	{ '9', Key::Num9 },
	{ ';', Key::Semicolon },
	{ '=', Key::Equal },
	{ 'A', Key::A },
	{ 'B', Key::B },
	{ 'C', Key::C },
	{ 'D', Key::D },
	{ 'E', Key::E },
	{ 'F', Key::F },
	{ 'G', Key::G },
	{ 'H', Key::H },
	{ 'I', Key::I },
	{ 'J', Key::J },
	{ 'K', Key::K },
	{ 'L', Key::L },
	{ 'M', Key::M },
	{ 'N', Key::N },
	{ 'O', Key::O },
	{ 'P', Key::P },
	{ 'Q', Key::Q },
	{ 'R', Key::R },
	{ 'S', Key::S },
	{ 'T', Key::T },
	{ 'U', Key::U },
	{ 'V', Key::V },
	{ 'W', Key::W },
	{ 'X', Key::X },
	{ 'Y', Key::Y },
	{ 'Z', Key::Z },
	{ '[', Key::LeftBracket },
	{ '\\', Key::Backslash },
	{ ']', Key::RightBracket },
	{ '`', Key::GraveAccent },
	{ '!', Key::World1 },
	{ '"', Key::World2 }
	};

	const std::unordered_map<Key, char> InputHandler::s_KeyToCharMap = {
		{ Key::Space, ' ' },
		{ Key::Apostrophe, '\'' },
		{ Key::Comma, ',' },
		{ Key::Minus, '-' },
		{ Key::Period, '.' },
		{ Key::Slash, '/' },
		{ Key::Num0, '0' },
		{ Key::Num1, '1' },
		{ Key::Num2, '2' },
		{ Key::Num3, '3' },
		{ Key::Num4, '4' },
		{ Key::Num5, '5' },
		{ Key::Num6, '6' },
		{ Key::Num7, '7' },
		{ Key::Num8, '8' },
		{ Key::Num9, '9' },
		{ Key::Semicolon, ';' },
		{ Key::Equal, '=' },
		{ Key::A, 'A' },
		{ Key::B, 'B' },
		{ Key::C, 'C' },
		{ Key::D, 'D' },
		{ Key::E, 'E' },
		{ Key::F, 'F' },
		{ Key::G, 'G' },
		{ Key::H, 'H' },
		{ Key::I, 'I' },
		{ Key::J, 'J' },
		{ Key::K, 'K' },
		{ Key::L, 'L' },
		{ Key::M, 'M' },
		{ Key::N, 'N' },
		{ Key::O, 'O' },
		{ Key::P, 'P' },
		{ Key::Q, 'Q' },
		{ Key::R, 'R' },
		{ Key::S, 'S' },
		{ Key::T, 'T' },
		{ Key::U, 'U' },
		{ Key::V, 'V' },
		{ Key::W, 'W' },
		{ Key::X, 'X' },
		{ Key::Y, 'Y' },
		{ Key::Z, 'Z' },
		{ Key::LeftBracket, '[' },
		{ Key::Backslash, '\\' },
		{ Key::RightBracket, ']' },
		{ Key::GraveAccent, '`' },
		{ Key::World1, '!' },
		{ Key::World2, '"' }
	};

	const std::unordered_map<const char*, Key> InputHandler::s_StrToKeyMap = {
		{ "null", Key::Null },
		{ "Unknown", Key::Unknown },
		{ "Space", Key::Space },
		{ "Apostrophe", Key::Apostrophe },
		{ "Comma", Key::Comma },
		{ "Minus", Key::Minus },
		{ "Period", Key::Period },
		{ "Slash", Key::Slash },
		{ "Num0", Key::Num0 },
		{ "Num1", Key::Num1 },
		{ "Num2", Key::Num2 },
		{ "Num3", Key::Num3 },
		{ "Num4", Key::Num4 },
		{ "Num5", Key::Num5 },
		{ "Num6", Key::Num6 },
		{ "Num7", Key::Num7 },
		{ "Num8", Key::Num8 },
		{ "Num9", Key::Num9 },
		{ "Semicolon", Key::Semicolon },
		{ "Equal", Key::Equal },
		{ "A", Key::A },
		{ "B", Key::B },
		{ "C", Key::C },
		{ "D", Key::D },
		{ "E", Key::E },
		{ "F", Key::F },
		{ "G", Key::G },
		{ "H", Key::H },
		{ "I", Key::I },
		{ "J", Key::J },
		{ "K", Key::K },
		{ "L", Key::L },
		{ "M", Key::M },
		{ "N", Key::N },
		{ "O", Key::O },
		{ "P", Key::P },
		{ "Q", Key::Q },
		{ "R", Key::R },
		{ "S", Key::S },
		{ "T", Key::T },
		{ "U", Key::U },
		{ "V", Key::V },
		{ "W", Key::W },
		{ "X", Key::X },
		{ "Y", Key::Y },
		{ "Z", Key::Z },
		{ "LeftBracket", Key::LeftBracket },
		{ "Backslash", Key::Backslash },
		{ "RightBracket", Key::RightBracket },
		{ "GraveAccent", Key::GraveAccent },
		{ "World1", Key::World1 },
		{ "World2", Key::World2 },
		{ "Escape", Key::Escape },
		{ "Enter", Key::Enter },
		{ "Tab", Key::Tab },
		{ "Backspace", Key::Backspace },
		{ "Insert", Key::Insert },
		{ "Delete", Key::Delete },
		{ "ArrowRight", Key::ArrowRight },
		{ "ArrowLeft", Key::ArrowLeft },
		{ "ArrowDown", Key::ArrowDown },
		{ "ArrowUp", Key::ArrowUp },
		{ "PageUp", Key::PageUp },
		{ "PageDown", Key::PageDown },
		{ "Home", Key::Home },
		{ "End", Key::End },
		{ "CapsLock", Key::CapsLock },
		{ "ScrollLock", Key::ScrollLock },
		{ "NumLock", Key::NumLock },
		{ "PrintScreen", Key::PrintScreen },
		{ "Pause", Key::Pause },
		{ "F1", Key::F1 },
		{ "F2", Key::F2 },
		{ "F3", Key::F3 },
		{ "F4", Key::F4 },
		{ "F5", Key::F5 },
		{ "F6", Key::F6 },
		{ "F7", Key::F7 },
		{ "F8", Key::F8 },
		{ "F9", Key::F9 },
		{ "F10", Key::F10 },
		{ "F11", Key::F11 },
		{ "F12", Key::F12 },
		{ "F13", Key::F13 },
		{ "F14", Key::F14 },
		{ "F15", Key::F15 },
		{ "F16", Key::F16 },
		{ "F17", Key::F17 },
		{ "F18", Key::F18 },
		{ "F19", Key::F19 },
		{ "F20", Key::F20 },
		{ "F21", Key::F21 },
		{ "F22", Key::F22 },
		{ "F23", Key::F23 },
		{ "F24", Key::F24 },
		{ "F25", Key::F25 },
		{ "Numpad0", Key::Numpad0 },
		{ "Numpad1", Key::Numpad1 },
		{ "Numpad2", Key::Numpad2 },
		{ "Numpad3", Key::Numpad3 },
		{ "Numpad4", Key::Numpad4 },
		{ "Numpad5", Key::Numpad5 },
		{ "Numpad6", Key::Numpad6 },
		{ "Numpad7", Key::Numpad7 },
		{ "Numpad8", Key::Numpad8 },
		{ "Numpad9", Key::Numpad9 },
		{ "NumpadDecimal", Key::NumpadDecimal },
		{ "NumpadDivide", Key::NumpadDivide },
		{ "NumpadMultiply", Key::NumpadMultiply },
		{ "NumpadSubtract", Key::NumpadSubtract },
		{ "NumpadAdd", Key::NumpadAdd },
		{ "NumpadEnter", Key::NumpadEnter },
		{ "NumpadEqual", Key::NumpadEqual },
		{ "LeftShift", Key::LeftShift },
		{ "LeftControl", Key::LeftControl },
		{ "LeftAlt", Key::LeftAlt },
		{ "LeftSuper", Key::LeftSuper },
		{ "RightShift", Key::RightShift },
		{ "RightControl", Key::RightControl },
		{ "RightAlt", Key::RightAlt },
		{ "RightSuper", Key::RightSuper },
		{ "Menu", Key::Menu },
		{ "MaxKey", Key::MaxKey }
	};

	const std::unordered_map<Key, const char*> InputHandler::s_KeyToStrMap = {
		{ Key::Null, "null" },
		{ Key::Unknown, "Unknown" },
		{ Key::Space, "Space" },
		{ Key::Apostrophe, "Apostrophe" },
		{ Key::Comma, "Comma" },
		{ Key::Minus, "Minus" },
		{ Key::Period, "Period" },
		{ Key::Slash, "Slash" },
		{ Key::Num0, "Num0" },
		{ Key::Num1, "Num1" },
		{ Key::Num2, "Num2" },
		{ Key::Num3, "Num3" },
		{ Key::Num4, "Num4" },
		{ Key::Num5, "Num5" },
		{ Key::Num6, "Num6" },
		{ Key::Num7, "Num7" },
		{ Key::Num8, "Num8" },
		{ Key::Num9, "Num9" },
		{ Key::Semicolon, "Semicolon" },
		{ Key::Equal, "Equal" },
		{ Key::A, "A" },
		{ Key::B, "B" },
		{ Key::C, "C" },
		{ Key::D, "D" },
		{ Key::E, "E" },
		{ Key::F, "F" },
		{ Key::G, "G" },
		{ Key::H, "H" },
		{ Key::I, "I" },
		{ Key::J, "J" },
		{ Key::K, "K" },
		{ Key::L, "L" },
		{ Key::M, "M" },
		{ Key::N, "N" },
		{ Key::O, "O" },
		{ Key::P, "P" },
		{ Key::Q, "Q" },
		{ Key::R, "R" },
		{ Key::S, "S" },
		{ Key::T, "T" },
		{ Key::U, "U" },
		{ Key::V, "V" },
		{ Key::W, "W" },
		{ Key::X, "X" },
		{ Key::Y, "Y" },
		{ Key::Z, "Z" },
		{ Key::LeftBracket, "LeftBracket" },
		{ Key::Backslash, "Backslash" },
		{ Key::RightBracket, "RightBracket" },
		{ Key::GraveAccent, "GraveAccent" },
		{ Key::World1, "World1" },
		{ Key::World2, "World2" },
		{ Key::Escape, "Escape" },
		{ Key::Enter, "Enter" },
		{ Key::Tab, "Tab" },
		{ Key::Backspace, "Backspace" },
		{ Key::Insert, "Insert" },
		{ Key::Delete, "Delete" },
		{ Key::ArrowRight, "ArrowRight" },
		{ Key::ArrowLeft, "ArrowLeft" },
		{ Key::ArrowDown, "ArrowDown" },
		{ Key::ArrowUp, "ArrowUp" },
		{ Key::PageUp, "PageUp" },
		{ Key::PageDown, "PageDown" },
		{ Key::Home, "Home" },
		{ Key::End, "End" },
		{ Key::CapsLock, "CapsLock" },
		{ Key::ScrollLock, "ScrollLock" },
		{ Key::NumLock, "NumLock" },
		{ Key::PrintScreen, "PrintScreen" },
		{ Key::Pause, "Pause" },
		{ Key::F1, "F1" },
		{ Key::F2, "F2" },
		{ Key::F3, "F3" },
		{ Key::F4, "F4" },
		{ Key::F5, "F5" },
		{ Key::F6, "F6" },
		{ Key::F7, "F7" },
		{ Key::F8, "F8" },
		{ Key::F9, "F9" },
		{ Key::F10, "F10" },
		{ Key::F11, "F11" },
		{ Key::F12, "F12" },
		{ Key::F13, "F13" },
		{ Key::F14, "F14" },
		{ Key::F15, "F15" },
		{ Key::F16, "F16" },
		{ Key::F17, "F17" },
		{ Key::F18, "F18" },
		{ Key::F19, "F19" },
		{ Key::F20, "F20" },
		{ Key::F21, "F21" },
		{ Key::F22, "F22" },
		{ Key::F23, "F23" },
		{ Key::F24, "F24" },
		{ Key::F25, "F25" },
		{ Key::Numpad0, "Numpad0" },
		{ Key::Numpad1, "Numpad1" },
		{ Key::Numpad2, "Numpad2" },
		{ Key::Numpad3, "Numpad3" },
		{ Key::Numpad4, "Numpad4" },
		{ Key::Numpad5, "Numpad5" },
		{ Key::Numpad6, "Numpad6" },
		{ Key::Numpad7, "Numpad7" },
		{ Key::Numpad8, "Numpad8" },
		{ Key::Numpad9, "Numpad9" },
		{ Key::NumpadDecimal, "NumpadDecimal" },
		{ Key::NumpadDivide, "NumpadDivide" },
		{ Key::NumpadMultiply, "NumpadMultiply" },
		{ Key::NumpadSubtract, "NumpadSubtract" },
		{ Key::NumpadAdd, "NumpadAdd" },
		{ Key::NumpadEnter, "NumpadEnter" },
		{ Key::NumpadEqual, "NumpadEqual" },
		{ Key::LeftShift, "LeftShift" },
		{ Key::LeftControl, "LeftControl" },
		{ Key::LeftAlt, "LeftAlt" },
		{ Key::LeftSuper, "LeftSuper" },
		{ Key::RightShift, "RightShift" },
		{ Key::RightControl, "RightControl" },
		{ Key::RightAlt, "RightAlt" },
		{ Key::RightSuper, "RightSuper" },
		{ Key::Menu, "Menu" },
		{ Key::MaxKey, "MaxKey" }
	};

	const std::unordered_map<const char*, MouseButton> InputHandler::s_StrToButtonMap = {
		{ "null", MouseButton::Null },
		{ "Unknown", MouseButton::Unknown },
		{ "Button1", MouseButton::Button1 },
		{ "Button2", MouseButton::Button2 },
		{ "Button3", MouseButton::Button3 },
		{ "Button4", MouseButton::Button4 },
		{ "Button5", MouseButton::Button5 },
		{ "Button6", MouseButton::Button6 },
		{ "Button7", MouseButton::Button7 },
		{ "Button8", MouseButton::Button8 },
		{ "Left", MouseButton::Left },
		{ "Right", MouseButton::Right },
		{ "Middle", MouseButton::Middle },
		{ "SideBack", MouseButton::SideBack },
		{ "SideFront", MouseButton::SideFront },
		{ "MaxButton", MouseButton::MaxButton }
	};

	const std::unordered_map<MouseButton, const char*> InputHandler::s_ButtonToStrMap = {
		{ MouseButton::Null, "null" },
		{ MouseButton::Unknown, "Unknown" },
		{ MouseButton::Button1, "Button1" },
		{ MouseButton::Button2, "Button2" },
		{ MouseButton::Button3, "Button3" },
		{ MouseButton::Button4, "Button4" },
		{ MouseButton::Button5, "Button5" },
		{ MouseButton::Button6, "Button6" },
		{ MouseButton::Button7, "Button7" },
		{ MouseButton::Button8, "Button8" },
		{ MouseButton::Left, "Left" },
		{ MouseButton::Right, "Right" },
		{ MouseButton::Middle, "Middle" },
		{ MouseButton::SideBack, "SideBack" },
		{ MouseButton::SideFront, "SideFront" },
		{ MouseButton::MaxButton, "MaxButton" }
	};
	const std::unordered_map<Key, int> InputHandler::s_KeyToImGuiKeyMap = {
	{ Key::Space, ImGuiKey_Space },
	{ Key::Apostrophe, ImGuiKey_Apostrophe },
	{ Key::Comma, ImGuiKey_Comma },
	{ Key::Minus, ImGuiKey_Minus },
	{ Key::Period, ImGuiKey_Period },
	{ Key::Slash, ImGuiKey_Slash },
	{ Key::Num0, ImGuiKey_0 },
	{ Key::Num1, ImGuiKey_1 },
	{ Key::Num2, ImGuiKey_2 },
	{ Key::Num3, ImGuiKey_3 },
	{ Key::Num4, ImGuiKey_4 },
	{ Key::Num5, ImGuiKey_5 },
	{ Key::Num6, ImGuiKey_6 },
	{ Key::Num7, ImGuiKey_7 },
	{ Key::Num8, ImGuiKey_8 },
	{ Key::Num9, ImGuiKey_9 },
	{ Key::Semicolon, ImGuiKey_Semicolon },
	{ Key::Equal, ImGuiKey_Equal },
	{ Key::A, ImGuiKey_A },
	{ Key::B, ImGuiKey_B },
	{ Key::C, ImGuiKey_C },
	{ Key::D, ImGuiKey_D },
	{ Key::E, ImGuiKey_E },
	{ Key::F, ImGuiKey_F },
	{ Key::G, ImGuiKey_G },
	{ Key::H, ImGuiKey_H },
	{ Key::I, ImGuiKey_I },
	{ Key::J, ImGuiKey_J },
	{ Key::K, ImGuiKey_K },
	{ Key::L, ImGuiKey_L },
	{ Key::M, ImGuiKey_M },
	{ Key::N, ImGuiKey_N },
	{ Key::O, ImGuiKey_O },
	{ Key::P, ImGuiKey_P },
	{ Key::Q, ImGuiKey_Q },
	{ Key::R, ImGuiKey_R },
	{ Key::S, ImGuiKey_S },
	{ Key::T, ImGuiKey_T },
	{ Key::U, ImGuiKey_U },
	{ Key::V, ImGuiKey_V },
	{ Key::W, ImGuiKey_W },
	{ Key::X, ImGuiKey_X },
	{ Key::Y, ImGuiKey_Y },
	{ Key::Z, ImGuiKey_Z },
	{ Key::LeftBracket, ImGuiKey_LeftBracket },
	{ Key::Backslash, ImGuiKey_Backslash },
	{ Key::RightBracket, ImGuiKey_RightBracket },
	{ Key::GraveAccent, ImGuiKey_GraveAccent },
	{ Key::Escape, ImGuiKey_Escape },
	{ Key::Enter, ImGuiKey_Enter },
	{ Key::Tab, ImGuiKey_Tab },
	{ Key::Backspace, ImGuiKey_Backspace },
	{ Key::Insert, ImGuiKey_Insert },
	{ Key::Delete, ImGuiKey_Delete },
	{ Key::ArrowRight, ImGuiKey_RightArrow },
	{ Key::ArrowLeft, ImGuiKey_LeftArrow },
	{ Key::ArrowDown, ImGuiKey_DownArrow },
	{ Key::ArrowUp, ImGuiKey_UpArrow },
	{ Key::PageUp, ImGuiKey_PageUp },
	{ Key::PageDown, ImGuiKey_PageDown },
	{ Key::Home, ImGuiKey_Home },
	{ Key::End, ImGuiKey_End },
	{ Key::CapsLock, ImGuiKey_CapsLock },
	{ Key::ScrollLock, ImGuiKey_ScrollLock },
	{ Key::NumLock, ImGuiKey_NumLock },
	{ Key::PrintScreen, ImGuiKey_PrintScreen },
	{ Key::Pause, ImGuiKey_Pause },
	{ Key::F1, ImGuiKey_F1 },
	{ Key::F2, ImGuiKey_F2 },
	{ Key::F3, ImGuiKey_F3 },
	{ Key::F4, ImGuiKey_F4 },
	{ Key::F5, ImGuiKey_F5 },
	{ Key::F6, ImGuiKey_F6 },
	{ Key::F7, ImGuiKey_F7 },
	{ Key::F8, ImGuiKey_F8 },
	{ Key::F9, ImGuiKey_F9 },
	{ Key::F10, ImGuiKey_F10 },
	{ Key::F11, ImGuiKey_F11 },
	{ Key::F12, ImGuiKey_F12 },
	{ Key::F13, ImGuiKey_F13 },
	{ Key::F14, ImGuiKey_F14 },
	{ Key::F15, ImGuiKey_F15 },
	{ Key::F16, ImGuiKey_F16 },
	{ Key::F17, ImGuiKey_F17 },
	{ Key::F18, ImGuiKey_F18 },
	{ Key::F19, ImGuiKey_F19 },
	{ Key::F20, ImGuiKey_F20 },
	{ Key::F21, ImGuiKey_F21 },
	{ Key::F22, ImGuiKey_F22 },
	{ Key::F23, ImGuiKey_F23 },
	{ Key::F24, ImGuiKey_F24 },
	{ Key::LeftShift, ImGuiKey_LeftShift },
	{ Key::LeftControl, ImGuiKey_LeftCtrl },
	{ Key::LeftAlt, ImGuiKey_LeftAlt },
	{ Key::LeftSuper, ImGuiKey_LeftSuper },
	{ Key::RightShift, ImGuiKey_RightShift },
	{ Key::RightControl, ImGuiKey_RightCtrl },
	{ Key::RightAlt, ImGuiKey_RightAlt },
	{ Key::RightSuper, ImGuiKey_RightSuper },
	{ Key::Menu, ImGuiKey_Menu },
	{ Key::MaxKey, ImGuiKey_None } // Assuming no ImGuiKey for MaxKey
	};

	// Map for MouseButton enums to ImGui mouse button constants
	const std::unordered_map<MouseButton, int> InputHandler::s_ButtonToImGuiMouseButtonMap = {
		{ MouseButton::Null, ImGuiMouseButton_COUNT },
		{ MouseButton::Unknown, ImGuiMouseButton_COUNT },
		{ MouseButton::Button1, ImGuiMouseButton_Left },
		{ MouseButton::Button2, ImGuiMouseButton_Right },
		{ MouseButton::Button3, ImGuiMouseButton_Middle },
		{ MouseButton::Button4, ImGuiMouseButton_COUNT },
		{ MouseButton::Button5, ImGuiMouseButton_COUNT },
		{ MouseButton::Button6, ImGuiMouseButton_COUNT },
		{ MouseButton::Button7, ImGuiMouseButton_COUNT },
		{ MouseButton::Button8, ImGuiMouseButton_COUNT },
		{ MouseButton::Left, ImGuiMouseButton_Left },
		{ MouseButton::Right, ImGuiMouseButton_Right },
		{ MouseButton::Middle, ImGuiMouseButton_Middle },
		{ MouseButton::SideBack, ImGuiMouseButton_COUNT },
		{ MouseButton::SideFront, ImGuiMouseButton_COUNT },
		{ MouseButton::MaxButton, ImGuiMouseButton_COUNT }
	};

	void InputHandler::Init()
	{
	}

	void InputHandler::Update(double _dt)
	{
		//reset scroll boolean
		m_ScrollThisFrame = false;
		for(uint32_t i{}; i < static_cast<int32_t>(Key::MaxKey); ++i)
		{
#if RD_LOG_INPUT
			auto& key = m_Keys[i];
			//this log is one frame late
			if (key.m_InputState.m_Hold || key.m_InputState.m_Tap || key.m_InputState.m_LongPress || key.m_InputState.m_MultiTap)
				LogKeyEvents(static_cast<Key>(i), key);
#endif
			UpdateDataStatesAndTimers(static_cast<Key>(i), _dt);
		}

		for(uint32_t i{}; i < static_cast<int32_t>(MouseButton::MaxButton); ++i)
		{
#if RD_LOG_INPUT
			auto& mbtn = m_MouseButtons[i];
			//this log is one frame late
			if (mbtn.m_InputState.m_Hold || mbtn.m_InputState.m_Tap || mbtn.m_InputState.m_LongPress || mbtn.m_InputState.m_MultiTap)
				LogMouseEvents(static_cast<MouseButton>(i), mbtn);
#endif
			UpdateDataStatesAndTimers(static_cast<MouseButton>(i), _dt);
		}

		//reconcil with imgui io
		auto& io = ImGui::GetIO();
		io.MousePos = { m_MousePos.x, m_MousePos.y };
		io.MouseWheel += m_ScrollDeltas.y;

		for (const auto& key : s_KeyToImGuiKeyMap) {
			if (key.second == ImGuiKey_COUNT)
				continue;
			io.KeysDown[(ImGuiKey)key.second] = m_Keys[(int)key.first].m_InputState.m_Hold;
		}
		for (const auto& key : s_ButtonToImGuiMouseButtonMap) {
			if (key.second == ImGuiMouseButton_COUNT)
				continue;
			io.MouseDown[(ImGuiMouseButton_)key.second] = m_MouseButtons[(int)key.first].m_InputState.m_Hold;
		}
	}

	void InputHandler::Shutdown()
	{
	}

	void InputHandler::OnKeyPressed(KeyPressedEvent& event)
	{
		auto& key = m_Keys[event.GetKeyCode()];
		if(!key.m_InputState.m_Hold)
			key.m_InputState.m_Press = 1;
		key.m_InputState.m_Release = 0;
		key.m_InputState.m_Hold = 1;
		key.m_InputState.m_Repeat = event.GetRepeatCount();

		key.m_RepeatCount += key.m_InputState.m_Repeat;

		//reset tap and double press timers
		key.m_MultiTapTimer = 0.f;
	}

	void InputHandler::OnKeyReleased(KeyReleasedEvent& event)
	{
		auto& key = m_Keys[event.GetKeyCode()];
		key.m_InputState.m_Press = 0;
		key.m_InputState.m_Release = 1;
		key.m_InputState.m_Hold = 0;
		key.m_InputState.m_Repeat = 0;
		key.m_InputState.m_IncrementMultiTapTimer = 1;
		key.m_InputState.m_LongPressTriggered = 0;

		key.m_RepeatCount = 0;

		//if the tap timer is smaller the tap time, means user tapped key
		if (key.m_HeldTimer < s_TapTime)
		{
			key.m_InputState.m_Tap = 1;
		}
		//reset the long press timer
		key.m_HeldTimer = 0.f;
	}

	void InputHandler::OnKeyTyped(KeyTypedEvent& event)
	{
		UNREFERENCED_PARAMETER(event);
		//handle in the future in case character pressed is a unicode character, get the unicode value for text input
		auto& io = ImGui::GetIO();

		io.AddInputCharacter(event.GetKeyCode());
	}

	void InputHandler::OnMouseMove(MouseMovedEvent& event)
	{
		m_MousePos = { event.GetX(), event.GetY() };
		m_MouseDeltas = XMFLOAT2(m_MousePos.x - m_LastMousePos.x, m_MousePos.y - m_LastMousePos.y);
		m_LastMousePos = m_MousePos;
#if RD_LOG_INPUT
		RD_CORE_TRACE("Mouse Pos: {}, Mouse Delta: {}", m_MousePos, m_MouseDeltas);
#endif
	}

	void InputHandler::OnMouseButtonPressed(MouseButtonPressedEvent& event)
	{
		auto& mbtn = m_MouseButtons[event.GetMouseButton()];
		if (!mbtn.m_InputState.m_Hold)
			mbtn.m_InputState.m_Press = 1;
		mbtn.m_InputState.m_Release = 0;
		mbtn.m_InputState.m_Hold = 1;

		//reset tap and double press timers
		mbtn.m_MultiTapTimer = 0.f;
	}

	void InputHandler::OnMouseButtonReleased(MouseButtonReleasedEvent& event)
	{
		auto& mbtn = m_MouseButtons[event.GetMouseButton()];
		mbtn.m_InputState.m_Press = 0;
		mbtn.m_InputState.m_Release = 1;
		mbtn.m_InputState.m_Hold = 0;
		mbtn.m_InputState.m_Repeat = 0;
		mbtn.m_InputState.m_IncrementMultiTapTimer = 1;
		mbtn.m_InputState.m_LongPressTriggered = 0;

		//if the tap timer is smaller the tap time, means user tapped key
		if (mbtn.m_HeldTimer < s_TapTime)
		{
			mbtn.m_InputState.m_Tap = 1;
		}
		//reset the long press timer
		mbtn.m_HeldTimer = 0.f;
	}

	void InputHandler::OnMouseScrolled(MouseScrolledEvent& event)
	{
		m_ScrollThisFrame = true;
		m_ScrollDeltas = { event.GetXOffset(), event.GetYOffset() };
#if RD_LOG_INPUT
		RD_CORE_TRACE("Scroll Delta: {}", m_ScrollDeltas);
#endif
	}

	void InputHandler::UpdateDataStatesAndTimers(Key _key, double _dt)
	{
		UpdateDataStatesAndTimers(m_Keys[static_cast<int32_t>(_key)], _dt);
	}

	void InputHandler::UpdateDataStatesAndTimers(MouseButton _mousebtn, double _dt)
	{
		UpdateDataStatesAndTimers(m_MouseButtons[static_cast<int32_t>(_mousebtn)], _dt);
	}

	void InputHandler::UpdateDataStatesAndTimers(InputData& _data, double _dt)
	{
		//reset the states
		_data.m_InputState.m_Repeat = _data.m_InputState.m_LongPress = _data.m_InputState.m_MultiTap = _data.m_InputState.m_Tap = 0;

		//double press logic
		if (_data.m_InputState.m_IncrementMultiTapTimer)
		{
			_data.m_MultiTapTimer += static_cast<float>(_dt);
			if (_data.m_MultiTapTimer > s_MultiTapTime)
			{
				_data.m_MultiTapTimer = 0.f;
				_data.m_InputState.m_IncrementMultiTapTimer = 0;
				_data.m_TapCount = 0;
			}
		}
		if (_data.m_InputState.m_Press)
		{
			if (_data.m_InputState.m_IncrementMultiTapTimer)
			{
				if (_data.m_MultiTapTimer < s_MultiTapTime)
				{
					_data.m_InputState.m_MultiTap = 1;
					_data.m_TapCount++;
					_data.m_MultiTapTimer = 0.f;
				}
				_data.m_InputState.m_IncrementMultiTapTimer = 0;
			}
		}

		//long pressing logic
		if (_data.m_InputState.m_Hold)
		{
			_data.m_InputState.m_Press = 0;
			_data.m_HeldTimer += static_cast<float>(_dt);
			//if the _data is held for more than 0.75s, it is considered a long press
			if (_data.m_HeldTimer > s_LongPressTime && !_data.m_InputState.m_LongPressTriggered)
			{
				_data.m_InputState.m_LongPressTriggered = 1;
				_data.m_InputState.m_LongPress = 1;
				_data.m_HeldTimer = 0.f;
			}
		}
	}

#if RD_LOG_INPUT
	void InputHandler::LogKeyEvents(const Key& keyCode, const InputData& data)
	{
		const char* red = "\x1b[31m";
		const char* green = "\x1b[32m";
		const char* yellow = "\x1b[33m";
		const char* reset = "\x1b[0m";
		RD_CORE_TRACE("{}: Press[{}{}{}],Release[{}{}{}],Hold[{}{}{}],Repeat[{}{}{}],Tap[{}{}{}],MulTap[{}{}{}],LongPress[{}{}{}],RepeatCnt[{}{}{}],TapCnt[{}{}{}]",
			s_KeyToStrMap.at(keyCode),
			data.m_InputState.m_Press ? green : red, data.m_InputState.m_Press, reset,
			data.m_InputState.m_Release ? green : red, data.m_InputState.m_Release, reset,
			data.m_InputState.m_Hold ? green : red, data.m_InputState.m_Hold, reset,
			data.m_InputState.m_Repeat ? green : red, data.m_InputState.m_Repeat, reset,
			data.m_InputState.m_Tap ? green : red, data.m_InputState.m_Tap, reset,
			data.m_InputState.m_MultiTap ? green : red, data.m_InputState.m_MultiTap, reset,
			data.m_InputState.m_LongPress ? green : red, data.m_InputState.m_LongPress, reset,
			yellow, data.m_RepeatCount, reset,
			yellow, data.m_TapCount, reset);
	}

	void InputHandler::LogMouseEvents(const MouseButton& mouseButton, const InputData& data)
	{
		const char* red = "\x1b[31m";
		const char* green = "\x1b[32m";
		const char* yellow = "\x1b[33m";
		const char* reset = "\x1b[0m";
		RD_CORE_TRACE("{}: Press[{}{}{}],Release[{}{}{}],Hold[{}{}{}],Tap[{}{}{}],MulTap[{}{}{}],LongPress[{}{}{}],TapCnt[{}{}{}]",
			s_ButtonToStrMap.at(mouseButton),
			data.m_InputState.m_Press ? green : red, data.m_InputState.m_Press, reset,
			data.m_InputState.m_Release ? green : red, data.m_InputState.m_Release, reset,
			data.m_InputState.m_Hold ? green : red, data.m_InputState.m_Hold, reset,
			data.m_InputState.m_Tap ? green : red, data.m_InputState.m_Tap, reset,
			data.m_InputState.m_MultiTap ? green : red, data.m_InputState.m_MultiTap, reset,
			data.m_InputState.m_LongPress ? green : red, data.m_InputState.m_LongPress, reset,
			yellow, data.m_TapCount, reset);
	}
#endif
}
