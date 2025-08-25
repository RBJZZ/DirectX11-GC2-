// EvolvingPS.hlsl - Modificado para iluminación ambiental, difusa y especular

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);
Texture2D shadowMap : register(t1);
SamplerComparisonState shadowSampler : register(s1);

// Constant Buffer para las propiedades globales de la luz (desde Game.cpp)
cbuffer LightProperties : register(b1) // Asegúrate que este slot (b1) se use en C++
{
    float3 cameraPositionWorld; // Posición de la cámara en el mundo
    float _paddingToAlignCamPos; // Padding
    float3 directionalLightVector; // Vector normalizado DESDE la superficie HACIA la luz
    float _paddingToAlignLightVec; // Padding
    float4 directionalLightColor; // Color e intensidad de la luz direccional
    float4 ambientLightColor; // Color e intensidad de la luz ambiental global
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
    // 1. Proyeccin perspectiva y coordenadas de textura
    lightSpacePos.xyz /= lightSpacePos.w;
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);

    float shadowFactor = 0.0f;
    float2 texelSize;
    uint width, height;
    shadowTex.GetDimensions(width, height);
    texelSize = float2(1.0f / width, 1.0f / height);

    // 2. Bucle del kernel 5x5
    for (int y = -2; y <= 2; y++)
    {
        for (int x = -2; x <= 2; x++)
        {
            // Muestrear y comparar la profundidad
            shadowFactor += shadowTex.SampleCmpLevelZero(
                shadowSamp,
                shadowTexCoord + float2(x, y) * texelSize, // Coordenada de la muestra actual
                lightSpacePos.z - bias // Profundidad del pxel actual con bias
            );
        }
    }

    // 3. Promediar los resultados
    return shadowFactor / 25.0f; // Dividir por el nmero total de muestras (5*5=25)
}

float4 main(PixelInputType_Evolving input) : SV_TARGET
{
    // Obtener el color base de la textura
    float4 albedo = diffuseTexture.Sample(textureSampler, input.texCoord); 

    // Alpha clipping para las hojas de los rboles
    float alphaClipThreshold = 0.5f;
    clip(albedo.a - alphaClipThreshold);

    // Clculos de vectores de iluminacin
    float3 N = normalize(input.worldNormal);
    float3 L = normalize(directionalLightVector);
    float4 ambient = ambientLightColor * materialDiffuseColor * albedo;
    float NdotL = saturate(dot(N, L));
    float4 diffuse = NdotL * directionalLightColor * materialDiffuseColor * albedo;
    float4 specular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (NdotL > 0.0f)
    {
        float3 V = normalize(cameraPositionWorld - input.worldPosition);
        float3 R = reflect(-L, N);
        float RdotV = saturate(dot(R, V));
        specular = pow(RdotV, specularPower) * directionalLightColor * materialSpecularColor;
    }

    // --- LGICA FINAL Y CORRECTA DE SOMBRAS ---
    
    // 1. Empezamos con el color base siendo solo la luz ambiental.
    float4 finalColor = ambient;
    
    float4 lightSpacePos = input.positionInLightSpace; // Copiamos para claridad
    lightSpacePos.xyz /= lightSpacePos.w;
    float2 shadowTexCoord = float2(lightSpacePos.x * 0.5f + 0.5f, -lightSpacePos.y * 0.5f + 0.5f);

    // 3. Calculamos la visibilidad de la luz (el desvanecimiento en los bordes)
    float2 fromCenter = abs(shadowTexCoord - 0.5f) * 2.0f;
    float dist = max(fromCenter.x, fromCenter.y);
    float falloffStart = 0.85f;
    float falloffEnd = 0.98f;
    float lightVisibility = 1.0 - smoothstep(falloffStart, falloffEnd, dist);

    // 4. Calculamos el factor de sombra (si el pxel est tapado o no)
    float bias = 0.0005f;
    float shadowFactor = CalculatePCFShadowFactor(shadowMap, shadowSampler, input.positionInLightSpace, bias);
    
    // 5. COMBINACIN FINAL:
    // El factor de luz final es el producto de la visibilidad Y si est en sombra.
    float finalLightFactor = lightVisibility * shadowFactor;
    
    // 6. Aadimos la luz direccional y especular, modulada por nuestro factor final.
    finalColor = materialEmissiveColor + ambient + (diffuse + specular) * finalLightFactor;
    
    finalColor.a = albedo.a;

    return finalColor;
}

