
static const float3 kXYZToR = float3( 3.2404542f, -1.5371385f, -0.4985314f);
static const float3 kXYZToG = float3(-0.9692660f,  1.8760108f,  0.0415560f);
static const float3 kXYZToB = float3( 0.0556434f, -0.2040259f,  1.0572252f);

static const float3 kRGBToX = float3(0.4124564f,  0.3575761f,  0.1804375f);
static const float3 kRGBToY = float3(0.2126729f,  0.7151522f,  0.0721750f);
static const float3 kRGBToZ = float3(0.0193339f,  0.1191920f,  0.9503041f);

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

float3 Decode(float2 f)
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

// Maps standard viewport UV to screen position.
float2 ViewportUVToScreenPos(float2 ViewportUV)
{
	return float2(2 * ViewportUV.x - 1, 1 - 2 * ViewportUV.y);
}

//screenpos but should be ndc, lifted from unreal
float2 ScreenPosToViewportUV(float2 ScreenPos)
{
	return float2(0.5 + 0.5 * ScreenPos.x, 0.5 - 0.5 * ScreenPos.y);
}

float3 DepthToWorld(float depth, float2 inTexcoord, float4x4 InvViewProj)
{
	float4 clipspacePos = float4(ViewportUVToScreenPos(inTexcoord), depth, 1.0);
	float4 homogenousPos = mul(clipspacePos, InvViewProj);
    return homogenousPos.xyz / homogenousPos.w;;
}

float3 DepthToView(float depth, float2 inTexcoord, float4x4 InvProj)
{
    float4 clipspacePos = float4(ViewportUVToScreenPos(inTexcoord), depth, 1.0);
    float4 homogenousPos = mul(clipspacePos, InvProj);
    return homogenousPos.xyz / homogenousPos.w;
}

float3 WorldToLight(float3 fragPos, float4x4 lightMatrix, float bias)
{
    float4 fragPosLightSpace = mul(float4(fragPos, 1.f), lightMatrix);
    float3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoord.xy = projCoord.xy * 0.5f + float2(0.5f, 0.5f);
    projCoord.y = projCoord.y * -1.0f + 1.0f;
    projCoord.z += bias;
    return projCoord;
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

float3 GammaCorrect(float3 color, float gamma)
{
    return pow(color, float3(1.f / gamma, 1.f / gamma, 1.f / gamma));
}

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

float3 convertxyYToXYZ(float3 _c)
{
    return float3(_c.x, _c.y, 1.0f - _c.x - _c.y) * _c.z / _c.y;
}

float3 convertxyYToRGB(float3 _xyY)
{
    float3 XYZ = convertxyYToXYZ(_xyY);

    return float3
    (
        dot(kXYZToR, XYZ),
        dot(kXYZToG, XYZ),
        dot(kXYZToB, XYZ)
    );
}

float3x3 Adjugate(float4x4 m)
{
    return float3x3(cross(m[1].xyz, m[2].xyz),
                cross(m[2].xyz, m[0].xyz),
                cross(m[0].xyz, m[1].xyz));

    /*
    // alternative way to write the adjoint

    return mat3( 
     m[1].yzx*m[2].zxy-m[1].zxy*m[2].yzx,
     m[2].yzx*m[0].zxy-m[2].zxy*m[0].yzx,
     m[0].yzx*m[1].zxy-m[0].zxy*m[1].yzx );
    */
    
    /*
    // alternative way to write the adjoint

    return mat3( 
     m[1][1]*m[2][2]-m[1][2]*m[2][1],
     m[1][2]*m[2][0]-m[1][0]*m[2][2],
     m[1][0]*m[2][1]-m[1][1]*m[2][0],
     m[0][2]*m[2][1]-m[0][1]*m[2][2],
	 m[0][0]*m[2][2]-m[0][2]*m[2][0],
     m[0][1]*m[2][0]-m[0][0]*m[2][1],
     m[0][1]*m[1][2]-m[0][2]*m[1][1],
     m[0][2]*m[1][0]-m[0][0]*m[1][2],
     m[0][0]*m[1][1]-m[0][1]*m[1][0] );
    */
}

//https://www.shadertoy.com/view/tllcR2
#define hash(p)  frac(sin(dot(p, float2(11.9898, 78.233))) * 43758.5453) // iq suggestion, for Windows
float BlueNoise(float2 U)
{
    float v = 0.;
    for (int k = 0; k < 9; k++)
        v += hash(U + float2(k%3-1,k/3-1));
  //return       1.125*hash(U)- v/8.  + .5; // some overbound, closer contrast
    return .9 * (1.125 * hash(U) - v / 8.) + .5; // 
  //return .75*( 1.125*hash(U)- v/8.) + .5; // very slight overbound
  //return .65*( 1.125*hash(U)- v/8.) + .5; // dimmed, but histo just fit without clamp. flat up to .5 +- .23
}