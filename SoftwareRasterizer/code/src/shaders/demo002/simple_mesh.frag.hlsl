struct VSOutput
{
    float4 Position     : SV_POSITION; 
    float4 FragColor    : COLOR0;
    float2 UV			: TEXCOORD0;
};

sampler   DefaultSampler 	: register(s0, space0);
Texture2D MyTexture 		: register(t0, space2);

struct PSOut
{
    float4 Color   : SV_TARGET;
};

PSOut PSMain(VSOutput input)
{
    PSOut output;
    float4 sampled_texture = MyTexture.Sample(DefaultSampler, input.UV);
    output.Color = lerp(input.FragColor, sampled_texture, 1.0f);
    return output;
}