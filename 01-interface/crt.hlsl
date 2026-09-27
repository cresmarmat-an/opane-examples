// A post-process pass: CRT scanlines, a little colour fringing, and a
// vignette. Chained after ripple.hlsl, it reads ripple's output.
//
//   Params[0].x  Strength   0 leaves the image alone, 1 is the full effect

#include "material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float2 size = ElementSize(input);
    float2 uv = input.Uv;
    float strength = Params[0].x;

    float4 base = Surface.Sample(SurfaceSampler, uv);

    // Red and blue sampled a pixel and a half apart, the way a misconverged
    // tube fringes its edges.
    float2 offset = float2(1.5 / size.x, 0.0) * strength;
    float red = Surface.Sample(SurfaceSampler, uv + offset).r;
    float blue = Surface.Sample(SurfaceSampler, uv - offset).b;
    float4 color = float4(red, base.g, blue, base.a);

    // One dark line every three pixels.
    float scan = 0.80 + 0.20 * sin(uv.y * size.y * 3.14159265 / 1.5);
    color.rgb *= lerp(1.0, scan, strength);

    float2 centred = uv - 0.5;
    float vignette = saturate(1.0 - dot(centred, centred) * 1.4);
    color.rgb *= lerp(1.0, vignette, strength);

    // Darkening premultiplied colour keeps it premultiplied, so this is
    // returned as it is.
    return color;
}
