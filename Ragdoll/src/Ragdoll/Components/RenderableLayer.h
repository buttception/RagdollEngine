#pragma once
#include "RenderableComp.h"
#include "Ragdoll/Layer/Layer.h"
#include "Ragdoll/Math/RagdollMath.h"

struct Proxy {
	//for now it is like this

};
namespace ragdoll {
	class EntityManager;
	class RenderableLayer : public Layer {
	public:
		RenderableLayer(std::shared_ptr<EntityManager> reg);
		~RenderableLayer() override = default;

		void Init() override;
		void Update(float _dt) override;
		void Shutdown() override;
	private:
		std::vector<Proxy> Proxies;
	};
}