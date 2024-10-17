#ifndef __INTELLISENSE__    // avoids some pesky intellisense errors
#include "XeGTAO.h"
#include "Utils.hlsli"
#endif

#define VA_SATURATE     saturate
#define VA_MIN          min
#define VA_MAX          max
#define VA_LENGTH       length
#define VA_INLINE       
#define VA_REFERENCE    
#define VA_CONST

// PREDEFINED GLOBAL SAMPLER SLOTS
#define SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT                 10
#define SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT                12

#define CONCATENATE_HELPER(a, b) a##b
#define CONCATENATE(a, b) CONCATENATE_HELPER(a, b)

#define B_CONCATENATER(x) CONCATENATE(b,x)
#define S_CONCATENATER(x) CONCATENATE(s,x)
#define T_CONCATENATER(x) CONCATENATE(t,x)
#define U_CONCATENATER(x) CONCATENATE(u,x)

SamplerState                            g_samplerPointClamp                     : register( S_CONCATENATER( SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT         ) ); 
SamplerState                            g_samplerLinearClamp                     : register( S_CONCATENATER( SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT         ) ); 

cbuffer GTAOConstantBuffer                      : register( b0 )
{
    GTAOConstants               g_GTAOConsts;
}

cbuffer g_Const : register(b1) {
	float4x4 viewMatrix;
    float4x4 InvViewProjMatrix;
    float4x4 prevViewProjMatrix;
    float modulationFactor;
};

//#define XE_GTAO_GENERATE_NORMALS_INPLACE
#include "XeGTAO.hlsli"

// input output textures for the first pass (XeGTAO_PrefilterDepths16x16)
Texture2D<float>            g_srcRawDepth           : register( t0 );   // source depth buffer data (in NDC space in DirectX)
RWTexture2D<lpfloat>        g_outWorkingDepthMIP0   : register( u0 );   // output viewspace depth MIP (these are views into g_srcWorkingDepth MIP levels)
RWTexture2D<lpfloat>        g_outWorkingDepthMIP1   : register( u1 );   // output viewspace depth MIP (these are views into g_srcWorkingDepth MIP levels)
RWTexture2D<lpfloat>        g_outWorkingDepthMIP2   : register( u2 );   // output viewspace depth MIP (these are views into g_srcWorkingDepth MIP levels)
RWTexture2D<lpfloat>        g_outWorkingDepthMIP3   : register( u3 );   // output viewspace depth MIP (these are views into g_srcWorkingDepth MIP levels)
RWTexture2D<lpfloat>        g_outWorkingDepthMIP4   : register( u4 );   // output viewspace depth MIP (these are views into g_srcWorkingDepth MIP levels)

// input output textures for the second pass (XeGTAO_MainPass)
Texture2D<lpfloat>          g_srcWorkingDepth       : register( t0 );   // viewspace depth with MIPs, output by XeGTAO_PrefilterDepths16x16 and consumed by XeGTAO_MainPass
Texture2D<float2>           g_srcNormalmap          : register( t1 );   // source normal map (if used)
Texture2D<uint>             g_srcHilbertLUT         : register( t5 );   // hilbert lookup table  (if any)
RWTexture2D<uint>           g_outWorkingAOTerm      : register( u0 );   // output AO term (includes bent normals if enabled - packed as R11G11B10 scaled by AO)
RWTexture2D<unorm float>    g_outWorkingEdges       : register( u1 );   // output depth-based edges used by the denoiser
RWTexture2D<uint>           g_outNormalmap          : register( u0 );   // output viewspace normals if generating from depth

// input output textures for the third pass (XeGTAO_Denoise)
Texture2D<uint>             g_srcWorkingAOTerm      : register( t0 );   // coming from previous pass
Texture2D<lpfloat>          g_srcWorkingEdges       : register( t1 );   // coming from previous pass
RWTexture2D<uint>           g_outFinalAOTerm        : register( u0 );   // final AO term - just 'visibility' or 'visibility + bent normals'

// composition pass
Texture2D<uint>             g_currAOTerm            : register( t0 );   // coming from previous pass
Texture2D<float>            g_velocityBuffer        : register( t1 );   // velocity buffer
Texture2D<float>            g_depth                 : register( t2 );   // depth buffer
Texture2D<float>            g_accumulationAOTerm    : register( t3 );   // accumulated term
RWTexture2D<float>          g_outAO                 : register( u1 );   // orm

