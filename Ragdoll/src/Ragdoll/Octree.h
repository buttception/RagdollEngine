#pragma once
#include "entt/entt.hpp"

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
	struct Node {
		DirectX::BoundingBox Box;
		uint32_t Index;
	};

	~Octant();

	static constexpr uint32_t DivisionCriteria{ 20 };
	std::vector<Node> Nodes;
	std::vector<Octant> Octants;
	Octant* Parent{ nullptr };
	DirectX::BoundingBox Box;
	bool bIsCulled{ false };
	uint32_t Level{ 0 };

	void CheckToSubdivide();
	void Subdivide();
};

struct Octree {
	inline static Vector3 Max{ 100.f, 100.f, 100.f };
	inline static Vector3 Min{ -100.f, -100.f, -100.f };
	static uint32_t TotalProxies;
	static uint32_t MaxDepth;
	Octant Octant;

	void Init();
	void AddProxy(const DirectX::BoundingBox& box, uint32_t index);
	void Clear();
};