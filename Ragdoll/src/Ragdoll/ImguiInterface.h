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

	nvrhi::GraphicsPipelineHandle pso;
	std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> bindingsCache;

	nvrhi::BufferHandle vertexBuffer;
	nvrhi::BufferHandle indexBuffer;
	std::vector<ImDrawVert> vtxBuffer;
	std::vector<ImDrawIdx> idxBuffer;

	ImguiInterface(DirectXTest& directXTest) : m_DirectXTest(directXTest) {}

	void Init();
	void BeginFrame();
	void Render();
	void BackbufferResizing();
	void Shutdown();
private:
	DirectXTest& m_DirectXTest;

	bool reallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
	nvrhi::IGraphicsPipeline* getPSO(nvrhi::IFramebuffer* fb);
	nvrhi::IBindingSet* getBindingSet(nvrhi::ITexture* texture);
	bool updateGeometry(nvrhi::ICommandList* commandList);

	void CreateFontTexture();
};