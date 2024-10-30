#pragma once

#include <nvrhi/nvrhi.h>
#include "Ragdoll/Math/RagdollMath.h"
#include "AssetManager.h"

class GeometryBuilder {
public:
	void Init(nvrhi::DeviceHandle nvrhiDevice);
	
	size_t BuildCube(float size);
	size_t BuildSphere(float diameter, uint32_t tessellation);
	size_t BuildCylinder(float height, float diameter, uint32_t tessellation);
	size_t BuildCone(float diameter, float height, uint32_t tessellation);
	size_t BuildIcosahedron(float size);
private:
	nvrhi::DeviceHandle Device;
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	nvrhi::CommandListHandle CommandList;
};