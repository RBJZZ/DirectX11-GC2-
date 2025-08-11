// ShadowVS.hlsl (NUEVA VERSIÓN)

cbuffer PerObjectConstants : register(b0)
{
    matrix World;
    matrix LightViewProjection;
};

struct VertexInputType
{
    float3 localPosition : POSITION;
    // El resto de los campos (normal, texcoord) no son necesarios aquí,
    // pero el InputLayout de C++ debe corresponder al buffer que le pasamos.
};

float4 main(VertexInputType input) : SV_POSITION
{
    // Transformar a espacio del mundo primero
    float4 worldPos = mul(float4(input.localPosition, 1.0f), transpose(World));
    
    // Luego transformar a espacio de recorte de la luz
    return mul(worldPos, transpose(LightViewProjection));
}