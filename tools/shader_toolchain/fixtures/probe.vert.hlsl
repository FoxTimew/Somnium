// Exercises an entry point, an include and a vertex uniform through all formats.
// This is a compiler fixture, not a renderer or a GPU runtime test.
#include "probe_common.hlsli"
cbuffer Camera : register(b0, space1) { float4x4 transform; };
struct Output { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Output vsMain(uint index : SV_VertexID) {
    const float2 positions[3] = {float2(-0.75, -0.75), float2(0.75, -0.75), float2(0.0, 0.75)};
    Output output;
    output.position = mul(transform, float4(positions[index], kDepth, 1.0));
    output.uv = positions[index] * 0.5 + 0.5;
    return output;
}
