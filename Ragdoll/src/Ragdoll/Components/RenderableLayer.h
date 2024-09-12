#pragma once
#include "RenderableComp.h"
#include "Ragdoll/Math/RagdollMath.h"

struct Proxy {
	//2 4x4 float matrix i feel is quite heavy, check if this is the norms
	Matrix ModelToWorld;
	Matrix InvModelToWorld;	//hlsl cannot compute the inverse
	int32_t TextureIndex{ -1 };	//in the future for instancing with textures

};
namespace ragdoll {
	class EntityManager;
	class RenderableSystem 
	{
	public:
		RenderableSystem(std::shared_ptr<EntityManager> reg);

	private:
		std::shared_ptr<EntityManager> m_EntityManager;

		std::vector<Proxy> Proxies;
	};
}