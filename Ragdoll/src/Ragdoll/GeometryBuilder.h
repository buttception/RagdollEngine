#pragma once

#include <nvrhi/nvrhi.h>
#include "Ragdoll/Math/RagdollMath.h"

class GeometryBuilder {
public:
	void Init(nvrhi::DeviceHandle nvrhiDevice);
private:
	nvrhi::DeviceHandle Device;
};