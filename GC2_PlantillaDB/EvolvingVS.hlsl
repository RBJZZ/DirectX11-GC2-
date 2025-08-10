// EvolvingVS.hlsl - Modificado para iluminaci�n

// El Constant Buffer ahora necesita la matriz World por separado
// para transformar las normales correctamente, y ViewProjection para la posici�n.
cbuffer PerObjectConstants_Evolving : register(b0)
{
    matrix World; // Matriz para transformar a espacio del mundo
    matrix ViewProjection; // Matriz combinada de Vista * Proyecci�n
    // matrix WorldInverseTranspose; // Opcional: Podr�as calcular y pasar esto desde C++
                                    // para transformar normales si hay escalado no uniforme.
                                    // Si no, puedes usar 'World' (con cuidado) o calcularla aqu�.
};

struct VertexInputType_Evolving // Desde ModelVertex
{
    float3 localPosition : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 localNormal : NORMAL;
};

struct PixelInputType_Evolving // Salida hacia el Pixel Shader
{
    float4 clipSpacePosition : SV_POSITION; // Posici�n final en espacio de recorte
    float2 texCoord : TEXCOORD0; // Coordenadas de textura
    float3 worldNormal : NORMAL; // Normal del v�rtice en espacio del mundo
    float3 worldPosition : WORLDPOS; // Posici�n del v�rtice en espacio del mundo
};

PixelInputType_Evolving main(VertexInputType_Evolving input)
{
    PixelInputType_Evolving output;

    // Transformar la posici�n del v�rtice al espacio del mundo
    float4 positionWorld = mul(float4(input.localPosition, 1.0f), transpose(World));
    output.worldPosition = positionWorld.xyz;

    // Transformar la posici�n del v�rtice al espacio de recorte
    output.clipSpacePosition = mul(positionWorld, transpose(ViewProjection));

    // Transformar la normal del v�rtice al espacio del mundo
    // Es crucial usar la transpuesta inversa de la matriz World si hay escalado no uniforme.
    // Si solo tienes rotaci�n y traslaci�n (o escalado uniforme), puedes usar 'World'.
    // Asumiendo que pasamos 'World' transpuesta desde C++ (o la transponemos aqu�):
    // Para una transformaci�n correcta de normales, la matriz debe ser la inversa transpuesta de la World.
    // Si no la pasas expl�citamente, una aproximaci�n com�n (si no hay escalado no uniforme)
    // es usar la parte 3x3 de la matriz World.
    output.worldNormal = normalize(mul(input.localNormal, (float3x3) transpose(World)));
    // Si tienes WorldInverseTranspose en el CB:
    // output.worldNormal = normalize(mul(input.localNormal, (float3x3)transpose(WorldInverseTranspose)));


    // Pasar las coordenadas de textura
    output.texCoord = input.texCoord;

    return output;
}