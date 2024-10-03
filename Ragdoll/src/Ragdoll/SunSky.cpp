#include "ragdollpch.h"
#include "SunSky.h"

float sqr(float f) { return f * f; }
float ZenithLuminance(float thetaS, float T)
{
    float chi = (4.0f / 9.0f - T / 120.0f) * (DirectX::XM_PI - 2.0f * thetaS);
    float Lz = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
    Lz *= 1000.0;   // conversion from kcd/m^2 to cd/m^2
    return Lz;
}
float ClampUnit(float s)
{
    if (s <= 0.0f)
        return 0.0f;
    if (s >= 1.0f)
        return 1.0f;
    return s;
}
inline float lerp(float a, float b, float s)
{
    return (1.0f - s) * a + s * b;
}
inline float PerezUpper(const float* lambdas, float cosTheta, float gamma, float cosGamma)
{
    return  (1.0f + lambdas[0] * expf(lambdas[1] / (cosTheta + 1e-6f)))
        * (1.0f + lambdas[2] * expf(lambdas[3] * gamma) + lambdas[4] * sqr(cosGamma));
}

inline float PerezLower(const float* lambdas, float cosThetaS, float thetaS)
{
    return  (1.0f + lambdas[0] * expf(lambdas[1]))
        * (1.0f + lambdas[2] * expf(lambdas[3] * thetaS) + lambdas[4] * sqr(cosThetaS));
}
inline float UnmapGamma(float g)
{
    return 1 - 2 * sqr(g);
}
inline uint8_t ToU8(float f)
{
    if (f <= 0.0f)
        return 0;
    if (f >= 1.0f)
        return 255;

    return uint8_t(f * 255.0f + 0.5f);
}

FSunSkyPreetham::FSunSkyPreetham() :
    mToSun(Vector3::Zero),

    // mPerez_x[5] = {}
    // mPerez_y[5] = {}
    // mPerez_Y[5] = {}
    mZenith(Vector3::Zero),
    mPerezInvDen(Vector3::One)
{
}

void FSunSkyPreetham::Update(const Vector3& sun, float turbidity, float overcast, float horizCrush)
{
    mToSun = sun;

    float T = turbidity;

    mPerez_Y[0] = 0.17872f * T - 1.46303f;
    mPerez_Y[1] = -0.35540f * T + 0.42749f;
    mPerez_Y[2] = -0.02266f * T + 5.32505f;
    mPerez_Y[3] = 0.12064f * T - 2.57705f;
    mPerez_Y[4] = -0.06696f * T + 0.37027f;

    mPerez_x[0] = -0.01925f * T - 0.25922f;
    mPerez_x[1] = -0.06651f * T + 0.00081f;
    mPerez_x[2] = -0.00041f * T + 0.21247f;
    mPerez_x[3] = -0.06409f * T - 0.89887f;
    mPerez_x[4] = -0.00325f * T + 0.04517f;

    mPerez_y[0] = -0.01669f * T - 0.26078f;
    mPerez_y[1] = -0.09495f * T + 0.00921f;
    mPerez_y[2] = -0.00792f * T + 0.21023f;
    mPerez_y[3] = -0.04405f * T - 1.65369f;
    mPerez_y[4] = -0.01092f * T + 0.05291f;

    float cosTheta = mToSun.z;
    float theta = acosf(cosTheta);    // angle from zenith rather than horizon
    float theta2 = sqr(theta);
    float theta3 = theta2 * theta;
    float T2 = sqr(T);

    // mZenith stored as xyY
    mZenith.z = ZenithLuminance(theta, T);

    mZenith.x =
        (0.00165f * theta3 - 0.00374f * theta2 + 0.00208f * theta + 0.00000f) * T2 +
        (-0.02902f * theta3 + 0.06377f * theta2 - 0.03202f * theta + 0.00394f) * T +
        (0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * theta + 0.25885f);

    mZenith.y =
        (0.00275f * theta3 - 0.00610f * theta2 + 0.00316f * theta + 0.00000f) * T2 +
        (-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * theta + 0.00515f) * T +
        (0.15346f * theta3 - 0.26756f * theta2 + 0.06669f * theta + 0.26688f);

    // Adjustments (extensions)

    if (cosTheta < 0.0f)    // Handle sun going below the horizon
    {
        float s = ClampUnit(1.0f + cosTheta * 50.0f);   // goes from 1 to 0 as the sun sets

        // Take C/E which control sun term to zero
        mPerez_x[2] *= s;
        mPerez_y[2] *= s;
        mPerez_Y[2] *= s;
        mPerez_x[4] *= s;
        mPerez_y[4] *= s;
        mPerez_Y[4] *= s;
    }

    if (overcast != 0.0f)      // Handle overcast term
    {
        float invOvercast = 1.0f - overcast;

        // lerp back towards unity
        mPerez_x[0] *= invOvercast;  // main sky chroma -> base
        mPerez_y[0] *= invOvercast;

        // sun flare -> 0 strength/base chroma
        mPerez_x[2] *= invOvercast;
        mPerez_y[2] *= invOvercast;
        mPerez_Y[2] *= invOvercast;
        mPerez_x[4] *= invOvercast;
        mPerez_y[4] *= invOvercast;
        mPerez_Y[4] *= invOvercast;

        // lerp towards a fit of the CIE cloudy sky model: 4, -0.7
        mPerez_Y[0] = lerp(mPerez_Y[0], 4.0f, overcast);
        mPerez_Y[1] = lerp(mPerez_Y[1], -0.7f, overcast);

        // lerp base colour towards white point
        mZenith.x = mZenith.x * invOvercast + 0.333f * overcast;
        mZenith.y = mZenith.y * invOvercast + 0.333f * overcast;
    }

    if (horizCrush != 0.0f)
    {
        // The Preetham sky model has a "muddy" horizon, which can be objectionable in
        // typical game views. We allow artistic control over it.
        mPerez_Y[1] *= horizCrush;
        mPerez_x[1] *= horizCrush;
        mPerez_y[1] *= horizCrush;
    }

    // initialize sun-constant parts of the Perez functions

    Vector3 perezLower    // denominator terms are dependent on sun angle only
    (
        PerezLower(mPerez_x, cosTheta, theta),
        PerezLower(mPerez_y, cosTheta, theta),
        PerezLower(mPerez_Y, cosTheta, theta)
    );

    mPerezInvDen = mZenith / perezLower;
}

