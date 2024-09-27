#pragma once

#include <nvrhi/nvrhi.h>
#include "Ragdoll/Math/RagdollMath.h"
#include "AssetManager.h"

class GeometryBuilder {
public:
	void Init(nvrhi::DeviceHandle nvrhiDevice);
	
	int32_t BuildCube(float size);
	int32_t BuildSphere(float diameter, uint32_t tessellation);
	int32_t BuildCylinder(float height, float diameter, size_t tessellation);
	int32_t BuildCone(float diameter, float height, size_t tessellation);
	int32_t BuildIcosahedron(float size);
private:
	nvrhi::DeviceHandle Device;
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	nvrhi::CommandListHandle CommandList;
};