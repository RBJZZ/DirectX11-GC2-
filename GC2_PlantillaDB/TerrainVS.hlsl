cbuffer CBTerrainVSData : register(b0)
{
    matrix World;
    matrix ViewProjection;
    matrix LightViewProjection;
    float MaxTerrainHeightLocal; // m_heightScale, ya que la altura del vértice es 0-1 * m_heightScale
    // float3 padding; // No es necesario acceder al padding en el shader
};

struct VertexInputType
{
    float3 localPosition : POSITION; // Posición local del vértice del terreno (X=i, Y=altura*escala, Z=j)
    float3 localNormal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0; // Coordenadas de textura para muestreo
    float3 worldNormal : NORMAL; // Normal en espacio del mundo para iluminación
    float3 worldPosition : WORLDPOS; // Posición en espacio del mundo para iluminación
    float scaledLocalY : TEXCOORD1; // Y local escalada (altura real local)
    float maxHeight : TEXCOORD2; // MaxTerrainHeightLocal para el cálculo de mezcla
    float4 positionInLightSpace : TEXCOORD3;
};

PixelInputType main(VertexInputType input)
{
    PixelInputType output;

    float4 worldPos = mul(float4(input.localPosition, 1.0f), transpose(World));
    output.worldPosition = worldPos.xyz;
    output.clipSpacePosition = mul(worldPos, transpose(ViewProjection));

    output.worldNormal = normalize(mul(input.localNormal, (float3x3) transpose(World)));
    output.texCoord = input.texCoord; // Pasa las UVs multiplicadas por m_textureTilingFactor

    output.scaledLocalY = input.localPosition.y; // Esta es la altura ya escalada por m_heightScale
    output.maxHeight = MaxTerrainHeightLocal; // m_heightScale (ya que los datos del heightmap van de 0-1)
    output.positionInLightSpace = mul(worldPos, transpose(LightViewProjection));

    return output;
}