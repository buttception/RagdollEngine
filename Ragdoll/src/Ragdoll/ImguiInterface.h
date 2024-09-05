#pragma once
#include "DirectXTest.h"
#include <imgui.h>

class ImguiInterface {
public:
	nvrhi::CommandListHandle CommandList;
	nvrhi::SamplerHandle FontSampler;
	nvrhi::TextureHandle FontTexture;
	nvrhi::InputLayoutHandle ShaderAttribLayout;
	nvrhi::BindingLayoutHandle BindingLayout;
	nvrhi::GraphicsPipelineDesc BasePSODesc;

	nvrhi::GraphicsPipelineHandle PSO;
	std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> BindingsCache;

	nvrhi::BufferHandle VertexBufferHandle;
	nvrhi::BufferHandle IndexBufferHandle;
	std::vector<ImDrawVert> VertexBufferRaw;
	std::vector<ImDrawIdx> IndexBufferRaw;

	void Init(DirectXTest* dx);
	void BeginFrame();
	void Render();
	void BackbufferResizing();
	void Shutdown();
private:
	DirectXTest* m_DirectXTest;

	bool ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
	nvrhi::IGraphicsPipeline* GetPSO(nvrhi::IFramebuffer* fb);
	nvrhi::IBindingSet* GetBindingSet(nvrhi::ITexture* texture);
	bool UpdateGeometry(nvrhi::ICommandList* commandList);

	void CreateFontTexture();
};