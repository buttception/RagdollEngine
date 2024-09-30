#define GROUP_SIZE 256
#define THREADS_X 256
#define THREADS_Y 1

// Uniforms:
cbuffer Params
{
    float MinLogLuminance;
    float LogLuminanceRange;
    float TimeCoeff;
    float NumPixels;
}

// We'll be writing our average to Target
RWBuffer<float> Target : register(u0);
RWBuffer<uint> histogram : register(u1);

// Shared
groupshared uint histogramShared[GROUP_SIZE];

[numthreads(THREADS_X, THREADS_Y, 1)]
void main_cs(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID) {
    // Get the count from the histogram buffer
    uint countForThisBin = histogram[GIid];
    histogramShared[GIid] = countForThisBin * GIid;

    // Sync threads within the group
    GroupMemoryBarrierWithGroupSync();

    // Reset the count stored in the buffer in anticipation of the next pass
    histogram[GIid] = 0;

    // This loop will perform a weighted count of the luminance range
    [unroll]
    for (uint cutoff = (GROUP_SIZE >> 1); cutoff > 0; cutoff >>= 1) {
        if (GIid < cutoff) {
            histogramShared[GIid] += histogramShared[GIid + cutoff];
        }

        // Sync threads within the group after each reduction step
        GroupMemoryBarrierWithGroupSync();
    }

    // Only a single thread needs to calculate the final result
    if (GIid == 0) {
        // Calculate weighted log average and divide by the number of pixels
        float weightedLogAverage = (float(histogramShared[0]) / max(NumPixels - countForThisBin, 1)) - 1.0;

        // Map from histogram space to actual luminance
        float weightedAvgLum = exp2(((weightedLogAverage / 254.0) * LogLuminanceRange) + MinLogLuminance);

        // Load the luminance from the previous frame
        float lumLastFrame = Target[0];

        // Apply temporal adaptation to smooth out the exposure over time
        float adaptedLum = lumLastFrame + (weightedAvgLum - lumLastFrame) * TimeCoeff;

        // Store the adapted luminance back to the target texture
        Target[0] = adaptedLum;
    }
}