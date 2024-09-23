cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
    float4x4 LightViewProj;
	float4 lightDiffuseColor;
	float4 sceneAmbientColor;
	float3 lightDirection;
	int instanceOffset;
	float3 cameraPosition;
	float lightIntensity;
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

void main_vs(
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
	outBinormal = cross(outTangent, outNormal) * binormalSign;
	outTexcoord = inTexcoord;
	outInstanceId = inInstanceId;
}

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
    float3 F0 = float3(0.1, 0.1, 0.1); // Base reflectivity
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

Texture2D ShadowMap : register(t1);
sampler Samplers[8] : register(s0);
SamplerComparisonState ShadowSample : register(s9);

void main_ps(
	in float4 inPos : SV_Position,
	in float4 inFragPos : TEXCOORD1,
	in float3 inNormal : TEXCOORD2,
	in float3 inTangent : TEXCOORD3,
	in float3 inBinormal : TEXCOORD4,
	in float2 inTexcoord : TEXCOORD5,
	in uint inInstanceId : TEXCOORD6,
	out float4 outColor : SV_Target0
)
{
	InstanceData data = InstanceDatas[inInstanceId + instanceOffset];
	if(data.isLit)
	{
		// Sample textures
		float4 albedo = data.albedoFactor;
		if(data.albedoIndex != -1){
			albedo *= Textures[data.albedoIndex].Sample(Samplers[data.albedoSamplerIndex], inTexcoord);
		}
		float4 RM = float4(1.f, data.roughness, data.metallic, 0);
		if(data.roughnessMetallicIndex != -1){
			RM = Textures[data.roughnessMetallicIndex].Sample(Samplers[data.roughnessMetallicSamplerIndex], inTexcoord);
		}
		float ao = 1.f;
		float roughness = RM.g;
		float metallic = RM.b;

		// Sample normal map and transform to world space
		float3 N = inNormal;
		if(data.normalIndex != -1){
			float3 normalMapValue = normalize(Textures[data.normalIndex].Sample(Samplers[data.normalSamplerIndex], inTexcoord).xyz * 2.0f - 1.0f);
			float3x3 TBN = float3x3(inTangent, inBinormal, inNormal);
			N = normalize(mul(normalMapValue, TBN));
		}

		float3 diffuse = PBRLighting(albedo.rgb, N, cameraPosition - inFragPos.xyz, lightDirection, lightDiffuseColor.rgb * lightIntensity, metallic, roughness, ao);

		float4 fragPosLightSpace = mul(inFragPos, LightViewProj);
		float3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
		projCoord.xy = projCoord.xy * 0.5f + float2(0.5f, 0.5f);
		projCoord.y = projCoord.y * -1.0f + 1.0f;
		projCoord.z += 0.005f;
		float shadow = 0.f;

		// Percentage-Closer Filtering (PCF) for smoother shadow edges.
		float2 texelSize = float2(1.f, 1.f) / 2000.f;
		for(int x = -1; x <= 1; ++x)
		{
			for(int y = -1; y <= 1; ++y)
			{
				float2 offset = float2(x, y) * texelSize;
				shadow += ShadowMap.SampleCmpLevelZero(ShadowSample, projCoord.xy + offset, projCoord.z);
				//shadow += projCoord.z < ShadowMap.Sample(Samplers[0], projCoord.xy + offset).r ? 1.f : 0.f;
			}
		}
		// Divide by number of samples (9)
		shadow /= 9.f;

		//float depth = ShadowMap.Sample(Samplers[0], projCoord.xy).r;
		//shadow = projCoord.z < depth ? 1.f : 0.f;

		//shadow = ShadowMap.SampleCmpLevelZero(ShadowSample, projCoord.xy, projCoord.z);

		// Combine lighting contributions
		float3 ambient = sceneAmbientColor.rgb * albedo.rgb;
		float3 lighting = ambient + diffuse * (1.f - shadow);

		// Final color output
		outColor = float4(lighting.rgb, albedo.a);
	}
	else
	{
		outColor = data.albedoFactor;
	}
}