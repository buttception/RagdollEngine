#pragma once
#include <tiny_gltf.h>

struct MaterialComp {
	int32_t AlbedoIndex = -1;
	int32_t NormalIndex = -1;
	int32_t MetallicRoughnessIndex = -1;

	Vector4 Color;
	float Roughness = 1.f;
	float Metallic = 1.f;
};