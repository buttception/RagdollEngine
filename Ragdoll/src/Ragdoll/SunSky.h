#pragma once

#include "Ragdoll/Math/RagdollMath.h"

class FSunSkyPreetham
{
public:
    FSunSkyPreetham();

    void Update(const Vector3& sun, float turbidity, float overcast = 0.0f, float hCrush = 0.0f); // update model with given settings

    // Data
    Vector3 mToSun;

    float mPerez_x[5];
    float mPerez_y[5];
    float mPerez_Y[5];

    Vector3 mZenith;
    Vector3 mPerezInvDen;
};

class FSunSkyTable
{
public:
    // Table-based version - faster than per-sample Perez/Hosek function evaluation, suitable for shader use via 64 x 2 texture
    // For a fixed time, Preetham can be expressed in the form
    //   K (1 + F(theta))(1 + G(gamma))
    // where theta is the zenith angle of v, and gamma the angle between v and the sun direction.

    void FindThetaGammaTables(const FSunSkyPreetham& pt);

    // Table acceleration
    enum { kTableSize = 64 };
    struct Tables
    {
        Vector3 mThetaTable[kTableSize];
        Vector3 mGammaTable[kTableSize];
    }Tables;
    uint8_t Data[2][kTableSize][4];
    float mMaxTheta = 1.0f;       // To avoid clipping when using non-float textures. Currently only necessary if overcast is being used.
    float mMaxGamma = 1.0f;       // To avoid clipping when using non-float textures.
    bool mXYZ = false;      // Whether tables are storing xyY (Preetham) or XYZ (Hosek)
};