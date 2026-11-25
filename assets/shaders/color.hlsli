// Converts linear-light RGB to the IEC sRGB transfer function exactly once at output.
// Offscreen UNORM targets use this; SDR_LINEAR swapchains perform it in hardware.
#ifndef SOMNIUM_COLOR
#define SOMNIUM_COLOR
float3 encodeSrgb(float3 linearLight) {
    float3 value = saturate(linearLight);
    return float3(value.r <= 0.0031308 ? 12.92 * value.r : 1.055 * pow(value.r, 1.0 / 2.4) - 0.055,
                  value.g <= 0.0031308 ? 12.92 * value.g : 1.055 * pow(value.g, 1.0 / 2.4) - 0.055,
                  value.b <= 0.0031308 ? 12.92 * value.b : 1.055 * pow(value.b, 1.0 / 2.4) - 0.055);
}
#endif
