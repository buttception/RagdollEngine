﻿/*!
\file		EntityManager.cpp
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

#include "ragdollpch.h"

#include "EntityManager.h"

#include "Ragdoll/Core/Logger.h"
#include "Ragdoll/Core/Guid.h"

namespace ragdoll
{
	entt::entity EntityManager::CreateEntity()
	{
		entt::entity entity = m_Registry.create();
		auto guid = GuidGenerator::Generate();
		m_GuidToEntity.insert({ guid, entity });
		m_EntityToGuid.insert({ entity, guid });
		return entity;
	}

	entt::entity EntityManager::GetEntity(const Guid& guid)
	{
		if(m_GuidToEntity.find(guid) != m_GuidToEntity.end())
			return m_GuidToEntity[guid];
		RD_CORE_WARN("Entity {} does not exist, returning entt::null");
		return entt::null;
	}

	Guid EntityManager::GetGuid(const entt::entity& entity)
	{
		if(m_EntityToGuid.find(entity) != m_EntityToGuid.end())
			return m_EntityToGuid[entity];
		RD_CORE_WARN("Entity does not exist, returning Guid::null");
		return 0;
	}
}
