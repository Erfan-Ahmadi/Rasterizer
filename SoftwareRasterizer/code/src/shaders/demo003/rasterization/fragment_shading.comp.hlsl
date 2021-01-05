
// Input
struct Fragment { // Interpolated Values for each Fragment
    float3 pos_ndc; // equivalent to interpolated gl_Position(?)
    float3 pos_world;
    float3 normal_world;
    float3 color;
    float2 uv;
};
StructuredBuffer<Fragment> fragments : register(t0, space1);

struct CS_SystemValues {
    uint GI : SV_GroupIndex;
    uint3 GTid : SV_GroupThreadID;
    uint3 DTid : SV_DispatchThreadID;
};

// Output
RWTexture2D<float4> Framebuffer : register(u0, space1);

float4 PackColor(float4 linear_color) {
    // no srgb support yet.
    return linear_color;
}

// 1 thread per fragment
[numthreads( 16, 16, 1 )]
void main(CS_SystemValues cs) {
    int width, height;
    Framebuffer.GetDimensions(width, height);
    int x = cs.DTid.x;
    int y = cs.DTid.y;
    int index = y * width + x;
    Fragment frag = fragments[index];

    if(x < width && y < height) {
        Framebuffer[cs.DTid.xy] = PackColor(float4(frag.color, 1.0f));
    }
}