/*!
\file		InputHandler.h
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
#pragma once

#include "InputEnums.h"
#include "Ragdoll/Event/KeyEvents.h"
#include "Ragdoll/Event/MouseEvent.h"
#include "Ragdoll/Math/RagdollMath.h"

namespace ragdoll
{
	struct InputState
	{
		uint32_t m_Press : 1 { 0 };
		uint32_t m_Release : 1 { 0 };
		uint32_t m_Hold : 1 { 0 };
		uint32_t m_Repeat : 1 { 0 };
		uint32_t m_LongPress : 1 { 0 };
		uint32_t m_Tap : 1 { 0 };
		uint32_t m_MultiTap : 1 { 0 };
		uint32_t m_LongPressTriggered : 1 { 0 };
		uint32_t m_IncrementMultiTapTimer : 1 { 0 };
		uint32_t : 23;
	};

	struct InputData
	{
		union
		{
			InputState m_InputState;
			struct
			{
				uint32_t : 9;
				uint32_t m_TapCount : 23;
			};
		};
		float m_HeldTimer{ 0.f };
		float m_MultiTapTimer{ 0.f };
		uint32_t m_RepeatCount{ 0 };

		InputData() : m_InputState{ 0 } {}
		~InputData() {}
	};

	class InputHandler
	{
		friend class Application;
	public:
		void Init();
		void Update(double _dt);
		void Shutdown();

		const static Key GetKeyFromChar(char _key) { return s_CharToKeyMap.at(_key); }
		const static char GetCharFromKey(Key _key) { return s_KeyToCharMap.at(_key); }
		const static Key GetKeyFromString(const char* _key) { return s_StrToKeyMap.at(_key); }
		const static char* GetStringFromKey(Key _key) { return s_KeyToStrMap.at(_key); }
		const static MouseButton GetButtonFromString(const char* _button) { return s_StrToButtonMap.at(_button); }
		const static char* GetStringFromButton(MouseButton _button) { return s_ButtonToStrMap.at(_button); }

		//functions for getting key states
		bool IsKeyPressed(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_Press; }
		bool IsKeyDown(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_Hold; }
		bool IsKeyReleased(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_Release; }
		bool IsKeyRepeated(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_Repeat; }
		uint32_t GetKeyRepeatCount(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_RepeatCount; }
		bool IsKeyLongPressed(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_LongPress; }
		bool IsKeyTapped(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_Tap; }
		bool IsKeyMultiTap(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_InputState.m_MultiTap; }
		uint32_t GetKeyTapCount(Key _key) const { return m_Keys[static_cast<int32_t>(_key)].m_TapCount; }

		//functions for getting mouse states
		bool IsMouseButtonPressed(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_InputState.m_Press; }
		bool IsMouseButtonDown(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_InputState.m_Hold; }
		bool IsMouseButtonReleased(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_InputState.m_Release; }
		bool IsMouseButtonLongPressed(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_InputState.m_LongPress; }
		bool IsMouseButtonTapped(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_InputState.m_Tap; }
		bool IsMouseButtonMultiTap(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_InputState.m_MultiTap; }
		uint32_t GetMouseButtonTapCount(MouseButton _button) const { return m_MouseButtons[static_cast<int32_t>(_button)].m_TapCount; }

		glm::ivec2 GetMousePosition() const { return m_MousePos; }
		glm::ivec2 GetMouseDelta() const { return m_MouseDeltas; }
		glm::ivec2 GetScrollDelta() const { return m_ScrollDeltas; }
		bool IsScrollThisFrame() const { return m_ScrollThisFrame; }

	private:
		const static std::unordered_map<char, Key> s_CharToKeyMap;
		const static std::unordered_map<Key, char> s_KeyToCharMap;

		const static std::unordered_map<const char*, Key> s_StrToKeyMap;
		const static std::unordered_map<Key, const char*> s_KeyToStrMap;

		const static std::unordered_map<const char*, MouseButton> s_StrToButtonMap;
		const static std::unordered_map<MouseButton, const char*> s_ButtonToStrMap;

		inline const static float s_LongPressTime = 0.5f;
		inline const static float s_MultiTapTime = 0.2f;
		inline const static float s_TapTime = 0.3f;

		InputData m_Keys[static_cast<int>(Key::MaxKey)];
		InputData m_MouseButtons[static_cast<int>(MouseButton::MaxButton)];

		glm::ivec2 m_MousePos{};
		glm::ivec2 m_LastMousePos{};
		glm::ivec2 m_MouseDeltas{};
		glm::ivec2 m_ScrollDeltas{};
		bool m_ScrollThisFrame{};

		//functions to update states based on the event
		void OnKeyPressed(KeyPressedEvent& event);
		void OnKeyReleased(KeyReleasedEvent& event);
		void OnKeyTyped(KeyTypedEvent& event);

		void OnMouseMove(MouseMovedEvent& event);
		void OnMouseButtonPressed(MouseButtonPressedEvent& event);
		void OnMouseButtonReleased(MouseButtonReleasedEvent& event);
		void OnMouseScrolled(MouseScrolledEvent& event);

		//helpers for updating key states
		void UpdateDataStatesAndTimers(Key _key, double _dt);
		void UpdateDataStatesAndTimers(MouseButton _mousebtn, double _dt);
		void UpdateDataStatesAndTimers(InputData& _data, double _dt);

#if RD_LOG_INPUT
		void LogKeyEvents(const Key& keyCode, const InputData& data);
		void LogMouseEvents(const MouseButton& mouseButton, const InputData& data);
#endif
	};
}
