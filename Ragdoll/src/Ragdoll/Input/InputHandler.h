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

#include "Ragdoll/Enums.h"
#include "Ragdoll/Event/KeyEvents.h"
#include "Ragdoll/Event/MouseEvent.h"
#include "Ragdoll/Math/Vector2.h"

namespace Ragdoll
{
	struct KeyState
	{
		unsigned int m_Press : 1 { 0 };
		unsigned int m_Release : 1 { 0 };
		unsigned int m_Hold : 1 { 0 };
		unsigned int m_Repeat : 1 { 0 };
		unsigned int m_LongPress : 1 { 0 };
		unsigned int m_Tap : 1 { 0 };
		unsigned int m_MultiTap : 1 { 0 };
		unsigned int m_LongPressTriggered : 1 { 0 };
		unsigned int m_IncrementMultiTapTimer : 1 { 0 };
		unsigned int : 23;
	};

	struct KeyData
	{
		union
		{
			KeyState m_KeyState;
			struct
			{
				unsigned int : 9;
				unsigned int m_TapCount : 23;
			};
		};
		float m_HeldTimer{ 0.f };
		float m_MultiTapTimer{ 0.f };
		unsigned int m_RepeatCount{ 0 };

		KeyData() : m_KeyState{ 0 } {}
		~KeyData() {}
	};

	class InputHandler
	{
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

		void OnKeyPressed(KeyPressedEvent& event);
		void OnKeyReleased(KeyReleasedEvent& event);
		void OnKeyTyped(KeyTypedEvent& event);

		void OnMouseMove(MouseMovedEvent& event);
		void OnMouseButtonPressed(MouseButtonPressedEvent& event);
		void OnMouseButtonReleased(MouseButtonReleasedEvent& event);
		void OnMouseScrolled(MouseScrolledEvent& event);

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

		KeyData m_Keys[static_cast<int>(Key::MaxKey)];

		Vector2 m_MousePos{};
		Vector2 m_LastMousePos{};
		Vector2 m_MouseDeltas{};
		Vector2 m_ScrollDeltas{};
		bool m_ScrollThisFrame{};

#if RD_LOG_INPUT
		void LogInput(const Key& keyCode, const KeyData& data);
#endif
	};
}
