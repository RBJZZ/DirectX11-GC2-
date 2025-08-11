// EvolvingVS.hlsl - Modificado para iluminación

// El Constant Buffer ahora necesita la matriz World por separado
// para transformar las normales correctamente, y ViewProjection para la posición.
cbuffer PerObjectConstants_Evolving : register(b0)
{
    matrix World; // Matriz para transformar a espacio del mundo
    matrix ViewProjection; // Matriz combinada de Vista * Proyección
    matrix LightViewProjection;
    matrix WorldInverseTranspose;
};

struct VertexInputType_Evolving // Desde ModelVertex
{
    float3 localPosition : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 localNormal : NORMAL;
};

struct PixelInputType_Evolving // Salida hacia el Pixel Shader
{
    float4 clipSpacePosition : SV_POSITION; // Posición final en espacio de recorte
    float2 texCoord : TEXCOORD0; // Coordenadas de textura
    float3 worldNormal : NORMAL; // Normal del vértice en espacio del mundo
    float3 worldPosition : WORLDPOS; // Posición del vértice en espacio del mundo
    float4 positionInLightSpace : TEXCOORD1;
};

PixelInputType_Evolving main(VertexInputType_Evolving input)
{
    PixelInputType_Evolving output;

    // Transformar la posición del vértice al espacio del mundo
    float4 positionWorld = mul(float4(input.localPosition, 1.0f), transpose(World));
    output.worldPosition = positionWorld.xyz;

    // Transformar la posición del vértice al espacio de recorte
    output.clipSpacePosition = mul(positionWorld, transpose(ViewProjection));

    output.worldNormal = normalize(mul(input.localNormal, (float3x3) transpose(WorldInverseTranspose)));

    output.texCoord = input.texCoord;
    output.positionInLightSpace = mul(positionWorld, transpose(LightViewProjection));
    
    return output;
}