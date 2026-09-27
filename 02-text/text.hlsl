// A custom shader for text.
//
// Glyphs are stored as signed distance fields rather than coverage, which is
// how one atlas serves every size. This shader uses the distance to draw a
// gradient down each glyph and an outline around it.
//
// The outline width is in screen pixels and is animated by the program, so it
// widens when the pointer is over the heading and narrows when it leaves.

#include "material.hlsli"

float4 FragmentMain(SurfaceInput input) : SV_Target
{
    float3 top = Params[0].rgb;
    float3 bottom = Params[1].rgb;
    float3 outline = Params[2].rgb;
    float width = Params[3].x;

    // Anything in this element that is not a glyph, such as a panel behind the
    // text, is drawn the normal way.
    if (!IsGlyph(input))
    {
        return Premultiply(input.Color.rgb, input.Color.a * ShapeCoverage(input));
    }

    // Down the glyph's own box, so every letter gets the whole gradient rather
    // than a slice of one spread across the line.
    float3 fill = lerp(top, bottom, saturate(input.Uv.y));

    float letter = GlyphCoverage(input);
    float grown = GlyphCoverageAt(input, width);

    // The outline is what the grown letter covers and the letter does not.
    float3 color = lerp(outline, fill, letter);
    float alpha = grown * input.Color.a;

    return Premultiply(color, alpha);
}
