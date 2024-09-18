#include "ragdollpch.h"
#include "Octree.h"

uint32_t Octree::TotalProxies = 0;

void Octree::Init()
{
	DirectX::BoundingBox::CreateFromPoints(Octant.Box, -Max, Max);
	//create the first 8 octants
	Octant.Subdivide();
}

bool InsertProxy(Octant& octant, Proxy proxy) {
	//check if within this octant first
	DirectX::ContainmentType result = octant.Box.Contains(proxy.BoundingBox);
	if (result == DirectX::ContainmentType::INTERSECTS){
		//intersecting with the current octant, add to parent
		RD_ASSERT(octant.Parent == nullptr, "Bounding Box exceeds than origin octant, consider increasing octree size");
		octant.Parent->Proxies.push_back(proxy);
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
				if (InsertProxy(octant.Octants[i], proxy)) {
					//insertion is success, can just back out
					return true;
				}
			}
		}
		else { //no children can just add in
			octant.Proxies.push_back(proxy);
			Octree::TotalProxies++;
			octant.CheckToSubdivide();
			//return true because inserted
			return true;
		}
	}
	return false;
}

void Octree::AddProxy(Proxy proxy)
{
	//add the proxy into the octree
	InsertProxy(Octant, proxy);
}

void Octree::Clear()
{
	Octant.Octants.clear();
	Octant.Proxies.clear();
	Octree::TotalProxies = 0;
	Init();
}

Octant::~Octant()
{
	Octants.clear();
	Parent = nullptr;
	Proxies.clear();
}

void Octant::CheckToSubdivide()
{
	if (Proxies.size() >= DivisionCriteria)
		Subdivide();
}

void Octant::Subdivide()
{
	RD_ASSERT(Octants.size() != 0, "Octant has already been subdivided");
	for (int i = 0; i < 8; ++i)
	{
		Octant oct;
		oct.Parent = this;
		DirectX::BoundingBox::CreateFromPoints(oct.Box, Box.Center + Box.Extents * OctantMinOffsetsScalars[i], Box.Center + Box.Extents * OctantMaxOffsetsScalars[i]);
		Octants.push_back(oct);
	}
	//insert nodes into children
	//copy the current proxies
	std::vector<Proxy> copy = Proxies;
	Octree::TotalProxies -= Proxies.size();
	Proxies.clear();
	for (const Proxy& proxy : copy)
	{
		InsertProxy(*this, proxy);
	}
}
