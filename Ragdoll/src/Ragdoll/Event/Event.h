/*!
\file		Event.h
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

namespace Ragdoll
{
#define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; }\
								virtual EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

	class Event
	{
		friend class EventDispatcher;
	public:
		virtual ~Event() = default;

		bool m_Handled = false;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }
		bool IsInCategory(EventCategory category) const
		{
			return GetCategoryFlags() & category;
		}
	};

	class EventDispatcher
	{
	public:
		EventDispatcher(Event& event)
			: m_Event(event)
		{
		}

		template<typename T>
		using EventFn = std::function<bool(T&)>;
		/**
		 * \brief This function will check if the templated type is the same as the event type
		 * If the event type is the same, the dispatcher will execute the callback function.
		 * \tparam T Type of event
		 * \param func Event callback function
		 * \return True if callback was executed, false if it was not.
		 */
		template<typename T>
		bool Dispatch(EventFn<T> func)
		{
			if (m_Event.GetEventType() == T::GetStaticType())
			{
				if (m_Event.m_Handled)
					return false;
				m_Event.m_Handled = func(*static_cast<T*>(&m_Event));
				return true;
			}
			return false;
		}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}
}

#define RD_CONCAT(a, b) RD_CONCAT_INNER(a, b)
#define RD_CONCAT_INNER(a, b) a ## b

#define RD_DISPATCH_EVENT(dispatcher, eventType, event, callback)\
static auto RD_CONCAT(var, __LINE__) = RD_BIND_EVENT_FN(callback); dispatcher.Dispatch<eventType>(RD_CONCAT(var, __LINE__))