// EvolvingPS.hlsl - Modificado para iluminación ambiental, difusa y especular

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);
Texture2D shadowMap : register(t1);
SamplerComparisonState shadowSampler : register(s1);

// Constant Buffer para las propiedades globales de la luz (desde Game.cpp)
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

// Constant Buffer para las propiedades del material de la malla actual (desde Model.cpp)
cbuffer MaterialProperties : register(b2) // Asegúrate que este slot (b2) se use en C++
{
    float4 materialDiffuseColor; // Color difuso base del material (puede ser blanco si la textura lo define todo)
    float4 materialSpecularColor; // Color del brillo especular del material
    float specularPower; // Exponente para el brillo especular (qué tan concentrado es)
    float3 _paddingForMaterial; // Padding para alinear
    float4 materialEmissiveColor;
};

struct PixelInputType_Evolving // Entrada desde el Vertex Shader
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldNormal : NORMAL; // Normal en espacio del mundo (interpolada)
    float3 worldPosition : WORLDPOS; // Posición del píxel en espacio del mundo (interpolada
    float4 positionInLightSpace : TEXCOORD1;
};

float ApplyShadowFalloff(float currentShadowFactor, float2 shadowUV)
{
    // Distancia desde el centro del shadow map (0.5, 0.5)
    float2 fromCenter = abs(shadowUV - 0.5f) * 2.0f; // Distancia en cada eje, normalizada a 0-1
    float dist = max(fromCenter.x, fromCenter.y); // Usamos la distancia mxima (Chebyshev) para un borde cuadrado

    // Define dnde empieza y termina el degradado (ej. del 85% al 95% del borde)
    float falloffStart = 0.85f;
    float falloffEnd = 0.95f;

    // Calcula un factor de suavizado
    float smoothFactor = smoothstep(falloffEnd, falloffStart, dist);

    // Mezcla entre 1.0 (sin sombra) y el factor de sombra calculado.
    // En los bordes, el resultado ser 1.0.
    return lerp(1.0f, currentShadowFactor, smoothFactor);
}

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

float4 main(PixelInputType_Evolving input) : SV_TARGET
{
    // Obtener el color base de la textura
    float4 albedo = diffuseTexture.Sample(textureSampler, input.texCoord);

    // Alpha clipping para las hojas de los árboles
    float alphaClipThreshold = 0.5f;
    clip(albedo.a - alphaClipThreshold);

    // --- CÁLCULOS BÁSICOS ---
    float3 N = normalize(input.worldNormal);
    float3 V = normalize(cameraPositionWorld - input.worldPosition);
    
    // 1. Luz Ambiental
    float4 ambient = ambientLightColor * materialDiffuseColor * albedo;

    // 2. Luz Direccional (Sol/Luna)
    float3 L = -normalize(directionalLightVector);
    float NdotL = saturate(dot(N, L));
    float4 diffuse = NdotL * directionalLightColor * materialDiffuseColor * albedo;
    
    float4 specular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (NdotL > 0.0f)
    {
        float3 R = reflect(-L, N);
        float RdotV = saturate(dot(R, V));
        specular = pow(RdotV, specularPower) * directionalLightColor * materialSpecularColor;
    }

    // --- CÁLCULO DE SOMBRAS (SOL) ---
    float4 lightSpacePos = input.positionInLightSpace;
    lightSpacePos.xyz /= lightSpacePos.w;
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);

    // Falloff (bordes)
    float2 fromCenter = abs(shadowTexCoord - 0.5f) * 2.0f;
    float dist = max(fromCenter.x, fromCenter.y);
    float lightVisibility = 1.0 - smoothstep(0.85f, 0.98f, dist);

    // PCF (Sombra suave)
    float bias = 0.0005f;
    float shadowFactor = CalculatePCFShadowFactor(shadowMap, shadowSampler, input.positionInLightSpace, bias);
    
    float finalLightFactor = lightVisibility * shadowFactor;

    // --- 3. LUZ DE PUNTO (HORNO) ---
    // Esta luz se SUMA aparte, NO tiene sombras del sol.
    
    float3 pointLightResult = float3(0, 0, 0);
    
    // Vector desde el píxel hacia la luz del horno
    float3 lightDirPoint = pointLightPos - input.worldPosition;
    float distanceToLight = length(lightDirPoint);

    // Solo calcular si estamos dentro del rango
    if (distanceToLight < pointLightRange)
    {
        // Atenuación: La luz se apaga suavemente con la distancia
        float attenuation = saturate(1.0f - distanceToLight / pointLightRange);
        attenuation *= attenuation; // Cuadrática para más realismo (se ve mejor)

        lightDirPoint = normalize(lightDirPoint);

        // Difusa (Punto)
        float NdotL_Point = saturate(dot(N, lightDirPoint));
        float3 pDiffuse = NdotL_Point * pointLightColor.rgb * pointLightColor.a * attenuation;

        // Especular (Punto) - ¡Hace que la armadura brille naranja!
        float3 pSpecular = float3(0, 0, 0);
        if (NdotL_Point > 0.0f)
        {
            float3 R_Point = reflect(-lightDirPoint, N);
            float RdotV_Point = saturate(dot(R_Point, V));
            pSpecular = pow(RdotV_Point, specularPower) * pointLightColor.rgb * attenuation;
        }

        // Sumar difusa y especular, multiplicadas por el color del objeto
        pointLightResult = (pDiffuse + pSpecular) * albedo.rgb;
    }

    // --- COMBINACIÓN FINAL ---
    // Color = Emisivo + Ambiental + (Sol * Sombra) + LuzHorno
    float4 finalColor = materialEmissiveColor + ambient + (diffuse + specular) * finalLightFactor;
    
    // Sumamos la luz del horno al final
    finalColor.rgb += pointLightResult;
    
    finalColor.a = albedo.a;

    return finalColor;
}
