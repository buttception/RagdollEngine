#include "ragdollpch.h"
#include "Octree.h"

uint32_t Octree::TotalProxies = 0;
uint32_t Octree::MaxDepth = 0;

void Octree::Init()
{
	DirectX::BoundingBox::CreateFromPoints(Octant.Box, Min, Max);
	//create the first 8 octants
	Octant.Subdivide();
}

bool InsertProxy(Octant& octant, const DirectX::BoundingBox& box, uint32_t index) {
	//check if within this octant first
	DirectX::ContainmentType result = octant.Box.Contains(box);
	if (result == DirectX::ContainmentType::INTERSECTS){
		//intersecting with the current octant, add to parent
		RD_ASSERT(octant.Parent == nullptr, "Bounding Box exceeds than origin octant, consider increasing octree size");
		octant.Parent->Nodes.emplace_back(box, index);
		Octree::TotalProxies++;
		return true;
	}
	else if (result == DirectX::ContainmentType::CONTAINS) {
		//contain fully, check if got children first
		if (octant.Octants.size() != 0)
		{
			//recursively call to children
			for (int i = 0; i < 8; ++i)
			{
				if (InsertProxy(octant.Octants[i], box, index)) {
					//insertion is success, can just back out
					return true;
				}
			}
		}
		else { //no children can just add in
			octant.Nodes.emplace_back(box, index);
			Octree::TotalProxies++;
			octant.CheckToSubdivide();
			//return true because inserted
			return true;
		}
	}
	return false;
}

void Octree::AddProxy(const DirectX::BoundingBox& box, uint32_t index)
{
	//add the proxy into the octree
	InsertProxy(Octant, box, index);
}

void Octree::Clear()
{
	Octant.Octants.clear();
	Octant.Nodes.clear();
	Octree::TotalProxies = 0;
	Octree::MaxDepth = 0;
	Init();
}

Octant::~Octant()
{
	Octants.clear();
	Parent = nullptr;
	Nodes.clear();
}

void Octant::CheckToSubdivide()
{
	if (Nodes.size() >= DivisionCriteria)
		Subdivide();
}

void Octant::Subdivide()
{
	RD_ASSERT(Octants.size() != 0, "Octant has already been subdivided");
	for (int i = 0; i < 8; ++i)
	{
		Octant oct;
		oct.Parent = this;
		oct.Level = Level + 1;
		if(Octree::MaxDepth < oct.Level)
			Octree::MaxDepth = oct.Level;
		DirectX::BoundingBox::CreateFromPoints(oct.Box, Box.Center + Box.Extents * OctantMinOffsetsScalars[i], Box.Center + Box.Extents * OctantMaxOffsetsScalars[i]);
		Octants.push_back(oct);
	}
	//insert nodes into children
	//copy the current proxies
	std::vector<Node> copy = Nodes;
	Octree::TotalProxies -= Nodes.size();
	Nodes.clear();
	for (const Node& node : copy)
	{
		InsertProxy(*this, node.Box, node.Index);
	}
}
