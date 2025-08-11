// ShadowVS_AlphaClip.hlsl

cbuffer PerObjectConstants : register(b0)
{
    matrix World;
    matrix LightViewProjection;
};

struct VertexInputType
{
    float3 localPosition : POSITION;
    float2 texCoord : TEXCOORD0; // Ahora necesitamos las UVs
};

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0; // Pasamos las UVs al Pixel Shader
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    float4 worldPos = mul(float4(input.localPosition, 1.0f), transpose(World));
    output.clipSpacePosition = mul(worldPos, transpose(LightViewProjection));
    output.texCoord = input.texCoord; // Pasamos las UVs
    
    return output;
}