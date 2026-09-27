// A post-process pass: a horizontal ripple.
//
// It runs over a render-target element's finished subtree, reading it through
// Surface. input.Uv spans 0..1 across the whole element.
//
//   Params[0].x  Amplitude   in pixels
//   Params[0].y  Frequency   waves across the element's height

#include "material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float2 size = ElementSize(input);
    float2 uv = input.Uv;

    float wave = sin(uv.y * Params[0].y + Seconds() * 3.0) * Params[0].x / size.x;

    // Surface already holds premultiplied colour, so the sample is returned
    // as it is rather than through Premultiply.
    return Surface.Sample(SurfaceSampler, uv + float2(wave, 0.0));
}
