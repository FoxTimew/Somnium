// Passes explicit clip-space positions, colors and UVs to the reference fragment shader.
// Vertex semantics follow SDL's TEXCOORD convention; no backend-specific Y flip.
#include "conventions.hlsli"
struct VertexInput { float3 position : TEXCOORD0; float4 color : TEXCOORD1; float2 uv : TEXCOORD2; };
struct VertexOutput { float4 position : SV_Position; float4 color : TEXCOORD0; float2 uv : TEXCOORD1; };
VertexOutput vsMain(VertexInput input) {
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}
