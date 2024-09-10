#include "ragdollpch.h"

#include "AssetManager.h"

AssetManager* AssetManager::GetInstance()
{
	if(!s_Instance)
	{
		s_Instance = std::make_unique<AssetManager>();
	}
	return s_Instance.get();
}
