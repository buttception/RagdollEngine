
struct DrawIndexedIndirectArguments
{
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

cbuffer g_Const : register(b0)
{
    uint MeshCount;
}

RWStructuredBuffer<DrawIndexedIndirectArguments> DrawIndexedIndirectArgsOutput : register(u0);

[numthreads(64, 1, 1)]
void ResetIndirectCS(uint3 DTid : SV_DispatchThreadID, uint GIid : SV_GroupIndex, uint3 GTid : SV_GroupThreadID)
{
    if (DTid.x < MeshCount)
    {
        DrawIndexedIndirectArgsOutput[DTid.x].instanceCount = 0;
    }
}