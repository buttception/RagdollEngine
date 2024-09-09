cbuffer g_Const : register(b0) {
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;
	float4x4 viewProjMatrix;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;
	float3 cameraPosition;

	bool useNormalMap;
	bool useRoughnessMetallicMap;
};

void main_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float3 inBinormal : BINORMAL,
	in float2 inTexcoord : TEXCOORD,
	out float4 outPos : SV_Position,
	out float4 outFragPos : POSITION1,
	out float3 outNormal : NORMAL1,
	out float3 outTangent : TANGENT1,
	out float3 outBinormal : BINORMAL1,
	out float2 outTexcoord : TEXCOORD
)
{
	outFragPos = mul(float4(inPos, 1), worldMatrix); 
	outPos = mul(outFragPos, viewProjMatrix);

	outNormal = normalize(mul(inNormal, transpose((float3x3)invWorldMatrix)));
	//outNormal = normalize(mul(inNormal, (float3x3)worldMatrix));
	outTangent = normalize(mul(inTangent, (float3x3)worldMatrix));
	outBinormal = normalize(mul(inBinormal, (float3x3)worldMatrix));
	outTexcoord = inTexcoord;
}

sampler albedoSampler : register(s0);
Texture2D albedoTexture : register(t0);
sampler normalSampler : register(s1);
Texture2D normalTexture : register(t1);
sampler RMSampler : register(s2);
Texture2D RMTexture : register(t2);

// Helper functions for PBR calculations
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;
    
    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main_ps(
	in float4 inPos : SV_Position,
	in float4 inFragPos : POSITION1,
	in float3 inNormal : NORMAL1,
	in float3 inTangent : TANGENT1,
	in float3 inBinormal : BINORMAL1,
	in float2 inTexcoord : TEXCOORD,
	out float4 outColor : SV_Target0
)
{
    // Sample textures
	float3 albedo = albedoTexture.Sample(albedoSampler, inTexcoord).rgb;
	float4 RM = RMTexture.Sample(RMSampler, inTexcoord);
	float roughness = RM.r;
	float metallic = RM.g;

    // Sample normal map and transform to world space
    float3 normalMapValue = normalize(normalTexture.Sample(normalSampler, inTexcoord).xyz * 2.0f - 1.0f);
    float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
    float3 N = normalize(mul(normalMapValue, TBN));

    // Get camera and light directions
    float3 V = normalize(cameraPosition - inFragPos.xyz);
    float3 L = normalize(lightDirection);

    // Calculate half-vector
    float3 H = normalize(V + L);

    // Fresnel (Schlick's approximation)
    float3 F0 = float3(0.04, 0.04, 0.04); // Dielectric specular reflectance
    F0 = lerp(F0, albedo, metallic); // Adjust for metallic surfaces

    // Specular and diffuse contributions
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    // Final specular reflection
    float3 numerator = D * G * F;
    float3 denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    float3 specular = numerator / denominator;

    // Calculate diffuse and ambient lighting
    float3 kD = (1.0 - F) * (1.0 - metallic);  // Diffuse component for non-metallic surfaces
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse = kD * albedo * NdotL;

    // Combine lighting contributions
    float3 ambient = sceneAmbientColor.rgb * albedo;
    float3 lighting = ambient + diffuse + specular;

    // Final color output
    outColor = float4(lighting, 1.0);
}