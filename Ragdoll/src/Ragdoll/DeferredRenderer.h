#pragma once
#include <nvrhi/nvrhi.h>
#include "Renderer.h"

namespace ragdoll {
	class Window;
	class FileManager;
	class EntityManager;
	struct InstanceGroupInfo;
}
class DirectXDevice;

class DeferredRenderer : public Renderer {
public:
	nvrhi::ShaderHandle ForwardVertexShader;
	nvrhi::ShaderHandle ForwardPixelShader;
	nvrhi::BufferHandle ConstantBuffer;
	nvrhi::TextureHandle DepthBuffer;

	void Init(std::shared_ptr<ragdoll::Window> win, std::shared_ptr<ragdoll::FileManager> fm, std::shared_ptr<ragdoll::EntityManager> em) override;
	void Shutdown() override;

	void BeginFrame() override;
	void DrawAllInstances(nvrhi::BufferHandle instanceBuffer, const std::vector<ragdoll::InstanceGroupInfo>& infos, CBuffer& Cbuf) override;
	void DrawBoundingBoxes(nvrhi::BufferHandle instanceBuffer, uint32_t instanceCount, CBuffer& Cbuf) override;
private:
	//handled at renderer
	void CreateResource();
};