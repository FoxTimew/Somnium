// Samples a nearest-filtered texture for UV checks or passes a solid vertex color.
// Negative U selects solid color while retaining the same reflected resource layout.
#include "conventions.hlsli"
Texture2D<float4> colorTexture : SOMNIUM_TEXTURE(0);
SamplerState colorSampler : SOMNIUM_SAMPLER(0);
float4 psMain(float4 color : TEXCOORD0, float2 uv : TEXCOORD1) : SV_Target0 {
    return uv.x < 0.0 ? color : color * colorTexture.Sample(colorSampler, uv);
}
