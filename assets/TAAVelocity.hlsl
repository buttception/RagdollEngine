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
    float Depth = max(DepthBuffer[st], 0.0001f);

#if 0
	float3 fragPos = DepthToWorld(Depth, CurPixel / res, viewProjInverse);
    float4 prevNdcPos = mul(float4(fragPos, 1.f), prevViewProj);
    prevNdcPos /= prevNdcPos.w;
    float2 screenPos = ScreenPosToViewportUV(prevNdcPos.xy) * res;
    float2 xyDelta = screenPos - CurPixel;
    float prevDepth = prevNdcPos.z;
    VelocityBuffer[st] = float4(xyDelta, near_p / prevDepth - near_p / Depth, 1.f);
#endif

    float4 HPos = float4( CurPixel, Depth, 1.0 );
    float4 PrevHPos = mul( HPos , CurToPrevXForm );
    PrevHPos.xyz /= PrevHPos.w;
    PrevHPos.y = res.y - PrevHPos.y;
    VelocityBuffer[st] = float4(PrevHPos.xyz - float3(CurPixel, Depth), 1.f);

#if 0
    float4 test = HPos;
    VelocityBuffer[st] = float4(test.xyz, 1.f);
    test = mul(test, premult);
    VelocityBuffer[st] = float4(test.xyz, 1.f);
    test = mul(test, viewProjInverse);
    VelocityBuffer[st] = float4(test.xyz, 1.f);
    test = mul(test, prevViewProj);
    VelocityBuffer[st] = float4(test.xyz, 1.f);
    test = mul(test, postmult);
    VelocityBuffer[st] = float4(test.xyz, 1.f);
    test /= test.w;
    VelocityBuffer[st] = float4(test.xyz, 1.f);
#endif
}
