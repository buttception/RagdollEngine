#pragma pack_matrix(row_major)

cbuffer g_Const : register(b0) { float4x4 worldMatrix; float4x4 viewProjMatrix; }

void main_vs(
	in float3 inPos : POSITION,
	in float3 inColor : ICOLOR,
	out float4 o_pos : SV_Position,
	out float3 o_color : COLOR
)
{
	o_pos = mul(mul(float4(inPos, 1), worldMatrix), viewProjMatrix);
	o_color = inColor;
}

void main_ps(
	in float4 i_pos : SV_Position,
	in float3 i_color : COLOR,
	out float4 o_color : SV_Target0
)
{
	o_color = float4(i_color, 1);
}