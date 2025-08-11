// ShadowPS_AlphaClip.hlsl

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

struct PixelInputType
{
    float4 clipSpacePosition : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// Este shader no necesita devolver un color, pero s debe existir.
// Su nico propsito es descartar pxeles.
void main(PixelInputType input)
{
    // Leemos el valor alfa de la textura.
    float alpha = diffuseTexture.Sample(textureSampler, input.texCoord).a;
    
    // Si el alfa es menor que un umbral (ej. 0.5), el pxel se descarta.
    // Esto "recorta" la forma de la hoja en la sombra.
    clip(alpha - 0.5f);
}