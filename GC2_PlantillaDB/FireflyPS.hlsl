// FireflyPS.hlsl - Pixel Shader para partculas de lucirnaga

Texture2D particleTexture : register(t0);
SamplerState textureSampler : register(s0);

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // Muestrear la textura y multiplicarla por el color de la partcula.
    // El color de la partcula ya incluye el factor de parpadeo (blink).
    float4 textureColor = particleTexture.Sample(textureSampler, input.texCoord);
    
    return textureColor * input.color;
}