cbuffer g_Const : register(b0) {
	float4x4 viewProjMatrix;
    uint InstanceOffset;
};

// Define the cube's 8 corner vertices
static const float3 cubeVertices[8] = {
    float3(-1.0,  1.0, -1.0),  // Vertex 0: top-left-front
    float3( 1.0,  1.0, -1.0),  // Vertex 1: top-right-front
    float3( 1.0, -1.0, -1.0),  // Vertex 2: bottom-right-front
    float3(-1.0, -1.0, -1.0),  // Vertex 3: bottom-left-front
    float3(-1.0,  1.0,  1.0),  // Vertex 4: top-left-back
    float3( 1.0,  1.0,  1.0),  // Vertex 5: top-right-back
    float3( 1.0, -1.0,  1.0),  // Vertex 6: bottom-right-back
    float3(-1.0, -1.0,  1.0)   // Vertex 7: bottom-left-back
};

// Index pairs for 12 lines (24 vertices)
static const int2 lineIndices[12] = {
    int2(0, 1), int2(1, 2), int2(2, 3), int2(3, 0), // Front face edges
    int2(4, 5), int2(5, 6), int2(6, 7), int2(7, 4), // Back face edges
    int2(0, 4), int2(1, 5), int2(2, 6), int2(3, 7)  // Connecting front to back
};

struct InstanceData{
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;
	float4x4 prevWorldMatrix;

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

void main_vs(
    in uint vertexID : SV_VertexID,
    in uint inInstanceId : SV_INSTANCEID,
    out float4 outPos : SV_POSITION,
	out nointerpolation uint outInstanceId : TEXCOORD1
)
{
	InstanceData data = InstanceDatas[inInstanceId + InstanceOffset];
    int lineID = vertexID / 2; // Get the line index (0-11)
    int vertexInLine = vertexID % 2; // 0 or 1, to get either of the two vertices in the line
    
    // Use the lineID to get the pair of vertices, then select which one using vertexInLine
    float3 position = cubeVertices[lineIndices[lineID][vertexInLine]];

    // Transform to homogeneous clip space (apply model-view-projection matrix)
	float4 fragPos = mul(float4(position, 1.f), data.worldMatrix); 
	outPos = mul(fragPos, viewProjMatrix);
	outInstanceId = inInstanceId;
}

void main_ps(
	in float4 inPos : SV_Position,
	in uint inInstanceId : TEXCOORD1,
	out float4 outColor : SV_Target0
)
{
	InstanceData data = InstanceDatas[inInstanceId + InstanceOffset];
    outColor = data.albedoFactor;
}