#include "ragdollpch.h"
#include "ImguiInterface.h"

void ImguiInterface::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	CommandList = m_DirectXTest.m_NvrhiDevice->createCommandList();
	CommandList->open();
	RD_ASSERT(m_DirectXTest.ImguiVertexShader == nullptr || m_DirectXTest.ImguiPixelShader == nullptr, "Failed to load Imgui shaders");
	// create attribute layout object
	nvrhi::VertexAttributeDesc vertexAttribLayout[] = {
		{ "POSITION", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,pos), sizeof(ImDrawVert), false },
		{ "TEXCOORD", nvrhi::Format::RG32_FLOAT,  1, 0, offsetof(ImDrawVert,uv),  sizeof(ImDrawVert), false },
		{ "COLOR",    nvrhi::Format::RGBA8_UNORM, 1, 0, offsetof(ImDrawVert,col), sizeof(ImDrawVert), false },
	};
	//creating the layout for shader
	ShaderAttribLayout = m_DirectXTest.m_NvrhiDevice->createInputLayout(vertexAttribLayout, _countof(vertexAttribLayout), m_DirectXTest.ImguiVertexShader);

	io.Fonts->AddFontDefault();

	CreateFontTexture();

	//create the imgui pipeline state
	nvrhi::BlendState blendState;
	blendState.targets[0].blendEnable = true;
	blendState.targets[0].srcBlend = nvrhi::BlendFactor::SrcAlpha;
	blendState.targets[0].destBlend = nvrhi::BlendFactor::InvSrcAlpha;
	blendState.targets[0].srcBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
	blendState.targets[0].destBlendAlpha = nvrhi::BlendFactor::Zero;

	nvrhi::RasterState rasterState = nvrhi::RasterState();
	rasterState.fillMode = nvrhi::RasterFillMode::Solid;
	rasterState.cullMode = nvrhi::RasterCullMode::None;
	rasterState.scissorEnable = true;
	rasterState.depthClipEnable = true;

	nvrhi::DepthStencilState depthStencilState = nvrhi::DepthStencilState();
	depthStencilState.depthTestEnable = false;
	depthStencilState.depthWriteEnable = true;
	depthStencilState.stencilEnable = false;
	depthStencilState.depthFunc = nvrhi::ComparisonFunc::Always;

	nvrhi::RenderState renderState = nvrhi::RenderState();
	renderState.blendState = blendState;
	renderState.rasterState = rasterState;
	renderState.depthStencilState = depthStencilState;
	//why do i keep default constructing when it is already implicit
	nvrhi::BindingLayoutDesc layoutDesc = nvrhi::BindingLayoutDesc();
	//specify the resource is avail on all shader process
	layoutDesc.visibility = nvrhi::ShaderType::All;
	layoutDesc.bindings = {
		//bind slot 0 with 2 floats
		nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 2),
		//binds slot 0 with shader resource view, srv is used to read data from textures
		nvrhi::BindingLayoutItem::Texture_SRV(0),
		//binds slot 0 with sampler, controls how the texture is sampled
		nvrhi::BindingLayoutItem::Sampler(0),
	};
	BindingLayout = m_DirectXTest.m_NvrhiDevice->createBindingLayout(layoutDesc);

	BasePSODesc.primType = nvrhi::PrimitiveType::TriangleList;
	BasePSODesc.inputLayout = ShaderAttribLayout;
	BasePSODesc.VS = m_DirectXTest.ImguiVertexShader;
	BasePSODesc.PS = m_DirectXTest.ImguiPixelShader;
	BasePSODesc.renderState = renderState;
	BasePSODesc.bindingLayouts = { BindingLayout };
	CommandList->close();

	m_DirectXTest.m_NvrhiDevice->executeCommandList(CommandList);
}

void ImguiInterface::BeginFrame()
{
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = m_DirectXTest.m_PrimaryWindow->GetFrameTime();
	io.MouseDrawCursor = false;
	io.DisplaySize.x = m_DirectXTest.m_PrimaryWindow->GetBufferWidth();
	io.DisplaySize.y = m_DirectXTest.m_PrimaryWindow->GetBufferHeight();
	
	ImGui::NewFrame();
}

