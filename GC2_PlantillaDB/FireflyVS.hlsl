// FireflyVS.hlsl - Vertex Shader para partculas de lucirnaga con billboarding

// Datos que cambian por cada frame (globales)
cbuffer PerFrameConstants : register(b0)
{
    matrix ViewProjection;
    float3 CameraUp_World; // El vector "arriba" de la cmara en el mundo
    float3 CameraRight_World; // El vector "derecha" de la cmara en el mundo
};

// Datos que cambian por cada partcula
cbuffer PerParticleConstants : register(b1)
{
    float3 ParticleCenter_World; // Posicin de la lucirnaga en el mundo
    float2 ParticleSize; // Ancho y alto de la lucirnaga
    float2 _padding;
    float4 ParticleColor;
};

struct VertexInputType
{
    float3 localPosition : POSITION; // Coordenadas del vrtice del quad (-0.5 a 0.5)
    float2 texCoord : TEXCOORD0;
};

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    // 1. Calcula el desplazamiento del vrtice desde el centro usando los ejes de la cmara
    // Esto asegura que el quad siempre est orientado hacia la cmara.
    float3 worldOffset = (input.localPosition.x * CameraRight_World * ParticleSize.x) +
                         (input.localPosition.y * CameraUp_World * ParticleSize.y);
    
    // 2. Calcula la posicin final del vrtice en el mundo
    float3 finalWorldPos = ParticleCenter_World + worldOffset;
    
    // 3. Transforma a espacio de recorte
    output.clipSpacePosition = mul(float4(finalWorldPos, 1.0f), transpose(ViewProjection));
    
    // 4. Pasa los datos al Pixel Shader
    output.texCoord = input.texCoord;
    output.color = ParticleColor;
    
    return output;
}