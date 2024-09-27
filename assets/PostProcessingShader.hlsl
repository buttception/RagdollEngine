#include "Utils.hlsli"

cbuffer g_Const : register(b0) {
    float Gamma;
    float Exposure;
    int32_t UseFixedExposure;
};

Texture2D SceneColor : register(t0);
sampler Sampler : register(s0);
RWBuffer<float> Target : register(u0);

float3 convertRGB2XYZ(float3 _rgb)
{
	// Reference:
	// RGB/XYZ Matrices
	// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
	float3 xyz;
	xyz.x = dot(float3(0.4124564, 0.3575761, 0.1804375), _rgb);
	xyz.y = dot(float3(0.2126729, 0.7151522, 0.0721750), _rgb);
	xyz.z = dot(float3(0.0193339, 0.1191920, 0.9503041), _rgb);
	return xyz;
}

float3 convertXYZ2RGB(float3 _xyz)
{
	float3 rgb;
	rgb.x = dot(float3( 3.2404542, -1.5371385, -0.4985314), _xyz);
	rgb.y = dot(float3(-0.9692660,  1.8760108,  0.0415560), _xyz);
	rgb.z = dot(float3( 0.0556434, -0.2040259,  1.0572252), _xyz);
	return rgb;
}

float3 convertXYZ2Yxy(float3 _xyz)
{
	// Reference:
	// http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
	float inv = 1.0/dot(_xyz, float3(1.0, 1.0, 1.0) );
	return float3(_xyz.y, _xyz.x*inv, _xyz.y*inv);
}

float3 convertYxy2XYZ(float3 _Yxy)
{
	// Reference:
	// http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
	float3 xyz;
	xyz.x = _Yxy.x*_Yxy.y/_Yxy.z;
	xyz.y = _Yxy.x;
	xyz.z = _Yxy.x*(1.0 - _Yxy.y - _Yxy.z)/_Yxy.z;
	return xyz;
}

float3 convertRGB2Yxy(float3 _rgb)
{
	return convertXYZ2Yxy(convertRGB2XYZ(_rgb) );
}

float3 convertYxy2RGB(float3 _Yxy)
{
	return convertXYZ2RGB(convertYxy2XYZ(_Yxy) );
}

void tone_map_ps(
	in float4 inPos : SV_Position,
	in float2 inTexcoord : TEXCOORD0,
	out float4 outColor : SV_Target0
)
{
	float3 hdrColor = SceneColor.Sample(Sampler, inTexcoord).rgb;
    if(UseFixedExposure == 0)
    {
        float3 yxyColor = convertRGB2Yxy(hdrColor);
        yxyColor.x /= (9.6 * Target[0] + 0.0001);
        hdrColor = convertYxy2RGB(yxyColor);
    }
    else
        hdrColor *= Exposure;
    float3 finalColor = GammaCorrect(ACESFilm(hdrColor * Exposure), Gamma);
    outColor = float4(finalColor, 1.f);
}