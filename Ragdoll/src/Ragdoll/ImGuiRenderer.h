#pragma once
#include <imgui.h>
#include <nvrhi/nvrhi.h>
class DirectXDevice;
#include "Scene.h"

class ImguiRenderer {
public:
	nvrhi::CommandListHandle CommandList;
	nvrhi::SamplerHandle FontSampler;
	nvrhi::TextureHandle FontTexture;
	nvrhi::InputLayoutHandle ShaderAttribLayout;
	nvrhi::BindingLayoutHandle BindingLayout;
	nvrhi::GraphicsPipelineDesc BasePSODesc;

	nvrhi::GraphicsPipelineHandle PSO;

	nvrhi::BufferHandle VertexBufferHandle;
	nvrhi::BufferHandle IndexBufferHandle;
	std::vector<ImDrawVert> VertexBufferRaw;
	std::vector<ImDrawIdx> IndexBufferRaw;

	Matrix CameraViewProjection;
	Matrix CameraProjection;
	Matrix CameraView;

	void Init(DirectXDevice* dx);
	void BeginFrame();
	bool DrawSpawn(ragdoll::DebugInfo& DebugInfo, ragdoll::SceneInformation& SceneInfo, ragdoll::SceneConfig& Config);
	void DrawControl(ragdoll::DebugInfo& DebugInfo, ragdoll::SceneInformation& SceneInfo, ragdoll::SceneConfig& Config, float _dt);
	int32_t DrawFBViewer();
	void DrawSettings(ragdoll::DebugInfo& DebugInfo, ragdoll::SceneInformation& SceneInfo, ragdoll::SceneConfig& Config);
	void Render();
	void BackbufferResizing();
	void Shutdown();
private:
	DirectXDevice* m_DirectXTest;

	bool ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
	nvrhi::IGraphicsPipeline* GetPSO(nvrhi::IFramebuffer* fb);
	bool UpdateGeometry(nvrhi::ICommandList* commandList);

	void CreateFontTexture();
};