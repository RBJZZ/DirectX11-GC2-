// GC2_PlantillaDB\TerrainPS.hlsl (VERSIÓN MEJORADA)

// --- RECURSOS ---
Texture2D dirtTexture : register(t0); // Renombrada para claridad
Texture2D grassTexture : register(t1); // Renombrada para claridad
Texture2D snowTexture : register(t2); // Renombrada para claridad
Texture2D rockTexture : register(t3);
Texture2D shadowMap : register(t4);
SamplerState textureSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

// --- CONSTANT BUFFERS ---
cbuffer LightProperties : register(b1)
{
    float3 cameraPositionWorld;
    float _pad0;
    float3 directionalLightVector;
    float _pad1;
    float4 directionalLightColor;
    float4 ambientLightColor;
};

// NUEVO Constant Buffer para los parámetros del material
cbuffer TerrainMaterialProperties : register(b2)
{
    float dirtMaxHeight; // Altura (0-1) donde termina la transición tierra -> hierba
    float grassMaxHeight; // Altura (0-1) donde empieza la transición hierba -> nieve
    float blendRange; // Suavidad/ancho de la transición entre capas
    float rockSlopeThreshold; // Umbral de pendiente (0-1) para que aparezca la roca
};

// --- ESTRUCTURA DE ENTRADA ---
struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 worldPosition : WORLDPOS;
    float scaledLocalY : TEXCOORD1;
    float maxHeight : TEXCOORD2;
    float4 positionInLightSpace : TEXCOORD3;
};

// --- FUNCIONES DE AYUDA (PCF para sombras) ---
float CalculatePCFShadowFactor(Texture2D shadowTex, SamplerComparisonState shadowSamp, float4 lightSpacePos, float bias)
{
    lightSpacePos.xyz /= lightSpacePos.w;
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);

    float shadowFactor = 0.0f;
    float2 texelSize;
    uint width, height;
    shadowTex.GetDimensions(width, height);
    texelSize = float2(1.0f / width, 1.0f / height);

    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            shadowFactor += shadowTex.SampleCmpLevelZero(
                shadowSamp,
                shadowTexCoord + float2(x, y) * texelSize,
                lightSpacePos.z - bias
            );
        }
    }
    return shadowFactor / 25.0f;
}

// --- SHADER PRINCIPAL ---
float4 main(PixelInputType input) : SV_TARGET
{
    // 1. Muestrear todas las texturas una vez
    float4 colorDirt = dirtTexture.Sample(textureSampler, input.texCoord);
    float4 colorGrass = grassTexture.Sample(textureSampler, input.texCoord);
    float4 colorSnow = snowTexture.Sample(textureSampler, input.texCoord);
    float4 colorRock = rockTexture.Sample(textureSampler, input.texCoord);
    
    // 2. Lógica de mezcla por ALTURA (¡La parte nueva y mejorada!)
    // Normalizamos la altura actual del píxel a un rango de 0.0 a 1.0
    float normalizedHeight = saturate(input.scaledLocalY / input.maxHeight);
    
    // Usamos smoothstep para crear transiciones suaves y controlables
    // Factor para mezclar de tierra a hierba
    float grassFactor = smoothstep(dirtMaxHeight - blendRange, dirtMaxHeight + blendRange, normalizedHeight);
    // Factor para mezclar el resultado anterior con la nieve
    float snowFactor = smoothstep(grassMaxHeight - blendRange, grassMaxHeight + blendRange, normalizedHeight);

    // Mezclamos primero tierra y hierba, y luego el resultado con nieve
    float4 heightBlendedColor = lerp(colorDirt, colorGrass, grassFactor);
    heightBlendedColor = lerp(heightBlendedColor, colorSnow, snowFactor);

    // 3. Lógica de mezcla por PENDIENTE (Roca)
    float3 worldUp = float3(0, 1, 0);
    float slope = 1.0 - saturate(dot(input.worldNormal, worldUp));
    float rockFactor = smoothstep(rockSlopeThreshold, rockSlopeThreshold + 0.15, slope); // +0.15 para suavizar la transición de la roca

    // 4. Combinación final: Mezclamos el color de altura con el de la roca
    float4 blendedAlbedo = lerp(heightBlendedColor, colorRock, rockFactor);
    
    // --- ILUMINACIÓN Y SOMBRAS (sin cambios) ---
    float3 N = normalize(input.worldNormal);
    float3 L = normalize(directionalLightVector);
    float3 V = normalize(cameraPositionWorld - input.worldPosition);

    float4 ambient = ambientLightColor * blendedAlbedo;
    float NdotL = saturate(dot(N, L));
    float4 diffuse = NdotL * directionalLightColor * blendedAlbedo;
    
    float4 specular = float4(0, 0, 0, 0);
    if (NdotL > 0.0f)
    {
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        specular = pow(NdotH, 4.0f) * directionalLightColor * 0.1f;
    }

    // Cálculo de sombras
    float4 lightSpacePos = input.positionInLightSpace;
    lightSpacePos.xyz /= lightSpacePos.w;
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);
    float2 fromCenter = abs(shadowTexCoord - 0.5f) * 2.0f;
    float dist = max(fromCenter.x, fromCenter.y);
    float lightVisibility = 1.0 - smoothstep(0.85f, 0.98f, dist);
    float shadowFactor = CalculatePCFShadowFactor(shadowMap, shadowSampler, input.positionInLightSpace, 0.0005f);
    float finalLightFactor = lightVisibility * shadowFactor;
    
    float4 finalColor = ambient + (diffuse + specular) * finalLightFactor;
    finalColor.a = blendedAlbedo.a;

    return finalColor;
}