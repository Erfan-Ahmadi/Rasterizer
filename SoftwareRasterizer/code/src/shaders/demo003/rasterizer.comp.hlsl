// RWTexture2D<float4> Framebuffer : register(u0, space1);

// Input
struct VertexAttribs {
    float3 pos_ndc; // equivalent to gl_Position(?)
    float3 pos_world;
    float3 normal_world;
    float3 color;
    float2 uv;
};
// StructuredBuffer<VertexAttribs> vertices : register(t0, space1);

struct Primitive {
    VertexAttribs vertices[3];
};
// StructuredBuffer<Primitive> primitives : register(t0, space1);

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

// 1 thread per primitive
[numthreads( 16, 1, 1 )]
void main(CS_SystemValues cs) {
    int index = cs.DTid.x;
    // just for test, doesn't make sense
    fragments[index].color = float3(0.5f, 0.5f, 0.5f);
}