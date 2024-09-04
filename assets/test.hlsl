static float2 g_positions[] = {
	float2(-0.5, -0.5),
	float2(0, 0.5),
	float2(0.5, -0.5)
};

static float3 g_colors[] = {
	float3(1, 0, 0),
	float3(1, 0, 0),
	float3(1, 0, 0)	
};

cbuffer g_Const : register(b0) { float2 translationOffset; float2 scale; float radians; }

void main_vs(
	uint i_vertexId : SV_VertexID,
	out float4 o_pos : SV_Position,
	out float3 o_color : COLOR
)
{
	for(int i = 0; i < 3; ++i){
		g_positions[i] *= scale;
		float cosAngle = cos(radians);
		float sinAngle = sin(radians);
		float2x2 rotMat = float2x2(cosAngle, -sinAngle, sinAngle, cosAngle);
		g_positions[i] = mul(rotMat, g_positions[i]);
		g_positions[i] += translationOffset;
	}
	o_pos = float4(g_positions[i_vertexId], 0, 1);
	o_color = g_colors[i_vertexId];
}

void main_ps(
	in float4 i_pos : SV_Position,
	in float3 i_color : COLOR,
	out float4 o_color : SV_Target0
)
{
	o_color = float4(i_color, 1);
}