void FSunSkyTable::FindThetaGammaTables(const FSunSkyPreetham& pt)
{
    float dt = 1.0f / (kTableSize - 1);
    float t = dt * 1e-6f;    // epsilon to avoid divide by 0, which can lead to NaN when m_perez_[1] = 0

    for (int i = 0; i < kTableSize; i++)
    {
        float cosTheta = t;

        Tables.mThetaTable[i].x = -pt.mPerez_x[0] * expf(pt.mPerez_x[1] / cosTheta);
        Tables.mThetaTable[i].y = -pt.mPerez_y[0] * expf(pt.mPerez_y[1] / cosTheta);
        Tables.mThetaTable[i].z = -pt.mPerez_Y[0] * expf(pt.mPerez_Y[1] / cosTheta);

        mMaxTheta = mMaxTheta >= Tables.mThetaTable[i].z ? mMaxTheta : Tables.mThetaTable[i].z;

        float cosGamma = UnmapGamma(t);
        float gamma = acosf(cosGamma);

        Tables.mGammaTable[i].x = pt.mPerez_x[2] * expf(pt.mPerez_x[3] * gamma) + pt.mPerez_x[4] * sqr(cosGamma);
        Tables.mGammaTable[i].y = pt.mPerez_y[2] * expf(pt.mPerez_y[3] * gamma) + pt.mPerez_y[4] * sqr(cosGamma);
        Tables.mGammaTable[i].z = pt.mPerez_Y[2] * expf(pt.mPerez_Y[3] * gamma) + pt.mPerez_Y[4] * sqr(cosGamma);

        mMaxGamma = mMaxGamma >= Tables.mGammaTable[i].z ? mMaxGamma : Tables.mGammaTable[i].z;

        t += dt;
    }

    mXYZ = false;

    //update the data array
    for (int i = 0; i < kTableSize; i++)
    {
        Vector3 c = Tables.mThetaTable[i];

        c.z /= mMaxTheta;

        Data[0][i][0] = ToU8(c.x);
        Data[0][i][1] = ToU8(c.y);
        Data[0][i][2] = ToU8(c.z);
        Data[0][i][3] = 255;
    }

    for (int i = 0; i < kTableSize; i++)
    {
        Vector3 c = Tables.mGammaTable[i];

        c.z /= mMaxGamma;

        Data[1][i][0] = ToU8(c.x);
        Data[1][i][1] = ToU8(c.y);
        Data[1][i][2] = ToU8(c.z);
        Data[1][i][3] = 255;
    }
}
