// LightingVS.hlsl

cbuffer PerObjectConstants : register(b0) // Renombrado para claridad, antes era b1
{
    matrix World;
    matrix ViewProjection; // Combinamos View * Projection en C++
    matrix WorldInverseTranspose; // Para transformar normales correctamente
};

struct VertexInputType
{
    float3 localPosition : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 localNormal : NORMAL;
    // Podríamos añadir tangente y binormal aquí después para normal mapping
};

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION; // Posición en espacio de recorte (requerida)
    float2 texCoord : TEXCOORD0; // Coordenadas de textura
    float3 worldNormal : NORMAL; // Normal en espacio del mundo
    float3 worldPosition : POSITION0; // Posición en espacio del mundo
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    // Calcular posición en espacio del mundo
    output.worldPosition = mul(float4(input.localPosition, 1.0f), transpose(World)).xyz;

    // Calcular posición en espacio de recorte (para el rasterizador)
    // WorldViewProjection = World * View * Projection (precalculada en C++)
    // No, WorldViewProjection = View * Projection. Multiplicamos por World aquí.
    // De hecho, es mejor: WorldViewProjection = World * View * Projection.
    // Pero para pasar World y WorldInverseTranspose, es más común pasar W, V, P o W, VP.
    // Vamos a pasar ViewProjection y multiplicar por World.
    // matrix WorldViewProjectionMatrix = mul(World, ViewProjection); // Asumimos ViewProjection es View * Proj
    // output.clipSpacePosition = mul(float4(input.localPosition, 1.0f), transpose(WorldViewProjectionMatrix));

    // Alternativa más común: WVP precalculada en C++
    // output.clipSpacePosition = mul(float4(input.localPosition, 1.0f), transpose(WorldViewProjection));
    // Donde WorldViewProjection = World * View * Projection.
    // Para pasar World por separado para cálculos de iluminación:
    matrix WVP = mul(World, ViewProjection);
    output.clipSpacePosition = mul(float4(input.localPosition, 1.0f), transpose(WVP));


    // Transformar la normal del vértice al espacio del mundo
    // Usamos la transpuesta de la inversa de la matriz de mundo para las normales
    // si hay escalado no uniforme. Si no, solo la parte 3x3 de World es suficiente.
    output.worldNormal = normalize(mul(input.localNormal, (float3x3) transpose(WorldInverseTranspose)));
    // output.worldNormal = normalize(mul(input.localNormal, (float3x3)World)); // Más simple, pero incorrecto con escalado no uniforme

    // Pasar las coordenadas de textura
    output.texCoord = input.texCoord;

    return output;
}