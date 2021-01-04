// RWTexture2D<float4> Framebuffer : register(u0, space1);

// Input
struct OutputVertexAttribs {
    float3 pos_ndc;
    float3 pos_world;
    float3 normal_world;
    float3 color;
    float2 uv;
};
StructuredBuffer<OutputVertexAttribs> vertices : register(t0, space1);
StructuredBuffer<int> indices : register(t1, space1);

cbuffer vertices_info : register(b0, space1) {
    uint indices_count;
};

// Output
struct Fragment { // Interpolated Values for each Fragment
    float3 pos_ndc; // equivalent to interpolated gl_Position(?)
    float3 pos_world;
    float3 normal_world;
    float3 color;
    float2 uv;
};
RWStructuredBuffer<Fragment> fragments : register(u0, space1);

struct CS_SystemValues {
    uint GI : SV_GroupIndex;
    uint3 GTid : SV_GroupThreadID;
    uint3 DTid : SV_DispatchThreadID;
};

// 1 thread per primitive -> number of threads => primitive_count
// group_size_x = 16, indices_count = 30, primitive_count = 30/3 = 10, group_count_x = (10 / 16 + 1)
// thread 0 handles 0,1,2 indices
// thread 1 handles 3,4,5 and so on...

[numthreads( 16, 1, 1 )]
void main(CS_SystemValues cs) {
    int index = cs.DTid.x;
    if(3 * index + 2 < indices_count) {
        int index = cs.DTid.x;
        OutputVertexAttribs v0 = vertices[indices[3 * index + 0]];
        OutputVertexAttribs v1 = vertices[indices[3 * index + 1]];
        OutputVertexAttribs v2 = vertices[indices[3 * index + 2]];

        // Calculate AABB and Rasterize and write to Fragments via Atomic Functions.
    }
}