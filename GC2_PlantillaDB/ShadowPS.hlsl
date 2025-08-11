// ShadowPS.hlsl (Pixel Shader mnimo - Versin Correcta)

// Para un pase de solo profundidad, no necesitamos escribir un color.
// Sin embargo, el compilador requiere que el shader tenga una firma de
// salida vlida (SV_TARGET).
// El valor que devolvemos aqu ser ignorado por la GPU porque no hay
// un Render Target de color vinculado durante el pase de sombras.

float4 main() : SV_TARGET
{
    // Devolvemos cualquier valor. float4(0,0,0,0) es una opcin segura.
    // Simplemente 'return 0;' tambin funciona.
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}