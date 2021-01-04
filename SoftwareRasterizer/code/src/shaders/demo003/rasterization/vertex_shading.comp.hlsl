

// Input
struct InputVertexAttribs {
    float3 pos;
    float3 normal;
    float3 color;
    float2 uv;
};
StructuredBuffer<InputVertexAttribs> input_vertices : register(t0, space1);

// Output -> Transformed
struct OutputVertexAttribs {
    float3 pos_ndc; // equivalent to gl_Position(? = gl_Position.xyz/gl_Position.w)
    float3 pos_world;
    float3 normal_world;
    float3 color;
    float2 uv;
};
RWStructuredBuffer<OutputVertexAttribs> output_vertices : register(u0, space1);

struct MVP {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<MVP> MatUniform : register(b0, space1);

// root constant
cbuffer vertices_info : register(b1, space1) {
    uint vertices_count;
};

struct CS_SystemValues {
    uint GI : SV_GroupIndex;
    uint3 GTid : SV_GroupThreadID;
    uint3 DTid : SV_DispatchThreadID;
};

// 1 thread per vertex
[numthreads( 16, 1, 1 )]
void main(CS_SystemValues cs) {
    int index = cs.DTid.x;
    if(index <= vertices_count) {
        float4 pos_world = mul(MatUniform.model, float4(input_vertices[index].pos, 1));
        float4 pos_cam = mul(MatUniform.view, pos_world);
        float4 pos_clip = mul(MatUniform.proj, pos_cam);
        output_vertices[index].pos_ndc = pos_clip.xyz / pos_clip.w;
        output_vertices[index].pos_world = pos_world;
        output_vertices[index].normal_world = input_vertices[index].normal;
        output_vertices[index].color = input_vertices[index].color;
        output_vertices[index].uv = input_vertices[index].uv;
    }
}