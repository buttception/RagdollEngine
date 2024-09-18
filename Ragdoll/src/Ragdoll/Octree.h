#pragma once
#include "entt/entt.hpp"

struct Proxy {
	ENTT_ID_TYPE EnttId;
	int32_t BufferIndex = -1;
	int32_t MaterialIndex = -1;
	DirectX::BoundingBox BoundingBox;
};

struct Octant {
	enum OctantPosition {
		TOP_LEFT_FRONT,
		TOP_RIGHT_FRONT,
		TOP_LEFT_REAR,
		TOP_RIGHT_REAR,
		BOTTOM_LEFT_FRONT,
		BOTTOM_RIGHT_FRONT,
		BOTTOM_LEFT_REAR,
		BOTTOM_RIGHT_REAR,
	};
	//this vectors multiply with the extents + center gives the new points for a subdivided box
	static constexpr Vector3 OctantMinOffsetsScalars[8]{
		{-1.f, 0.f, -1.f},
		{0.f, 0.f, -1.f},
		{-1.f, 0.f, 0.f},
		{0.f, 0.f, 0.f},
		{-1.f, -1.f, -1.f},
		{0.f, -1.f, -1.f},
		{-1.f, -1.f, 0.f},
		{0.f, -1.f, 0.f},
	};
	static constexpr Vector3 OctantMaxOffsetsScalars[8]{
		{0.f, 1.f, 0.f},
		{1.f, 1.f, 0.f},
		{0.f, 1.f, 1.f},
		{1.f, 1.f, 1.f},
		{0.f, 0.f, 0.f},
		{1.f, 0.f, 0.f},
		{0.f, 0.f, 1.f},
		{1.f, 0.f, 1.f},
	};

	~Octant();

	static constexpr uint32_t DivisionCriteria{ 20 };
	std::vector<Proxy> Proxies;
	std::vector<Octant> Octants;
	Octant* Parent{ nullptr };
	DirectX::BoundingBox Box;

	void CheckToSubdivide();
	void Subdivide();
};

struct Octree {

	static constexpr Vector3 Max{ 100.f, 100.f, 100.f };
	static uint32_t TotalProxies;
	Octant Octant;

	void Init();
	void AddProxy(Proxy proxy);
	void Clear();
};