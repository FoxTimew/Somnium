// Exercises fragment sampler, storage and uniform reflection at SDL binding spaces.
// Referenced resources remain active so the metadata test can check their counts.
Texture2D<float4> image : register(t0, space2);
SamplerState imageSampler : register(s0, space2);
StructuredBuffer<float4> values : register(t1, space2);
cbuffer Material : register(b0, space3) { float4 tint; };
float4 psMain(float2 uv : TEXCOORD0) : SV_Target0 {
    return image.Sample(imageSampler, uv) * tint + values[0] * 0.001;
}
