cbuffer g_Const : register(b0) {
	float4x4 worldMatrix;
	float4x4 viewProjMatrix;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;
};

void main_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float2 inTexcoord : TEXCOORD,
	out float4 o_pos : SV_Position,
	out float3 outNormal : NORMAL1,
	out float2 outTexcoord : TEXCOORD
)
{
	o_pos = mul(mul(float4(inPos, 1), worldMatrix), viewProjMatrix);
	outNormal = inNormal;
	outTexcoord = inTexcoord;
}

sampler sampler0 : register(s0);
Texture2D texture0 : register(t0);

void main_ps(
	in float4 i_pos : SV_Position,
	in float3 inNormal : NORMAL1,
	in float2 inTexcoord : TEXCOORD,
	out float4 o_color : SV_Target0
)
{
	float4 objectColor = texture0.Sample(sampler0, inTexcoord);
	float3 normal = normalize(inNormal);
	float3 lightDir = normalize(lightDirection);
	float diffuseScalar = max(dot(normal, lightDir), 0);
	o_color = float4((sceneAmbientColor.xyz + diffuseScalar * lightDiffuseColor.xyz) * objectColor.xyz, 1);
}