#pragma once

#include <nvrhi/nvrhi.h>
#include "Ragdoll/Math/RagdollMath.h"
#include "AssetManager.h"
#include "ForwardRenderer.h"

class GeometryBuilder {
public:
	void Init(nvrhi::DeviceHandle nvrhiDevice);
	
	Mesh BuildCube(float size, int32_t matIndex);
	Mesh BuildSphere(float diameter, uint32_t tessellation, int32_t matIndex);
	Mesh BuildCylinder(float height, float diameter, size_t tessellation, int32_t matIndex);
	Mesh BuildCone(float diameter, float height, size_t tessellation, int32_t matIndex);
	Mesh BuildIcosahedron(float size, int32_t matIndex);
private:
	nvrhi::DeviceHandle Device;
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	nvrhi::CommandListHandle CommandList;

	Mesh BuildMesh(std::string debugName, int32_t matIndex);
};