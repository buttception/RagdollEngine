//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//
#include "Utils.hlsli"

Texture2D<float> DepthBuffer : register(t0);
RWTexture2D<float4> VelocityBuffer : register(u0);

cbuffer CBuffer : register(b1)
{
    matrix CurToPrevXForm;
    matrix premult;
    matrix postmult;
    matrix prev;
    matrix curr;
    float2 res;
    float near_p;
}

[numthreads( 8, 8, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 st = DTid.xy;
    float2 CurPixel = st + 0.5;
    float Depth = max(DepthBuffer[st], 0.0001f);

	float3 fragPos = DepthToWorld(Depth, CurPixel / res, curr);
    float4 prevNdcPos = mul(float4(fragPos, 1.f), prev);
    prevNdcPos /= prevNdcPos.w;
    float2 screenPos = ScreenPosToViewportUV(prevNdcPos.xy) * res;
    float2 xyDelta = screenPos - CurPixel;

    VelocityBuffer[st] = float4(xyDelta, near_p / (prevNdcPos.z * 0.5f + 0.5f) - near_p / Depth, 1.f);
}
