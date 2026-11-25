// Exercises a pushed vertex uniform with the D5 vertex layout.
// A zero offset preserves the analytic fullscreen quad.
#include "conventions.hlsli"
cbuffer Frame : SOMNIUM_UNIFORM(0) { float4 offset; };
struct Output { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Output vsMain(float3 position : TEXCOORD0, float4 color : TEXCOORD1, float2 uv : TEXCOORD2) {
    Output output;
    output.position = float4(position + offset.xyz, 1.0);
    output.uv = uv;
    return output;
}
