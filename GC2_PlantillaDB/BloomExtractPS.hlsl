Texture2D sceneTexture : register(t0);
SamplerState textureSampler : register(s0);

cbuffer BloomParameters : register(b0)
{
    float bloomThreshold;
    float bloomIntensity;
    float sceneIntensity;
    float padding;
};

float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_TARGET
{
    float4 color = sceneTexture.Sample(textureSampler, texCoord);
    
    // Extraer solo la parte del color que excede el umbral.
    // Esta es la forma correcta de aislar la "energía" del resplandor.
    return saturate(color - bloomThreshold);
}