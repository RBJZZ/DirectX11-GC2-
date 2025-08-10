Texture2D baseTexture : register(t0); // Ej. Pasto
Texture2D lowAltitudeTexture : register(t1); // Ej. Tierra
Texture2D highAltitudeTexture : register(t2); // Ej. Roca
SamplerState textureSampler : register(s0);

// CB de Luces (igual que el de tus modelos)
cbuffer LightProperties : register(b1)
{
    float3 cameraPositionWorld;
    float _pad0;
    float3 directionalLightVector;
    float _pad1;
    float4 directionalLightColor;
    float4 ambientLightColor;
};

// CB de Material del Terreno (puedes definir propiedades si las necesitas)
// cbuffer MaterialPropertiesTerrain : register(b2) { ... }
// Por ahora, asumiremos propiedades de material fijas o tomadas de forma simple.

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 worldPosition : WORLDPOS;
    float scaledLocalY : TEXCOORD1; // Y local escalada
    float maxHeight : TEXCOORD2; // MaxTerrainHeightLocal
};

float4 main(PixelInputType input) : SV_TARGET
{
    // --- Mezcla de Texturas Basada en Altura ---
    float blendFactor = 0.0f;
    if (input.maxHeight > 0.001f)
    { // Evitar división por cero si el terreno es plano
         // Normalizar la altura actual del píxel (0 en lo más bajo, 1 en lo más alto)
        blendFactor = saturate(input.scaledLocalY / input.maxHeight);
    }

    float4 colorTexBase = baseTexture.Sample(textureSampler, input.texCoord);
    float4 colorTexLow = lowAltitudeTexture.Sample(textureSampler, input.texCoord);
    float4 colorTexHigh = highAltitudeTexture.Sample(textureSampler, input.texCoord);

    // Lógica de mezcla (puedes refinarla mucho)
    // Ejemplo: Suave transición de low a base, y luego de base a high.
    // Esta es una lógica simple, la de tu shader original era diferente.
    // Vamos a intentar algo más controlable:
    // blendFactor: 0 = más bajo, 0.5 = medio, 1.0 = más alto

    float4 blendedAlbedo;
    if (blendFactor < 0.4f)
    { // Zona baja (0.0 a 0.4)
        float t = blendFactor / 0.4f; // Normalizar a 0-1 en este rango
        blendedAlbedo = lerp(colorTexLow, colorTexBase, t);
    }
    else if (blendFactor < 0.7f)
    { // Zona media (0.4 a 0.7)
        blendedAlbedo = colorTexBase; // Predomina la textura base
    }
    else
    { // Zona alta (0.7 a 1.0)
        float t = (blendFactor - 0.7f) / 0.3f; // Normalizar a 0-1 en este rango
        blendedAlbedo = lerp(colorTexBase, colorTexHigh, t);
    }

    // --- Iluminación (similar a EvolvingPS.hlsl) ---
    float3 N = normalize(input.worldNormal);
    float3 L = normalize(directionalLightVector); // Vector HACIA la luz
    float3 V = normalize(cameraPositionWorld - input.worldPosition); // Vector HACIA la cámara

    // Ambiental
    float4 ambient = ambientLightColor * blendedAlbedo; // Asumimos materialDiffuseColor=blanco para terreno

    // Difusa
    float NdotL = saturate(dot(N, L));
    float4 diffuse = NdotL * directionalLightColor * blendedAlbedo;

    // Especular (el terreno suele tener poco o nada de especular)
    float4 specular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float terrainSpecularPower = 4.0f; // Muy bajo y amplio
    float4 terrainSpecularColor = float4(0.05f, 0.05f, 0.05f, 1.0f); // Muy oscuro
    if (NdotL > 0.0f)
    {
        float3 H = normalize(L + V); // Blinn-Phong
        float NdotH = saturate(dot(N, H));
        specular = pow(NdotH, terrainSpecularPower) * directionalLightColor * terrainSpecularColor;
    }

    float4 finalColor = ambient + diffuse + specular;
    finalColor.a = blendedAlbedo.a; // O 1.0f si el terreno siempre es opaco

    return finalColor;
}