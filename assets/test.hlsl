#pragma pack_matrix(row_major)

static float3 g_color = float3(1,0,0);

//cbuffer g_Const : register(b0) { float2 translationOffset; float2 scale; float radians; }

void main_vs(
	in float3 inPos : POSITION,
	out float4 o_pos : SV_Position,
	out float3 o_color : COLOR
)
{
	o_pos = float4(inPos, 1);
	o_color = g_color;
}

void main_ps(
	in float4 i_pos : SV_Position,
	in float3 i_color : COLOR,
	out float4 o_color : SV_Target0
)
{
	o_color = float4(i_color, 1);
}