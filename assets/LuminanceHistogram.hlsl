#define GROUP_SIZE 256
#define THREADS_X 16
#define THREADS_Y 16

#define EPSILON 0.005
// Taken from RTR vol 4 pg. 278
static const float3 RGB_TO_LUM = float3(0.2125, 0.7154, 0.0721);

// Uniforms:
cbuffer g_Const : register(b0)
{
    float MinLogLuminance;
    float InvLogLuminanceRange;
    uint2 Dimension;
};

// Our two inputs, the read-only HDR color image, and the histogramBuffer
Texture2D<float3> SceneColor : register(t0);
RWBuffer<uint> histogram : register(u0);

// Shared histogram buffer used for storing intermediate sums for each work group
groupshared uint histogramShared[GROUP_SIZE];

// For a given color and luminance range, return the histogram bin index
uint colorToBin(float3 hdrColor, float minLogLum, float inverseLogLumRange) {
    // Convert our RGB value to Luminance, see note for RGB_TO_LUM above
    float lum = dot(hdrColor, RGB_TO_LUM);

    // Avoid taking the log of zero
    if (lum < EPSILON) {
        return 0;
    }

    // Calculate the log_2 luminance and express it as a value in [0.0, 1.0]
    // where 0.0 represents the minimum luminance, and 1.0 represents the max.
    float logLum = saturate((log2(lum) - minLogLum) * inverseLogLumRange);

    // Map [0, 1] to [1, 255]. The zeroth bin is handled by the epsilon check above.
    return (uint)(logLum * 254.0 + 1.0);
}

// 16 * 16 * 1 threads per group
[numthreads(THREADS_X, THREADS_Y, 1)]
void main_cs(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID) {
    // Initialize the bin for this thread to 0
    histogramShared[GIid] = 0;
    GroupMemoryBarrierWithGroupSync();

    // Ignore threads that map to areas beyond the bounds of our HDR image
    if (DTid.x < Dimension.x && DTid.y < Dimension.y) {
        float3 hdrColor = SceneColor.Load(int3(DTid.xy, 0));
        uint binIndex = colorToBin(hdrColor, MinLogLuminance, InvLogLuminanceRange);

        // Use an atomic add to ensure we don't write to the same bin in our
        // histogram from two different threads at the same time.
        InterlockedAdd(histogramShared[binIndex], 1);
    }

    // Wait for all threads in the work group to reach this point before adding our
    // local histogram to the global one
    GroupMemoryBarrierWithGroupSync();

    // Technically there's no chance that two threads write to the same bin here,
    // but different work groups might! So we still need the atomic add.
    InterlockedAdd(histogram[GIid], histogramShared[GIid]);
}