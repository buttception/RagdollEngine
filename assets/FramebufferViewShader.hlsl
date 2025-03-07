cbuffer g_Const : register(b0) {
	float4 CompAdd;
	float4 CompMul;
    float2 TexcoordAdd;
    float2 TexcoordMul;
	uint ComponentCount;
};

Texture2D Source : register(t0);
sampler Sampler : register(s0); 

void main_ps(
    in float4 inPos: SV_Position,
    in float2 inTexcoord : TEXCOORD0,
    out float4 outColor : SV_Target0
)
{
    float4 finalColor = float4(0.f, 0.f, 0.f, 1.f);
    float2 finalTexcoord = inTexcoord * TexcoordMul + TexcoordAdd;
    [branch]
    switch(ComponentCount){
        case 1:
            finalColor.x = float(Source.Sample(Sampler, finalTexcoord).x) * CompMul.x + CompAdd.x;
            break;
        case 2:
            finalColor.xy = float2(Source.Sample(Sampler, finalTexcoord).xy) * CompMul.xy + CompAdd.xy;
            break;
        case 3:
            finalColor.xyz = Source.Sample(Sampler, finalTexcoord).xyz * CompMul.xyz + CompAdd.xyz;
            break;
        case 4:
            finalColor.xyzw = Source.Sample(Sampler, finalTexcoord).xyzw * CompMul.xyzw + CompAdd.xyzw;
            break;
        default:
            break;
    }
    outColor = finalColor;
}

cbuffer g_Const : register(b0)
{
    float RenderWidth;
    float RenderHeight;
    uint FieldsNeeded;
    uint TileX;
    uint TileY;
    uint TileZ;
    uint LightCount;
    float LerpFactor;
    int SliceStart;
    int SliceEnd;
};

StructuredBuffer<uint> LightBitFieldsBufferInput : register(t0);
Texture2D<float4> FinalColor : register(t1);

float3 LerpColor(float3 ColorA, float3 ColorB, float T)
{
    return lerp(ColorA, ColorB, saturate(T));
}

float3 GetHeatMapColor(float Value)
{
    if (Value <= 0.33f)
        return LerpColor(float3(0, 0, 1), float3(0, 1, 0), Value / 0.33f);
    else if (Value <= 0.66f)
        return LerpColor(float3(0, 1, 0), float3(1, 1, 0), (Value - 0.33f) / 0.33f);
    else
        return LerpColor(float3(1, 1, 0), float3(1, 0, 0), (Value - 0.66f) / 0.34f);

}

void main_light_grid_ps(
    in float4 inPos : SV_Position,
    in float2 inTexcoord : TEXCOORD0,
    out float4 outColor : SV_Target0
)
{
    float2 flippedCoord = float2(inTexcoord.x, 1.f - inTexcoord.y);
    //based on texcoord can get the tile index
    float2 PixelPos = inTexcoord * float2(RenderWidth, RenderHeight);
    //get tile index
    uint2 TileIndex = flippedCoord * float2(TileX, TileY);
    uint LightInTile = 0;
    for (int i = 0; i < FieldsNeeded; i++)
    {
        //check every field in each tile to get all the lights
        uint Field = 0;
        for (int j = SliceStart; j < SliceEnd; ++j)
        {
            uint Index = TileIndex.x + TileIndex.y * TileX + j * TileX * TileY;
            Index *= FieldsNeeded;
            Field |= LightBitFieldsBufferInput[Index + i];
        }
        //get the number of bits that is set
        for (int k = 0; k < 32; k++)
        {
            LightInTile += (Field >> k) & 1;
        }
    }
    //color the pixel the heat map value
    float3 HeatColor = GetHeatMapColor((float) LightInTile / (float) LightCount);
    outColor = float4(LerpColor(HeatColor, FinalColor.Load(int3(PixelPos, 0)).rgb, LerpFactor), 1.f);

}