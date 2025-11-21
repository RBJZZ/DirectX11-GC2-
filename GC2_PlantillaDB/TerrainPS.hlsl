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
    // 1. Proyección perspectiva y normalización a [0, 1]
    lightSpacePos.xyz /= lightSpacePos.w;
    
    // 2. Transformación a coordenadas de textura (UV)
    //    Esta es la fórmula estándar para DirectX. La 'Y' se invierte.
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);

    float shadowFactor = 0.0f;
    float2 texelSize;
    uint width, height;
    shadowTex.GetDimensions(width, height);
    texelSize = float2(1.0f / width, 1.0f / height);

    // 3. Bucle del kernel de muestreo (PCF 5x5)
    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            // Muestrear y comparar la profundidad, aplicando el bias
            shadowFactor += shadowTex.SampleCmpLevelZero(
                shadowSamp,
                shadowTexCoord + float2(x, y) * texelSize,
                lightSpacePos.z - bias
            );
        }
    }

    // 4. Promediar los resultados
    return shadowFactor / 25.0f;
}


float4 main(PixelInputType input) : SV_TARGET
{

    float4 colorDirt = dirtTexture.Sample(textureSampler, input.texCoord);
    float4 colorGrass = grassTexture.Sample(textureSampler, input.texCoord);
    float4 colorSnow = snowTexture.Sample(textureSampler, input.texCoord);
    float4 colorRock = rockTexture.Sample(textureSampler, input.texCoord);
    

    float normalizedHeight = saturate(input.scaledLocalY / input.maxHeight);
    
    float grassFactor = smoothstep(dirtMaxHeight - blendRange, dirtMaxHeight + blendRange, normalizedHeight);
    float snowFactor = smoothstep(grassMaxHeight - blendRange, grassMaxHeight + blendRange, normalizedHeight);

    float4 heightBlendedColor = lerp(colorDirt, colorGrass, grassFactor);
    heightBlendedColor = lerp(heightBlendedColor, colorSnow, snowFactor);

    float3 worldUp = float3(0, 1, 0);
    float slope = 1.0 - saturate(dot(input.worldNormal, worldUp));
    float rockFactor = smoothstep(rockSlopeThreshold, rockSlopeThreshold + 0.15, slope);

    float4 blendedAlbedo = lerp(heightBlendedColor, colorRock, rockFactor);
    
    float3 L = -normalize(directionalLightVector);
    float3 N = normalize(input.worldNormal);
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
    float bias = 0.0005f;
    float shadowFactor = CalculatePCFShadowFactor(shadowMap, shadowSampler, input.positionInLightSpace, bias);
    float finalLightFactor = lightVisibility * shadowFactor;
    
    float4 finalColor = ambient + (diffuse + specular) * finalLightFactor;
    finalColor.a = blendedAlbedo.a;

    return finalColor;
}