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
RWTexture2D<float3> VelocityBuffer : register(u0);

cbuffer CBuffer : register(b1)
{
    matrix CurToPrevXForm;
    matrix premult;
    matrix postmult;
    //infinite z reverse z matrices
    matrix prevViewProj;
    matrix viewProjInverse;
    float2 res;
    //the near plane
    float near_p;
    float JitterX;
    float JitterY;
}

[numthreads( 8, 8, 1 )]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 st = DTid.xy;
    float2 CurPixel = st + 0.5f.xx;
    float Depth = DepthBuffer[st];

    float4 HPos = float4( CurPixel, Depth, 1.0 );
    float4 PrevHPos = mul( HPos , CurToPrevXForm );
    PrevHPos.xyz /= PrevHPos.w;
    PrevHPos.y = res.y - PrevHPos.y;
    VelocityBuffer[st] = PrevHPos.xyz - float3(CurPixel, Depth);

#if 0
	float3 fragPos = DepthToWorld(Depth, CurPixel / res, viewProjInverse);
    float4 prevNdcPos = mul(float4(fragPos, 1.f), prevViewProj);
    prevNdcPos /= prevNdcPos.w;
    float2 screenPos = ScreenPosToViewportUV(prevNdcPos.xy) * res;
    float2 xyDelta = screenPos - CurPixel;
    float prevDepth = prevNdcPos.z;
    VelocityBuffer[st] = float3(xyDelta, near_p / prevDepth - near_p / Depth);
#endif

#if 0
    float4 test = HPos;
    VelocityBuffer[st] = test.xyz;
    test = mul(test, premult);
    VelocityBuffer[st] = test.xyz;
    test = mul(test, viewProjInverse);
    VelocityBuffer[st] = test.xyz;
    test = mul(test, prevViewProj);
    VelocityBuffer[st] = test.xyz;
    test = mul(test, postmult);
    VelocityBuffer[st] = test.xyz;
    test /= test.w;
    VelocityBuffer[st] = test.xyz;
#endif
}
