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
    float time;
    float3 directionalLightVector;
    float _pad1;
    float4 directionalLightColor;
    float4 ambientLightColor;

    float4 pointLightColor;
    float3 pointLightPos;
    float pointLightRange;
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
    // 1. Muestreo de Texturas
    float4 colorDirt = dirtTexture.Sample(textureSampler, input.texCoord);
    float4 colorGrass = grassTexture.Sample(textureSampler, input.texCoord);
    float4 colorSnow = snowTexture.Sample(textureSampler, input.texCoord);
    float4 colorRock = rockTexture.Sample(textureSampler, input.texCoord);
    
    // 2. Mezcla por Altura (Multitextura)
    float normalizedHeight = saturate(input.scaledLocalY / input.maxHeight);
    
    float grassFactor = smoothstep(dirtMaxHeight - blendRange, dirtMaxHeight + blendRange, normalizedHeight);
    float snowFactor = smoothstep(grassMaxHeight - blendRange, grassMaxHeight + blendRange, normalizedHeight);

    float4 heightBlendedColor = lerp(colorDirt, colorGrass, grassFactor);
    heightBlendedColor = lerp(heightBlendedColor, colorSnow, snowFactor);

    // 3. Mezcla por Pendiente (Rocas)
    float3 worldUp = float3(0, 1, 0);
    float slope = 1.0 - saturate(dot(input.worldNormal, worldUp));
    float rockFactor = smoothstep(rockSlopeThreshold, rockSlopeThreshold + 0.15, slope);

    float4 blendedAlbedo = lerp(heightBlendedColor, colorRock, rockFactor);
    
    // 4. Vectores de Luz Global
    float3 L = -normalize(directionalLightVector);
    float3 N = normalize(input.worldNormal);
    float3 V = normalize(cameraPositionWorld - input.worldPosition);

    // 5. Iluminación Direccional (Sol/Luna)
    float4 ambient = ambientLightColor * blendedAlbedo;
    float NdotL = saturate(dot(N, L));
    float4 diffuse = NdotL * directionalLightColor * blendedAlbedo;
    
    float4 specular = float4(0, 0, 0, 0);
    if (NdotL > 0.0f)
    {
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        specular = pow(NdotH, 4.0f) * directionalLightColor * 0.1f; // Especular bajo para terreno
    }

    // 6. Sombras (Solo afecta a la luz direccional)
    float4 lightSpacePos = input.positionInLightSpace;
    lightSpacePos.xyz /= lightSpacePos.w;
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);
    float2 fromCenter = abs(shadowTexCoord - 0.5f) * 2.0f;
    float dist = max(fromCenter.x, fromCenter.y);
    float lightVisibility = 1.0 - smoothstep(0.85f, 0.98f, dist);
    float bias = 0.0005f;
    float shadowFactor = CalculatePCFShadowFactor(shadowMap, shadowSampler, input.positionInLightSpace, bias);
    float finalLightFactor = lightVisibility * shadowFactor;
    
    // ==========================================================
    // 7. CÁLCULO DE LUZ DE PUNTO (HORNO)
    // ==========================================================
    float3 terrainPointLight = float3(0, 0, 0);
    
    // Vector desde el píxel del suelo hacia la luz
    float3 lightDirP = pointLightPos - input.worldPosition;
    float distP = length(lightDirP);

    // Si el píxel está dentro del rango de la luz del horno
    if (distP < pointLightRange)
    {
        // Atenuación (Caída de luz cuadrática)
        float att = saturate(1.0f - distP / pointLightRange);
        att *= att;
        
        lightDirP = normalize(lightDirP);
        
        // Difusa (El suelo se ilumina de naranja)
        float NdotL_P = saturate(dot(N, lightDirP));
        
        // Especular (Opcional: hace que el suelo brille un poco si es rocoso)
        float3 H_P = normalize(lightDirP + V);
        float NdotH_P = saturate(dot(N, H_P));
        float specP = pow(NdotH_P, 16.0f) * 0.3f; // Brillo sutil en el suelo

        // Combinar: (Difusa + Especular) * ColorLuz * Intensidad * Atenuación * ColorSuelo
        terrainPointLight = (NdotL_P + specP) * pointLightColor.rgb * pointLightColor.a * att * blendedAlbedo.rgb;
    }
    // ==========================================================

    // 8. Combinación Final
    // Color = Ambiental + (Sol * Sombra) + LuzHorno
    float4 finalColor = ambient + (diffuse + specular) * finalLightFactor;
    
    // Sumamos la luz del horno (Additiva, brilla en la oscuridad)
    finalColor.rgb += terrainPointLight;

    finalColor.a = blendedAlbedo.a;

    return finalColor;
}