Texture2D inputTexture : register(t0);
SamplerState textureSampler : register(s0);

cbuffer BlurParameters : register(b0)
{
    float2 texelSize;
    float2 padding;
};

float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_TARGET
{
    float4 finalColor = 0;
    float weights[9] = { 0.055555, 0.088888, 0.111111, 0.133333, 0.144444, 0.133333, 0.111111, 0.088888, 0.055555 };
    
    for (int i = -4; i <= 4; ++i)
    {
        float2 offset = float2(0.0f, texelSize.y * i);
        finalColor += inputTexture.Sample(textureSampler, texCoord + offset) * weights[i + 4];
    }
    
    return finalColor;
}