// Keeps the production interface while changing the closed-form white probe to red.
// Both lighting variants participate in the same program transaction.
#define psMain originalMain
#if defined(RELOAD_AMBIENT)
#include "model_ambient.frag.hlsl"
#else
#include "model.frag.hlsl"
#endif
#undef psMain
float4 psMain(float3 normal : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0 {
    return originalMain(normal, uv) * float4(1, 0, 0, 1);
}