void ImguiInterface::Render()
{
	ImGui::Render();

	ImDrawData* drawData = ImGui::GetDrawData();
	const auto& io = ImGui::GetIO();

	CommandList->open();
	CommandList->beginMarker("ImGUI");

	if (!updateGeometry(CommandList))
	{
		return;
	}

	// handle DPI scaling
	drawData->ScaleClipRects(io.DisplayFramebufferScale);

	float invDisplaySize[2] = { 1.f / io.DisplaySize.x, 1.f / io.DisplaySize.y };

	// set up graphics state
	nvrhi::GraphicsState drawState;

	nvrhi::TextureHandle tex = m_DirectXTest.m_RhiSwapChainBuffers[m_DirectXTest.m_SwapChain->GetCurrentBackBufferIndex()];
	auto fbDesc = nvrhi::FramebufferDesc()
		.addColorAttachment(tex);
	nvrhi::FramebufferHandle pipelineFb = m_DirectXTest.m_NvrhiDevice->createFramebuffer(fbDesc);
	drawState.framebuffer = pipelineFb;
	assert(drawState.framebuffer);

	drawState.pipeline = getPSO(drawState.framebuffer);

	drawState.viewport.viewports.push_back(nvrhi::Viewport(io.DisplaySize.x * io.DisplayFramebufferScale.x,
		io.DisplaySize.y * io.DisplayFramebufferScale.y));
	drawState.viewport.scissorRects.resize(1);  // updated below

	nvrhi::VertexBufferBinding vbufBinding;
	vbufBinding.buffer = vertexBuffer;
	vbufBinding.slot = 0;
	vbufBinding.offset = 0;
	drawState.vertexBuffers.push_back(vbufBinding);

	drawState.indexBuffer.buffer = indexBuffer;
	drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
	drawState.indexBuffer.offset = 0;

	// render command lists
	int vtxOffset = 0;
	int idxOffset = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		for (int i = 0; i < cmdList->CmdBuffer.Size; i++)
		{
			const ImDrawCmd* pCmd = &cmdList->CmdBuffer[i];

			if (pCmd->UserCallback)
			{
				pCmd->UserCallback(cmdList, pCmd);
			}
			else {
				nvrhi::BindingSetDesc desc;
				desc.bindings = {
					nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
					nvrhi::BindingSetItem::Texture_SRV(0, (nvrhi::ITexture*)pCmd->TextureId),
					nvrhi::BindingSetItem::Sampler(0, FontSampler)
				};

				nvrhi::BindingSetHandle binding = m_DirectXTest.m_NvrhiDevice->createBindingSet(desc, BindingLayout);
				assert(binding);
				drawState.bindings = { binding };
				//drawState.bindings = { getBindingSet((nvrhi::ITexture*)pCmd->TextureId) };
				assert(drawState.bindings[0]);

				drawState.viewport.scissorRects[0] = nvrhi::Rect(int(pCmd->ClipRect.x),
					int(pCmd->ClipRect.z),
					int(pCmd->ClipRect.y),
					int(pCmd->ClipRect.w));

				nvrhi::DrawArguments drawArguments;
				drawArguments.vertexCount = pCmd->ElemCount;
				drawArguments.startIndexLocation = idxOffset;
				drawArguments.startVertexLocation = vtxOffset;

				CommandList->setGraphicsState(drawState);
				CommandList->setPushConstants(invDisplaySize, sizeof(invDisplaySize));
				CommandList->drawIndexed(drawArguments);
			}

			idxOffset += pCmd->ElemCount;
		}

		vtxOffset += cmdList->VtxBuffer.Size;
	}

	CommandList->endMarker();
	CommandList->close();
	m_DirectXTest.m_NvrhiDevice->executeCommandList(CommandList);
}

void ImguiInterface::BackbufferResizing()
{
	pso = nullptr;
}

void ImguiInterface::Shutdown()
{
}

bool ImguiInterface::reallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
{
	if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
	{
		nvrhi::BufferDesc desc;
		desc.byteSize = uint32_t(reallocateSize);
		desc.structStride = 0;
		desc.debugName = isIndexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
		desc.canHaveUAVs = false;
		desc.isVertexBuffer = !isIndexBuffer;
		desc.isIndexBuffer = isIndexBuffer;
		desc.isDrawIndirectArgs = false;
		desc.isVolatile = false;
		desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
		desc.keepInitialState = true;

		buffer = m_DirectXTest.m_NvrhiDevice->createBuffer(desc);

		RD_ASSERT(buffer == nullptr, "Imgui buffer resize fail");
		if (!buffer)
		{

			return false;
		}
	}

	return true;
}

