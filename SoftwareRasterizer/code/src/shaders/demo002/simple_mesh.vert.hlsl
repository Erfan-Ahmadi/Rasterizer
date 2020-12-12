struct VSInput
{
    float4 Position	    	: POSITION0;
    float4 Col	        	: COLOR0;
    float2 UV				: TEXCOORD0;
};

cbuffer WorldMat : register(b0, space2)
{
    float4x4 world;
};

cbuffer ViewProjMat : register(b0, space0)
{
    float4x4 proj;
    float4x4 view;
};

struct VSOutput 
{
    float4 Position     : SV_POSITION;
    float4 FragColor    : COLOR0;
    float2 UV			: TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = input.Position;// mul(proj, mul(view, mul(world, float4(input.Position, 1.0f))));
    output.FragColor = input.Col;
    output.UV = input.UV;
    return output;
}