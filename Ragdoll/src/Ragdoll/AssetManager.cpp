#include "ragdollpch.h"

#include "AssetManager.h"
#include "ForwardRenderer.h"
#include "DirectXDevice.h"

AssetManager* AssetManager::GetInstance()
{
	if(!s_Instance)
	{
		s_Instance = std::make_unique<AssetManager>();
	}
	return s_Instance.get();
}

uint32_t AssetManager::AddVertices(const std::vector<Vertex>& newVertices, const std::vector<uint32_t>& newIndices)
{
	uint32_t vCurrOffset = Vertices.size();
	uint32_t iCurrOffset = Indices.size();
	Vertices.resize(Vertices.size() + newVertices.size());
	Indices.resize(Indices.size() + newIndices.size());
	memcpy(Vertices.data() + vCurrOffset, newVertices.data(), newVertices.size() * sizeof(Vertex));
	memcpy(Indices.data() + iCurrOffset, newIndices.data(), newIndices.size() * sizeof(uint32_t));
	//create the vertexbuffer info
	VertexBufferInfo info;
	info.VBOffset = vCurrOffset;
	info.IBOffset = iCurrOffset;
	info.VerticesCount = newVertices.size();
	info.IndicesCount = newIndices.size();
	VertexBufferInfos.emplace_back(info);
	return VertexBufferInfos.size() - 1;
}

void AssetManager::UpdateVBOIBO(Renderer* renderer)
{
	nvrhi::BufferDesc vertexBufDesc;
	vertexBufDesc.byteSize = Vertices.size() * sizeof(Vertex);	//the offset is already the size of the vb
	vertexBufDesc.isVertexBuffer = true;
	vertexBufDesc.debugName = "Global vertex buffer";
	vertexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;	//set as copy dest to copy over data
	//smth smth syncrhonization need to be this state to be written

	VBO = renderer->Device->m_NvrhiDevice->createBuffer(vertexBufDesc);
	//copy data over
	renderer->CommandList->beginTrackingBufferState(VBO, nvrhi::ResourceStates::CopyDest);	//i tink this is to update nvrhi resource manager state tracker
	renderer->CommandList->writeBuffer(VBO, Vertices.data(), vertexBufDesc.byteSize);
	renderer->CommandList->setPermanentBufferState(VBO, nvrhi::ResourceStates::VertexBuffer);	//now its a vb

	nvrhi::BufferDesc indexBufDesc;
	indexBufDesc.byteSize = Indices.size() * sizeof(uint32_t);
	indexBufDesc.isIndexBuffer = true;
	indexBufDesc.debugName = "Global index buffer";
	indexBufDesc.initialState = nvrhi::ResourceStates::CopyDest;

	IBO = renderer->Device->m_NvrhiDevice->createBuffer(indexBufDesc);
	renderer->CommandList->beginTrackingBufferState(IBO, nvrhi::ResourceStates::CopyDest);
	renderer->CommandList->writeBuffer(IBO, Indices.data(), indexBufDesc.byteSize);
	renderer->CommandList->setPermanentBufferState(IBO, nvrhi::ResourceStates::IndexBuffer);
}