nvrhi::IGraphicsPipeline* ImguiInterface::getPSO(nvrhi::IFramebuffer* fb)
{
	if (pso)
		return pso;

	pso = m_DirectXTest.m_NvrhiDevice->createGraphicsPipeline(BasePSODesc, fb);
	assert(pso);

	return pso;
}

nvrhi::IBindingSet* ImguiInterface::getBindingSet(nvrhi::ITexture* texture)
{
	auto iter = bindingsCache.find(texture);
	if (iter != bindingsCache.end())
	{
		return iter->second;
	}

	nvrhi::BindingSetDesc desc;

	desc.bindings = {
		nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 2),
		nvrhi::BindingSetItem::Texture_SRV(0, texture),
		nvrhi::BindingSetItem::Sampler(0, FontSampler)
	};

	nvrhi::BindingSetHandle binding;
	binding = m_DirectXTest.m_NvrhiDevice->createBindingSet(desc, BindingLayout);
	assert(binding);

	bindingsCache[texture] = binding;
	return binding;
}

bool ImguiInterface::updateGeometry(nvrhi::ICommandList* commandList)
{
	ImDrawData* drawData = ImGui::GetDrawData();

	// create/resize vertex and index buffers if needed
	if (!reallocateBuffer(vertexBuffer,
		drawData->TotalVtxCount * sizeof(ImDrawVert),
		(drawData->TotalVtxCount + 5000) * sizeof(ImDrawVert),
		false))
	{
		return false;
	}

	if (!reallocateBuffer(indexBuffer,
		drawData->TotalIdxCount * sizeof(ImDrawIdx),
		(drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
		true))
	{
		return false;
	}

	vtxBuffer.resize(vertexBuffer->getDesc().byteSize / sizeof(ImDrawVert));
	idxBuffer.resize(indexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

	// copy and convert all vertices into a single contiguous buffer
	ImDrawVert* vtxDst = &vtxBuffer[0];
	ImDrawIdx* idxDst = &idxBuffer[0];

	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];

		memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

		vtxDst += cmdList->VtxBuffer.Size;
		idxDst += cmdList->IdxBuffer.Size;
	}

	commandList->writeBuffer(vertexBuffer, &vtxBuffer[0], vertexBuffer->getDesc().byteSize);
	commandList->writeBuffer(indexBuffer, &idxBuffer[0], indexBuffer->getDesc().byteSize);

	return true;
}

void ImguiInterface::CreateFontTexture()
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	{
		nvrhi::TextureDesc desc;
		desc.width = width;
		desc.height = height;
		desc.format = nvrhi::Format::RGBA8_UNORM;
		desc.debugName = "ImGui Font Texture";

		FontTexture = m_DirectXTest.m_NvrhiDevice->createTexture(desc);

		//specify the texture data state for the gpu?
		CommandList->beginTrackingTextureState(FontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);

		RD_ASSERT(FontTexture == nullptr, "Failed to create font texture for imgui");

		//draws to the new texture
		//nvhri already deal with allocation
		CommandList->writeTexture(FontTexture, 0, 0, pixels, width * 4);

		CommandList->setPermanentTextureState(FontTexture, nvrhi::ResourceStates::ShaderResource);
		CommandList->commitBarriers();

		io.Fonts->TexID = FontTexture;

		//create the font sampler
		nvrhi::SamplerDesc samplerDesc = nvrhi::SamplerDesc();
		samplerDesc.addressU = samplerDesc.addressV = samplerDesc.addressW = nvrhi::SamplerAddressMode::Wrap;
		samplerDesc.minFilter = samplerDesc.magFilter = samplerDesc.mipFilter = true;
		FontSampler = m_DirectXTest.m_NvrhiDevice->createSampler(samplerDesc);
		RD_ASSERT(FontSampler == nullptr, "Failed to create font sampler for imgui");

	}
}
