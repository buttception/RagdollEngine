﻿/*!
\file		EntityManager.h
\date		10/08/2024

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

#include "entt/entt.hpp"

namespace ragdoll
{
	class EntityManager
	{
	public:
		entt::registry& GetRegistry() { return m_Registry; }

		entt::entity CreateEntity();
		entt::entity GetEntity(const Guid& guid);
		Guid GetGuid(const entt::entity& entity);

		template<typename T>
		T* GetComponent(const Guid& guid)
		{
			auto entity = GetEntity(guid);
			if(entity == entt::null)
			{
				RD_CORE_ERROR("Entity in T* GetComponent(const Guid& guid) is entt::null");
				return nullptr;
			}
			if(m_Registry.all_of<T>(entity))
				return &m_Registry.get<T>(entity);
			RD_CORE_ERROR("Entity does not have component {}", typeid(T).name());
			return nullptr;
		}

		template<typename T>
		T* GetComponentOr(const Guid& guid, T* other = nullptr) {
			auto entity = GetEntity(guid);
			if (entity == entt::null)
			{
				RD_CORE_ERROR("Entity in T* GetComponentOr(const Guid& guid, T* other = nullptr) is entt::null");
				return other;
			}
			if (m_Registry.all_of<T>(entity))
				return &m_Registry.get<T>(entity);
			return other;
		}

		template<typename T>
		T* GetComponent(const entt::entity& entity)
		{
			if(entity == entt::null)
			{
				RD_CORE_ERROR("Entity in T* GetComponent(const entt::entity& entity) is entt::null");
				return nullptr;
			}
			if(m_Registry.all_of<T>(entity))
				return &m_Registry.get<T>(entity);
			RD_CORE_ERROR("Entity does not have component {}", typeid(T).name());
			return nullptr;
		}

		template<typename T>
		T* GetComponentOr(const entt::entity& entity, T* other = nullptr) {
			if (entity == entt::null)
			{
				RD_CORE_ERROR("Entity in T* GetComponent(const entt::entity& entity, T* other = nullptr) is entt::null");
				return other;
			}
			if (m_Registry.all_of<T>(entity))
				return &m_Registry.get<T>(entity);
			return other;
		}

		template<typename T>
		T* AddComponent(const Guid& guid)
		{
			auto entity = GetEntity(guid);
			if(entity == entt::null)
				return nullptr;
			if (m_Registry.all_of<T>(entity))
			{
				RD_CORE_ERROR("Entity {} already has component {}, attempted to double add", guid, typeid(T).name());
				return &m_Registry.get<T>(entity);
			}
			return &m_Registry.emplace<T>(entity);

		}

		template<typename T>
		T* AddComponent(const entt::entity& entity)
		{
			if(entity == entt::null)
			{
				RD_CORE_ERROR("Entity in T& AddComponent(const entt::entity& entity) is entt::null");
				return nullptr;
			}
			if(m_Registry.all_of<T>(entity))
			{
				RD_CORE_ERROR("Entity already has component {}, attempted to double add", typeid(T).name());
				return &m_Registry.get<T>(entity);
			}
			return &m_Registry.emplace<T>(entity);
		}

	private:
		entt::registry m_Registry;

		std::unordered_map<uint64_t, entt::entity> m_GuidToEntity;
		std::unordered_map<entt::entity, uint64_t> m_EntityToGuid;
	};
}