// Engine-specific normal map loader
lpfloat3 LoadNormal( int2 pos )
{
    // special decoding for external normals stored in 11_11_10 unorm - modify appropriately to support your own encoding 
    float2 f = g_srcNormalmap.Load( int3(pos, 0) ).xy;
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 normal = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-normal.z);
    normal.xy += normal.xy >= 0.0 ? -t : t;
    normal = normalize(mul(float4(normal, 0.f), viewMatrix).xyz);

    return (lpfloat3)normal;
}

// Engine-specific screen & temporal noise loader
lpfloat2 SpatioTemporalNoise( uint2 pixCoord, uint temporalIndex )    // without TAA, temporalIndex is always 0
{
    float2 noise;
#if 1   // Hilbert curve driving R2 (see https://www.shadertoy.com/view/3tB3z3)
    #ifdef XE_GTAO_HILBERT_LUT_AVAILABLE // load from lookup texture...
        uint index = g_srcHilbertLUT.Load( uint3( pixCoord % 64, 0 ) ).x;
    #else // ...or generate in-place?
        uint index = HilbertIndex( pixCoord.x, pixCoord.y );
    #endif
    index += 288*(temporalIndex%64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    // R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    return lpfloat2( frac( 0.5 + index * float2(0.75487766624669276005, 0.5698402909980532659114) ) );
#else   // Pseudo-random (fastest but looks bad - not a good choice)
    uint baseHash = Hash32( pixCoord.x + (pixCoord.y << 15) );
    baseHash = Hash32Combine( baseHash, temporalIndex );
    return lpfloat2( Hash32ToFloat( baseHash ), Hash32ToFloat( Hash32( baseHash ) ) );
#endif
}

// Engine-specific entry point for the first pass
[numthreads(8, 8, 1)]   // <- hard coded to 8x8; each thread computes 2x2 blocks so processing 16x16 block: Dispatch needs to be called with (width + 16-1) / 16, (height + 16-1) / 16
void CSPrefilterDepths16x16( uint2 dispatchThreadID : SV_DispatchThreadID, uint2 groupThreadID : SV_GroupThreadID )
{
    XeGTAO_PrefilterDepths16x16( dispatchThreadID, groupThreadID, g_GTAOConsts, g_srcRawDepth, g_samplerPointClamp, g_outWorkingDepthMIP0, g_outWorkingDepthMIP1, g_outWorkingDepthMIP2, g_outWorkingDepthMIP3, g_outWorkingDepthMIP4 );
}

// Engine-specific entry point for the second pass
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSGTAOLow( const uint2 pixCoord : SV_DispatchThreadID )
{
    // g_samplerPointClamp is a sampler with D3D12_FILTER_MIN_MAG_MIP_POINT filter and D3D12_TEXTURE_ADDRESS_MODE_CLAMP addressing mode
    XeGTAO_MainPass( pixCoord, 1, 2, SpatioTemporalNoise(pixCoord, g_GTAOConsts.NoiseIndex), LoadNormal(pixCoord), g_GTAOConsts, g_srcWorkingDepth, g_samplerPointClamp, g_outWorkingAOTerm, g_outWorkingEdges );
}

// Engine-specific entry point for the second pass
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSGTAOMedium( const uint2 pixCoord : SV_DispatchThreadID )
{
    // g_samplerPointClamp is a sampler with D3D12_FILTER_MIN_MAG_MIP_POINT filter and D3D12_TEXTURE_ADDRESS_MODE_CLAMP addressing mode
    XeGTAO_MainPass( pixCoord, 2, 2, SpatioTemporalNoise(pixCoord, g_GTAOConsts.NoiseIndex), LoadNormal(pixCoord), g_GTAOConsts, g_srcWorkingDepth, g_samplerPointClamp, g_outWorkingAOTerm, g_outWorkingEdges );
}

// Engine-specific entry point for the second pass
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSGTAOHigh( const uint2 pixCoord : SV_DispatchThreadID )
{
    // g_samplerPointClamp is a sampler with D3D12_FILTER_MIN_MAG_MIP_POINT filter and D3D12_TEXTURE_ADDRESS_MODE_CLAMP addressing mode
    XeGTAO_MainPass( pixCoord, 3, 3, SpatioTemporalNoise(pixCoord, g_GTAOConsts.NoiseIndex), LoadNormal(pixCoord), g_GTAOConsts, g_srcWorkingDepth, g_samplerPointClamp, g_outWorkingAOTerm, g_outWorkingEdges );
}

// Engine-specific entry point for the second pass
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSGTAOUltra( const uint2 pixCoord : SV_DispatchThreadID )
{
    // g_samplerPointClamp is a sampler with D3D12_FILTER_MIN_MAG_MIP_POINT filter and D3D12_TEXTURE_ADDRESS_MODE_CLAMP addressing mode
    XeGTAO_MainPass( pixCoord, 9, 3, SpatioTemporalNoise( pixCoord, g_GTAOConsts.NoiseIndex ), LoadNormal( pixCoord ), g_GTAOConsts, g_srcWorkingDepth, g_samplerPointClamp, g_outWorkingAOTerm, g_outWorkingEdges );
}

// Engine-specific entry point for the third pass
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSDenoisePass( const uint2 dispatchThreadID : SV_DispatchThreadID )
{
    const uint2 pixCoordBase = dispatchThreadID * uint2( 2, 1 );    // we're computing 2 horizontal pixels at a time (performance optimization)
    // g_samplerPointClamp is a sampler with D3D12_FILTER_MIN_MAG_MIP_POINT filter and D3D12_TEXTURE_ADDRESS_MODE_CLAMP addressing mode
    XeGTAO_Denoise( pixCoordBase, g_GTAOConsts, g_srcWorkingAOTerm, g_srcWorkingEdges, g_samplerPointClamp, g_outFinalAOTerm, false );
}

[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSDenoiseLastPass( const uint2 dispatchThreadID : SV_DispatchThreadID )
{
    const uint2 pixCoordBase = dispatchThreadID * uint2( 2, 1 );    // we're computing 2 horizontal pixels at a time (performance optimization)
    // g_samplerPointClamp is a sampler with D3D12_FILTER_MIN_MAG_MIP_POINT filter and D3D12_TEXTURE_ADDRESS_MODE_CLAMP addressing mode
    XeGTAO_Denoise( pixCoordBase, g_GTAOConsts, g_srcWorkingAOTerm, g_srcWorkingEdges, g_samplerPointClamp, g_outFinalAOTerm, true );
}

[numthreads(8, 8, 1)]
void CSComposeAO( const uint2 dispatchThreadID : SV_DispatchThreadID )
{
    uint i = dispatchThreadID.x;
    uint j = dispatchThreadID.y;
    float2 texcoord = float2(i,j) * g_GTAOConsts.ViewportPixelSize;
    float depth = g_depth[int2(i,j)];
    //get the current fragment pos
    float3 worldPos = DepthToWorld(depth, texcoord, InvViewProjMatrix);
    //get where it was in the prev frame in clip space
    float4 prevNdcPos = mul(float4(worldPos, 1.f), prevViewProjMatrix);
    //get where it is in ndc space
    prevNdcPos /= prevNdcPos.w;
    //get where it will be in screenspace [0, 1]
    float2 prevScreenspacePos = prevNdcPos.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    
    float currModulationFactor = modulationFactor;
    float oldAOTerm = 0.f;
    if(prevScreenspacePos.x >= 1.f || prevScreenspacePos.x <= 0.f || prevScreenspacePos.y >= 1.f || prevScreenspacePos.y <= 0.f)
    {
        //outside ndc, dont lerp any old values
        currModulationFactor = 0.f;
    }
    else
    {
        //get where the texel space of the pixel should be in the previous frame
        int2 texelPos = int2(prevScreenspacePos.xy * g_GTAOConsts.ViewportSize + float2(0.5f, 0.5f));
        texelPos.x = clamp(texelPos.x, 0, g_GTAOConsts.ViewportSize.x - 1);
        texelPos.y = clamp(texelPos.y, 0, g_GTAOConsts.ViewportSize.y - 1);
        //get the pixel movement
        float2 texcoordDiff = float2(prevScreenspacePos.xy - texcoord);
        currModulationFactor *= clamp(1.f - length(texcoordDiff), 0.f, 1.f);
        //get what the ao was in the last frame at that fragpos
        oldAOTerm = g_accumulationAOTerm[texelPos];
    }
    float newAOTerm = (float)g_currAOTerm[int2(i,j)] / 255.f;
    float interpolatedTerm = lerp(newAOTerm, oldAOTerm, currModulationFactor);
    //write the accumulated value onto the the accumulation buffer
    g_outAO[int2(i,j)] = interpolatedTerm;
}

// Optional screen space viewspace normals from depth generation
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CSGenerateNormals( const uint2 pixCoord : SV_DispatchThreadID )
{
    float3 viewspaceNormal = XeGTAO_ComputeViewspaceNormal( pixCoord, g_GTAOConsts, g_srcRawDepth, g_samplerPointClamp );

    // pack from [-1, 1] to [0, 1] and then to R11G11B10_UNORM
    g_outNormalmap[ pixCoord ] = XeGTAO_FLOAT3_to_R11G11B10_UNORM( saturate( viewspaceNormal * 0.5 + 0.5 ) );
}
///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
