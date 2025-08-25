struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// La correcci�n est� aqu�: cambiamos 'float' a 'uint'
VS_OUTPUT main(uint vertexId : SV_VertexID)
{
    VS_OUTPUT output;
    
    // Este c�digo genera las coordenadas de un tri�ngulo que cubre toda la pantalla
    // y funciona correctamente con un 'uint' para vertexId.
    output.texCoord = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.texCoord * 2.0f - 1.0f, 0.0f, 1.0f);
    output.position.y = -output.position.y; // Invertir Y para que coincida con las coordenadas de textura
    
    return output;
}