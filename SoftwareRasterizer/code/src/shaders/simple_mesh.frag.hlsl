struct VSOutput
{
	float4 Position     : SV_POSITION; 
	float4 FragColor    : COLOR0;
	float2 UV			: TEXCOORD0;
};

struct PSOut
{
    float4 Color   : SV_Target0;
};

PSOut PSMain(VSOutput input)
{
	PSOut output;
    output.Color = input.FragColor;
    return output;
}