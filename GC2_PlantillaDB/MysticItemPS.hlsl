// MysticItemPS.hlsl

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

cbuffer LightProperties : register(b1)
{
    float3 cameraPositionWorld;
    float time; // <--- USAREMOS ESTO (Lo pasaremos en el padding)
    float3 directionalLightVector;
    float _pad1;
    float4 directionalLightColor;
    float4 ambientLightColor;
};

cbuffer MaterialProperties : register(b2)
{
    float4 materialDiffuseColor;
    float4 materialSpecularColor;
    float specularPower;
    float3 _paddingMaterial;
    float4 materialEmissiveColor;
};

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 worldPosition : WORLDPOS;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // 1. Efecto de Pulso (latido)
    float pulse = (sin(time * 3.0f) + 1.0f) * 0.5f; // Va de 0 a 1
    
    // 2. Efecto de "Bandas de Energía" que suben
    float energy = sin(input.worldPosition.y * 10.0f - (time * 5.0f));
    energy = smoothstep(0.0f, 1.0f, energy); // Hacerlo más nítido
    
    // 3. Color Base (Dorado Brillante)
    float3 goldColor = float3(1.0f, 0.8f, 0.2f);
    float3 magicColor = float3(1.0f, 0.2f, 0.5f); // Rosado mágico
    
    // Mezclar colores según el pulso
    float3 finalColor = lerp(goldColor, magicColor, pulse * energy);
    
    // Añadir brillo extra (Emisivo)
    finalColor += energy * 0.5f;

    // Devolver color sólido (Alpha 1.0)
    return float4(finalColor, 1.0f);
}