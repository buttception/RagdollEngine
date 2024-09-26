//numeric constants
static const float PI = 3.14159265;

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

    float3 diffuse = albedo / PI * max(dot(N, L), 0.0);

    // Combine the two contributions
    float3 color = (kD * diffuse + specular) * lightColor * max(dot(N, L), 0.0);

    // Apply ambient occlusion
    color *= ao;

    return color;
}