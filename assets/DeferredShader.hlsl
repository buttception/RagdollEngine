cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
	int instanceOffset;
};

struct InstanceData{
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;

	float4 albedoFactor;
	float roughness;
	float metallic;

	int albedoIndex;
	int albedoSamplerIndex;
	int normalIndex;
	int normalSamplerIndex;
	int roughnessMetallicIndex;
	int roughnessMetallicSamplerIndex;
	int isLit;
};

StructuredBuffer<InstanceData> InstanceDatas : register(t0);
Texture2D Textures[] : register(t0, space1);

void gbuffer_vs(
	in float3 inPos : POSITION,
	in float3 inNormal : NORMAL,
	in float3 inTangent : TANGENT,
	in float2 inTexcoord : TEXCOORD,
	in uint inInstanceId : SV_INSTANCEID,
	out float4 outPos : SV_Position,
	out float4 outFragPos : TEXCOORD1,
	out float3 outNormal : TEXCOORD2,
	out float3 outTangent : TEXCOORD3,
	out float3 outBinormal : TEXCOORD4,
	out float2 outTexcoord : TEXCOORD5,
	out nointerpolation uint outInstanceId : TEXCOORD6
)
{
	InstanceData data = InstanceDatas[inInstanceId + instanceOffset];
	outFragPos = mul(float4(inPos, 1), data.worldMatrix); 
	outPos = mul(outFragPos, viewProjMatrix);

	float binormalSign = inNormal.x > 0.0f ? -1.0f : 1.0f;
	outNormal = normalize(mul(inNormal, transpose((float3x3)data.invWorldMatrix)));
	outTangent = normalize(mul(inTangent, transpose((float3x3)data.invWorldMatrix)));
	//still need so we can sample the normal properly
	outBinormal = normalize(cross(outTangent, outNormal));
	outTexcoord = inTexcoord;
	outInstanceId = inInstanceId;
}

sampler Samplers[9] : register(s0);

float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}
 
float2 Encode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

void gbuffer_ps(
	in float4 inPos : SV_Position,
	in float4 inFragPos : TEXCOORD1,
	in float3 inNormal : TEXCOORD2,
	in float3 inTangent : TEXCOORD3,
	in float3 inBinormal : TEXCOORD4,
	in float2 inTexcoord : TEXCOORD5,
	in uint inInstanceId : TEXCOORD6,
	out float4 outColor : SV_Target0,
	out float2 outNormals: SV_Target1,
	out float2 outRoughnessMetallic: SV_Target2
)
{
	InstanceData data = InstanceDatas[inInstanceId + instanceOffset];
	// Sample textures
	float4 albedo = data.albedoFactor;
	if(data.albedoIndex != -1){
		albedo *= Textures[data.albedoIndex].Sample(Samplers[data.albedoSamplerIndex], inTexcoord);
	}
	clip(albedo.a - 0.01f);
	float4 RM = float4(0, data.roughness, data.metallic, 0);
	if(data.roughnessMetallicIndex != -1){
		RM = Textures[data.roughnessMetallicIndex].Sample(Samplers[data.roughnessMetallicSamplerIndex], inTexcoord);
	}
	float roughness = RM.g;
	float metallic = RM.b;

	// Sample normal map and leave it in model space, the deferred lighting will calculate this instead
	float3 N = inNormal;
	if(data.normalIndex != -1){
		float3 normalMapValue = normalize(Textures[data.normalIndex].Sample(Samplers[data.normalSamplerIndex], inTexcoord).xyz * 2.0f - 1.0f);
		float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
		N = normalize(mul(normalMapValue, TBN));
	}

	//draw to the targets
	outColor = albedo;
	outNormals.xy = Encode(N);
	outRoughnessMetallic = float2(roughness, metallic);
}
cbuffer g_LightConst : register(b1) {
	float4x4 InvViewProjMatrix;
	float4 LightDiffuseColor;
	float4 SceneAmbientColor;
	float3 LightDirection;
	float LightIntensity;
	float3 CameraPosition;
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D RMTexture : register(t2);
Texture2D DepthBuffer : register(t3);

//numeric constants
static const float PI = 3.14159265;

float3 Decode(float2 f)
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

// Utility Functions for PBR Lighting
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// PBR Lighting Calculation
float3 PBRLighting(float3 albedo, float3 normal, float3 viewDir, float3 lightDir, float3 lightColor, float metallic, float roughness, float ao) {
    float3 N = normalize(normal);
    float3 V = normalize(viewDir);
    float3 L = normalize(lightDir);
    float3 H = normalize(V + L); // Halfway vector

    // Fresnel-Schlick approximation
    float3 F0 = float3(0.04, 0.04, 0.04); // Base reflectivity
    F0 = lerp(F0, albedo, metallic); // Mix with albedo for metallic surfaces
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    // GGX Normal Distribution Function (NDF)
    float NDF = DistributionGGX(N, H, roughness);

    // Smith's Geometry Function (Visibility)
    float G = GeometrySmith(N, V, L, roughness);

    // Specular BRDF
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001; // Prevent divide by zero
    float3 specular = numerator / denominator;

    // Lambertian diffuse
    float3 kS = F; // Fresnel reflectance
    float3 kD = 1.0 - kS; // Diffuse reflection (1.0 - specular)
    kD *= 1.0 - metallic; // Metallic surfaces don't have diffuse reflection

    float3 diffuse = albedo * max(dot(N, L), 0.0);

    // Combine the two contributions
    float3 color = (kD * diffuse + specular) * lightColor * max(dot(N, L), 0.0);

    // Apply ambient occlusion
    color *= ao;

    return color;
}

void deferred_light_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD1,
	out float4 outColor : SV_Target0
)
{
	//getting texture values
	float4 albedo = albedoTexture.Sample(Samplers[5], inTexcoord);
	float3 N = Decode(normalTexture.Sample(Samplers[5], inTexcoord).xy);
	float2 RM = RMTexture.Sample(Samplers[5], inTexcoord).xy;

	//getting fragpos
	float depth = DepthBuffer.Sample(Samplers[5], inTexcoord).r;
	float4 clipspacePos = float4(inTexcoord * 2.0 - 1.0, depth, 1.0);
	float4 homogenousPos = mul(clipspacePos, InvViewProjMatrix);
	float3 fragPos = homogenousPos.xyz / homogenousPos.w;

	//apply pbr lighting, AO is 1.f for now so it does nth
	//float3 diffuse = max(dot(N, LightDirection), 0) * albedo.rgb;
	float3 diffuse = PBRLighting(albedo.rgb, N, CameraPosition - fragPos, LightDirection, LightDiffuseColor.rgb * LightIntensity, RM.y, RM.x, 1.f);

	float3 ambient = SceneAmbientColor.rgb * albedo.rgb;
	float3 lighting = ambient + diffuse;
	outColor = float4(lighting, 1.f);
}