// Two sampled sRGB inputs, two pushed uniform slots, one UNORM output encoding.
// The CPU proof derives its expectation independently from the transfer functions.
#include "conventions.hlsli"
#include "color.hlsli"
Texture2D<float4> firstTexture : SOMNIUM_TEXTURE(0);
Texture2D<float4> secondTexture : SOMNIUM_TEXTURE(1);
SamplerState firstSampler : SOMNIUM_SAMPLER(0);
SamplerState secondSampler : SOMNIUM_SAMPLER(1);
cbuffer Frame : SOMNIUM_UNIFORM(0) { float4 weights; };
cbuffer Draw : SOMNIUM_UNIFORM(1) { float4 factor; };
float4 psMain(float2 uv : TEXCOORD0) : SV_Target0 {
    float3 linearLight = weights.x * firstTexture.Sample(firstSampler, uv).rgb
                  + weights.y * secondTexture.Sample(secondSampler, uv).rgb;
    return float4(encodeSrgb(linearLight * factor.rgb), 1.0);
}
