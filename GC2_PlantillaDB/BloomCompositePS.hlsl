//--------------------------------------------------------------------------------------
// BloomCompositePS.hlsl
// Versi�n simplificada: Combina escena y bloom, ajusta saturaci�n y colores.
//--------------------------------------------------------------------------------------

//--- RECURSOS DE ENTRADA ---
Texture2D sceneTexture : register(t0); // La escena 3D original
Texture2D bloomTexture : register(t1); // El resplandor (blur)
SamplerState textureSampler : register(s0);

//--- PAR�METROS CONTROLABLES DESDE C++ ---
cbuffer BloomParameters : register(b0)
{
    float bloomThreshold;
    float bloomIntensity;
    float sceneIntensity;
    float saturation; // Par�metro para controlar la vibraci�n del color
};

//--- FUNCI�N PRINCIPAL DEL SHADER ---
float4 main(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_TARGET
{
    // 1. Muestrear las texturas de entrada
    float4 baseColor = sceneTexture.Sample(textureSampler, texCoord);
    float4 bloomColor = bloomTexture.Sample(textureSampler, texCoord);
    
    // 2. Composici�n: Sumar la escena y el bloom para obtener el color HDR final
    float3 finalHdrColor = (baseColor.rgb * sceneIntensity) + (bloomColor.rgb * bloomIntensity);
    
    // 3. Tone Mapping: Comprimir el color HDR a un rango visible [0, 1] usando la f�rmula ACES
    //finalHdrColor = finalHdrColor * 0.6f; // Control de Exposici�n
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    float3 tonedColor = (finalHdrColor * (a * finalHdrColor + b)) / (finalHdrColor * (c * finalHdrColor + d) + e);
    finalHdrColor = max(float3(0.0f, 0.0f, 0.0f), tonedColor);
    
    // 4. Saturaci�n: Ajustar la vibraci�n de los colores
    // Convertimos el color a escala de grises para encontrar su intensidad.
    float grayscale = dot(finalHdrColor, float3(0.2126, 0.7152, 0.0722));
    // Interpolamos entre la versi�n en grises y la original para controlar la saturaci�n.
    finalHdrColor = lerp(float3(grayscale, grayscale, grayscale), finalHdrColor, saturation);

    // 5. Correcci�n Gamma: Ajuste final para la visualizaci�n en el monitor (SIEMPRE EL �LTIMO PASO)
    finalHdrColor = pow(finalHdrColor, 1.0f / 2.2f);
    
    // 6. Devolver el color final
    return float4(finalHdrColor, 1.0f);
}