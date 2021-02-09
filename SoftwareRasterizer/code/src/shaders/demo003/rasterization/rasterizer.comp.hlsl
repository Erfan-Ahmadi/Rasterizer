// RWTexture2D<float4> Framebuffer : register(u0, space1);

// Input
struct OutputVertexAttribs {
    float4 pos_ndc;
    float4 pos_world;
    float4 normal_world;
    float4 color;
    float2 uv;
};
StructuredBuffer<OutputVertexAttribs> vertices : register(t0, space1);
StructuredBuffer<int> indices : register(t1, space1);

cbuffer info : register(b0, space1) {
    uint indices_count;
    uint width;
    uint height;
};

// Output
struct Fragment { // Interpolated Values for each Fragment
    float4 pos_ndc; // equivalent to interpolated gl_Position(?)
    float4 pos_world;
    float4 normal_world;
    float4 color;
    float2 uv;
};
RWStructuredBuffer<Fragment> fragments : register(u0, space1);

RWStructuredBuffer<float> depth_array : register(u1, space1);


struct CS_SystemValues {
    uint GI : SV_GroupIndex;
    uint3 GTid : SV_GroupThreadID;
    uint3 DTid : SV_DispatchThreadID;
};

// 1 thread per primitive -> number of threads => primitive_count
// group_size_x = 16, indices_count = 30, primitive_count = 30/3 = 10, group_count_x = (10 / 16 + 1)
// thread 0 handles 0,1,2 indices
// thread 1 handles 3,4,5 and so on...

void CalculateTriangleAABB(float3 pos_ndc_0, float3 pos_ndc_1, float3 pos_ndc_2, out float2 min_point, out float2 max_point) {
    min_point = float2(min(pos_ndc_0.x, min(pos_ndc_1.x, pos_ndc_2.x)), min(pos_ndc_0.y, min(pos_ndc_1.y, pos_ndc_2.y)));
    max_point = float2(max(pos_ndc_0.x, max(pos_ndc_1.x, pos_ndc_2.x)), max(pos_ndc_0.y, max(pos_ndc_1.y, pos_ndc_2.y)));
}

float calculateSignedArea(float3 pos_ndc_0, float3 pos_ndc_1, float3 pos_ndc_2)
{
    return 0.5 *
        ((pos_ndc_2.x - pos_ndc_0.x) * (pos_ndc_1.y - pos_ndc_0.y) -
         (pos_ndc_1.x - pos_ndc_0.x) * (pos_ndc_2.y - pos_ndc_0.y));
}

float calculateBarycentricCoordinateValue(
        float2 a, float2 b, float2 c,     
        float3 pos_ndc_0, float3 pos_ndc_1, float3 pos_ndc_2)
{
    float3 bary_pos_ndc_0 = float3(a, 0);
    float3 bary_pos_ndc_1 = float3(b, 0);
    float3 bary_pos_ndc_2 = float3(c, 0);
    return calculateSignedArea(bary_pos_ndc_0, bary_pos_ndc_1, bary_pos_ndc_2) / calculateSignedArea(pos_ndc_0, pos_ndc_1, pos_ndc_2);
}

bool isBarycentricCoordInBounds(float3 barycentric_coord)
{
    return barycentric_coord.x >= 0.0 && barycentric_coord.x <= 1.0 &&
           barycentric_coord.y >= 0.0 && barycentric_coord.y <= 1.0 &&
           barycentric_coord.z >= 0.0 && barycentric_coord.z <= 1.0;
}

float3 CalculateBarycentricCoordinates(float3 pos_ndc_0, float3 pos_ndc_1, float3 pos_ndc_2, float2 v)
{
    float beta  = calculateBarycentricCoordinateValue(
            float2(pos_ndc_0.x, pos_ndc_0.y),
            v,
            float2(pos_ndc_2.x, pos_ndc_2.y),
            pos_ndc_0, pos_ndc_1, pos_ndc_2);
    float gamma = calculateBarycentricCoordinateValue(
            float2(pos_ndc_0.x, pos_ndc_0.y),
            float2(pos_ndc_1.x, pos_ndc_1.y),
            v,
            pos_ndc_0, pos_ndc_1, pos_ndc_2);
    float alpha = 1.0 - beta - gamma;
    return float3(alpha, beta, gamma);
}

float4 baryinterp(float3 bary, float4 v0, float4 v1, float4 v2)
{
    return bary.x * v0 + bary.y * v1 + bary.z * v2;
}

float3 baryinterp(float3 bary, float3 v0, float3 v1, float3 v2)
{
    return bary.x * v0 + bary.y * v1 + bary.z * v2;
}

float2 baryinterp(float3 bary, float2 v0, float2 v1, float2 v2)
{
    return bary.x * v0 + bary.y * v1 + bary.z * v2;
}


[numthreads( 16, 1, 1 )]
void main(CS_SystemValues cs) {
    int tid = cs.DTid.x;
    // fragments[tid].color = float3(1.0f, 0.0f, 0.0f);
    if(3 * tid + 2 < indices_count) {
        OutputVertexAttribs v0 = vertices[indices[3 * tid + 0]];
        OutputVertexAttribs v1 = vertices[indices[3 * tid + 1]];
        OutputVertexAttribs v2 = vertices[indices[3 * tid + 2]];

        float2 min_pointf = float2(0.0f, 0.0f);
        float2 max_pointf = float2(0.0f, 0.0f);
        CalculateTriangleAABB(v0.pos_ndc.xyz, v1.pos_ndc.xyz, v2.pos_ndc.xyz, min_pointf, max_pointf);

        // Convert to Screen Space Coordinates
        float2 min_point_s = max(float2(0.0f, 0.0f), float2((min_pointf.x + 1.0f) * 0.5f * width, (min_pointf.y + 1.0f) * 0.5f * height));
        float2 max_point_s = min(float2(width, height), float2((max_pointf.x + 1.0f) * 0.5f * width, (max_pointf.y + 1.0f) * 0.5f * height));
        

        for(int x = min_point_s.x; x < max_point_s.x; ++x) {
            for(int y = min_point_s.y; y < max_point_s.y; ++y) {
                int frag_index = (int)((height - y - 1) * width + x);

                float2 ndc = float2(
                    float(x * 2 - int(width)) / float(width),
                    float(y * 2 - int(height)) / float(height)
                );

                float3 bary = CalculateBarycentricCoordinates(v0.pos_ndc.xyz, v1.pos_ndc.xyz, v2.pos_ndc.xyz, ndc);

                if(isBarycentricCoordInBounds(bary)) {

                    float depthnew = 0.0f;

                    fragments[frag_index].pos_ndc = float4(ndc, depthnew, 0.0f);
                    fragments[frag_index].pos_world = baryinterp(bary, v0.pos_world, v1.pos_world, v2.pos_world);
                    fragments[frag_index].color = baryinterp(bary, v0.color, v1.color, v2.color);
                    fragments[frag_index].normal_world = baryinterp(bary, v0.normal_world, v1.normal_world, v2.normal_world);
                    fragments[frag_index].uv = baryinterp(bary, v0.uv, v1.uv, v2.uv);
                }
            }
        }
    }
}