cbuffer g_Const : register(b0) {
	float4x4 worldMatrix;
	float4x4 viewProjMatrix;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;

	bool useNormalMap;
	bool useRoughnessMetallicMap;
};

void main_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float3 inBinormal : BINORMAL,
	in float2 inTexcoord : TEXCOORD,
	out float4 o_pos : SV_Position,
	out float3 outNormal : NORMAL1,
	out float3 outTangent : TANGENT1,
	out float3 outBinormal : BINORMAL1,
	out float2 outTexcoord : TEXCOORD
)
{
	o_pos = mul(mul(float4(inPos, 1), worldMatrix), viewProjMatrix);
	outNormal = inNormal;
	outTangent = inTangent;
	outBinormal = inBinormal;
	outTexcoord = inTexcoord;
}

sampler sampler0 : register(s0);
Texture2D texture0 : register(t0);
sampler sampler1 : register(s1);
Texture2D texture1 : register(t1);

void main_ps(
	in float4 i_pos : SV_Position,
	in float3 inNormal : NORMAL1,
	in float3 inTangent : TANGENT1,
	in float3 inBinormal : BINORMAL1,
	in float2 inTexcoord : TEXCOORD,
	out float4 o_color : SV_Target0
)
{
	// Sample the normal map
    float3 normalMapValue = normalize(texture1.Sample(sampler1, inTexcoord).xyz * 2.0f - 1.0f);
    // Construct the tangent space matrix
    float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
    // Transform the light direction to tangent space
    float3 lightDir = mul(lightDirection.xyz, (float3x3)worldMatrix);
    float3 lightDirTangentSpace = mul(lightDir, TBN);

	float4 objectColor = texture0.Sample(sampler0, inTexcoord);
	float diffuseScalar = max(dot(normalMapValue, lightDirTangentSpace), 0);
	o_color = float4((sceneAmbientColor.xyz + diffuseScalar * lightDiffuseColor.xyz) * objectColor.xyz, 1);
}