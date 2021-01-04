
// Input Output
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

// root constant
cbuffer info : register(b0, space1) {
    uint width;
    uint height;
};

// 1 thread per fragment
[numthreads( 16, 16, 1 )]
void main(CS_SystemValues cs) {
    int x = cs.DTid.x;
    int y = cs.DTid.y;
    int index = x + (y * width);
    Fragment frag = fragments[index];

    if(x <= width && y <= height) {

    }
}