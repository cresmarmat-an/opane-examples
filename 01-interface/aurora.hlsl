// A custom opane material.
//
// It replaces the fragment shader of the element it is attached to. The
// element still lays out, batches, clips, and receives input as before; only
// its pixels come from here.
//
// Params, in the order they are declared when the material is created:
//   Params[0]    ColorA
//   Params[1]    ColorB
//   Params[2].x  Speed
//
// Save this file while the program is running and it is recompiled.

#include "material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float2 uv = LocalUv(input);
    float time = Seconds() * Params[2].x;

    // Two travelling waves, folded together into a vertical gradient blend.
    float waveA = sin(uv.x * 6.0 + time) * 0.5 + 0.5;
    float waveB = sin(uv.y * 4.0 - time * 0.7) * 0.5 + 0.5;
    float blend = saturate(uv.y * 0.55 + waveA * 0.25 + waveB * 0.20);

    float3 color = lerp(Params[0].rgb, Params[1].rgb, blend);

    // A sheen band sweeping across. AntiAlias rather than step, so the band's
    // edges are as smooth as the built-in primitives.
    float band = frac(time * 0.12);
    float distance = abs(uv.x - band) - 0.05;
    color += 0.30 * AntiAlias(distance * 6.0);

    // ShapeCoverage keeps the element's own rounded corners, so a material does
    // not have to know what shape it is painting.
    float alpha = ShapeCoverage(input) * input.Color.a;

    return Premultiply(color, alpha);
}
