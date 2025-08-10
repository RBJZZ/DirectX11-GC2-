// LightingPS.hlsl

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

// Propiedades de la luz (globales a la escena o pasadas por frame)
cbuffer LightProperties : register(b0) // Usaremos b0 para luces en PS
{
    float3 cameraPositionWorld;
    float3 directionalLightVector; // Vector APUNTANDO HACIA la fuente de luz (desde la superficie)
    float4 directionalLightColor;
    float4 ambientLightColor;
};

// Propiedades del material del objeto actual
cbuffer MaterialProperties : register(b1) // Usaremos b1 para materiales en PS
{
    float4 materialDiffuseColor; // Color base del material, puede usarse para tintar la textura
    float4 materialSpecularColor; // Color del brillo especular
    float specularPower; // Exponente para el brillo especular (qué tan concentrado es)
    float _padding1, _padding2, _padding3; // Para alineación de 16 bytes
};

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldNormal : NORMAL;
    float3 worldPosition : POSITION0;
};

float4 main(PixelInputType input) : SV_TARGET
{
    // Normalizar la normal (interpolada puede no estar normalizada)
    float3 N = normalize(input.worldNormal);

    // Obtener el color de la textura difusa
    float4 texColor = diffuseTexture.Sample(textureSampler, input.texCoord);

    // Color base del objeto (textura modulada por el color difuso del material)
    float3 albedo = texColor.rgb * materialDiffuseColor.rgb;

    // --- Iluminación Ambiental ---
    float3 ambient = ambientLightColor.rgb * albedo;

    // --- Iluminación Direccional (Difusa + Especular) ---
    // El vector de la luz direccional ya se espera que apunte HACIA la luz desde la superficie
    float3 L = normalize(directionalLightVector);

    // Componente Difuso (Lambert)
    float NdotL = saturate(dot(N, L)); // saturate limita el valor entre 0 y 1
    float3 diffuse = directionalLightColor.rgb * NdotL * albedo;

    // Componente Especular (Phong)
    float3 V = normalize(cameraPositionWorld - input.worldPosition); // Vector del punto al ojo/cámara
    float3 R = reflect(-L, N); // Vector de reflexión de la luz
    float RdotV = saturate(dot(R, V));
    
    float3 specular = float3(0.0f, 0.0f, 0.0f); // Inicializar especular a negro
    if (NdotL > 0.0f) // Solo calcular especular si la superficie está iluminada
    {
        specular = materialSpecularColor.rgb * directionalLightColor.rgb * pow(RdotV, specularPower);
    }

    // Combinar los componentes de luz
    float3 finalColor = ambient + diffuse + specular;
    
    // Devolver el color final, usando el alfa de la textura/material
    return float4(finalColor, texColor.a * materialDiffuseColor.a);
}