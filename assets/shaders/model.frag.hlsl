// Samples baseColor in sRGB, then applies factor and Lambert in linear light.
// UNORM offscreen targets encode here; SDR_LINEAR sRGB swapchains encode in hardware.
#include "conventions.hlsli"
#include "color.hlsli"
Texture2D<float4> baseColorTexture : SOMNIUM_TEXTURE(0);
SamplerState baseColorSampler : SOMNIUM_SAMPLER(0);
cbuffer Frame : SOMNIUM_UNIFORM(0) { float4 lightAndEncode; };
cbuffer Material : SOMNIUM_UNIFORM(1) { float4 baseColorFactor; };
float4 psMain(float3 normal : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0 {
    float lambert = max(dot(normalize(normal), lightAndEncode.xyz), 0);
    float3 color = baseColorTexture.Sample(baseColorSampler, uv).rgb * baseColorFactor.rgb * lambert;
    if (lightAndEncode.w != 0) color = encodeSrgb(color);
    return float4(color, 1);
}
