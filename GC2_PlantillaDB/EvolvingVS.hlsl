// EvolvingVS.hlsl - Modificado para iluminación

// El Constant Buffer ahora necesita la matriz World por separado
// para transformar las normales correctamente, y ViewProjection para la posición.
cbuffer PerObjectConstants_Evolving : register(b0)
{
    matrix World; // Matriz para transformar a espacio del mundo
    matrix ViewProjection; // Matriz combinada de Vista * Proyección
    // matrix WorldInverseTranspose; // Opcional: Podrías calcular y pasar esto desde C++
                                    // para transformar normales si hay escalado no uniforme.
                                    // Si no, puedes usar 'World' (con cuidado) o calcularla aquí.
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
};

PixelInputType_Evolving main(VertexInputType_Evolving input)
{
    PixelInputType_Evolving output;

    // Transformar la posición del vértice al espacio del mundo
    float4 positionWorld = mul(float4(input.localPosition, 1.0f), transpose(World));
    output.worldPosition = positionWorld.xyz;

    // Transformar la posición del vértice al espacio de recorte
    output.clipSpacePosition = mul(positionWorld, transpose(ViewProjection));

    // Transformar la normal del vértice al espacio del mundo
    // Es crucial usar la transpuesta inversa de la matriz World si hay escalado no uniforme.
    // Si solo tienes rotación y traslación (o escalado uniforme), puedes usar 'World'.
    // Asumiendo que pasamos 'World' transpuesta desde C++ (o la transponemos aquí):
    // Para una transformación correcta de normales, la matriz debe ser la inversa transpuesta de la World.
    // Si no la pasas explícitamente, una aproximación común (si no hay escalado no uniforme)
    // es usar la parte 3x3 de la matriz World.
    output.worldNormal = normalize(mul(input.localNormal, (float3x3) transpose(World)));
    // Si tienes WorldInverseTranspose en el CB:
    // output.worldNormal = normalize(mul(input.localNormal, (float3x3)transpose(WorldInverseTranspose)));


    // Pasar las coordenadas de textura
    output.texCoord = input.texCoord;

    return output;
}