// Transforms imported position/normal/UV0 with explicit frame and draw uniforms.
// Column-major matrices match GLM; normals use the inverse transpose of world.
#include "conventions.hlsli"
cbuffer Frame : SOMNIUM_UNIFORM(0) {
    column_major float4x4 view;
    column_major float4x4 projection;
};
cbuffer Object : SOMNIUM_UNIFORM(1) {
    column_major float4x4 world;
    column_major float4x4 normalWorld;
};
struct Input { float3 position : TEXCOORD0; float3 normal : TEXCOORD1; float2 uv : TEXCOORD2; };
struct Output { float4 position : SV_Position; float3 normal : TEXCOORD0; float2 uv : TEXCOORD1; };
Output vsMain(Input input) {
    Output output;
    output.position = mul(projection, mul(view, mul(world, float4(input.position, 1))));
    output.normal = mul(normalWorld, float4(input.normal, 0)).xyz;
    output.uv = input.uv;
    return output;
}
