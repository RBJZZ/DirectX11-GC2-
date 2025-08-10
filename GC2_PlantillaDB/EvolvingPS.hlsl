// EvolvingPS.hlsl - Modificado para iluminación ambiental, difusa y especular

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

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
};

struct PixelInputType_Evolving // Entrada desde el Vertex Shader
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 worldNormal : NORMAL; // Normal en espacio del mundo (interpolada)
    float3 worldPosition : WORLDPOS; // Posición del píxel en espacio del mundo (interpolada)
};

float4 main(PixelInputType_Evolving input) : SV_TARGET
{
    // Obtener el color base de la textura
    float4 albedo = diffuseTexture.Sample(textureSampler, input.texCoord);

    float alphaClipThreshold = 0.5f;
    clip(albedo.a - alphaClipThreshold);
    // clip(albedo.a - 0.1f); // Si alfa < 0.1, el píxel se descarta

    // Normalizar la normal del píxel (fue interpolada, así que necesita renormalizarse)
    float3 N = normalize(input.worldNormal);

    // Vector de luz (asumimos que directionalLightVector es la dirección HACIA la fuente de luz)
    // Si fuera la dirección DESDE la fuente de luz, lo negarías.
    float3 L = normalize(directionalLightVector);

    // --- 1. Cálculo de Luz Ambiental ---
    // La luz ambiental ilumina todo por igual.
    // Multiplica el color de la luz ambiental por el color difuso del material y el albedo de la textura.
    float4 ambient = ambientLightColor * materialDiffuseColor * albedo;

    // --- 2. Cálculo de Luz Difusa (Modelo de Lambert) ---
    // Depende del ángulo entre la normal de la superficie y la dirección de la luz.
    // El producto punto (dot product) da el coseno de ese ángulo.
    // saturate() lo mantiene en el rango [0, 1].
    float NdotL = saturate(dot(N, L));
    float4 diffuse = NdotL * directionalLightColor * materialDiffuseColor * albedo;

    // --- 3. Cálculo de Luz Especular (Modelo de Phong/Blinn-Phong simplificado) ---
    float4 specular = float4(0.0f, 0.0f, 0.0f, 0.0f); // Inicia en negro

    if (NdotL > 0.0f) // Solo calcular especular si la luz incide en la superficie
    {
        // Vector de Vista: Dirección desde el punto en la superficie (input.worldPosition) hacia la cámara.
        float3 V = normalize(cameraPositionWorld - input.worldPosition);

        // Vector de Reflexión: Refleja el vector de luz incidente (-L porque L va hacia la luz) sobre la normal N.
        float3 R = reflect(-L, N); // R = 2 * dot(N,L) * N - L

        // Factor especular: Coseno del ángulo entre el vector de reflexión y el vector de vista, elevado a la potencia especular.
        float RdotV = saturate(dot(R, V));
        
        // El color especular del material controla el color del brillo.
        // La luz direccional también contribuye al color del brillo especular.
        specular = pow(RdotV, specularPower) * directionalLightColor * materialSpecularColor;
    }

    // --- Combinación Final ---
    // Suma los componentes ambiental, difuso y especular.
    // El alfa final puede ser el de la textura o 1.0 si es opaco.
    if (albedo.a < 0.1f) // Si el alfa de la textura es muy bajo (casi transparente)
    {
    // Opción A: Forzar transparencia total y color negro (el color no debería importar)
    // Si esto funciona, el problema era que el albedo.a original no era realmente 0
    // o el RGB original en estas áreas transparentes estaba causando artefactos.
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Opción B: Pintar estas áreas de un color opaco brillante para verlas claramente.
    // Si estas áreas magentas SÍ se ven contra el cielo, pero antes no,
    // indica que el problema está en cómo se interpretaba el alfa bajo.
    // return float4(1.0f, 0.0f, 1.0f, 1.0f); // Magenta opaco
    }
    
    float4 finalColor = ambient + diffuse + specular;
    finalColor.a = albedo.a; // O finalColor.a = 1.0f;

    return finalColor;
}