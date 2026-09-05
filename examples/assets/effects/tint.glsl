//! effect Tint
//! input LastRTT
//! param vec4 uTint 1,0.85,0.7,1 Tint colour
//! param float uAmount 1.0 0.0 1.0 Amount
//
// A worked example of the post-effect asset format - see CustomEffect.h for
// the full set of directives. Everything that is not a `//!` line is the
// fragment shader body; the preamble (precision, FragColor, vTexcoord, one
// uTexN sampler per declared input, and the parameter block) is generated, so
// this file is only the part that is actually about the effect.

void main()
{
    vec4 c = texture_2D(uTex0, vTexcoord);
    FragColor = vec4(mix(c.rgb, c.rgb * uTint.rgb, uAmount), c.a);
}
