//============================================================================
// Name        : GradeEffect.cpp
// Author      : Duarte Peixinho
// Description : The source of the built-in post effect "Grade" (PostEffectChain):
//               a colourist's grade at the end of the chain - white balance, lift /
//               gamma / gain, a film S-curve, split toning, saturation and vibrance,
//               a vignette and grain. Written as an asset effect (CustomEffect header),
//               so its parameters are the editor's sliders like any other's.
//============================================================================

namespace p3d {

	const char* GradeEffectSource()
	{
		return
R"PYROSGRADE(//! effect Grade
//! input LastRTT
//! param vec4 uLens 0.52,1.7778,0,0 tan(half fov), aspect (filled by the engine)
//! param float uTime 0.0 Seconds (filled by the engine)
//! param float uExposure 0.0 -3.0 3.0 Exposure (stops)
//! param float uShoulder 0.0 0.0 0.95 Highlight shoulder (0 off; else where the roll-off to white starts)
//! param float uTemperature 0.0 -1.0 1.0 Temperature (cool - warm)
//! param float uTint 0.0 -1.0 1.0 Tint (green - magenta)
//! param vec4 uLift 0,0,0,0 Lift (rgb, and w for all)
//! param vec4 uGamma 1,1,1,1 Gamma (rgb, and w for all)
//! param vec4 uGain 1,1,1,1 Gain (rgb, and w for all)
//! param float uContrast 1.0 0.5 2.0 Contrast (a film S-curve)
//! param float uPivot 0.43 0.1 0.9 Contrast pivot
//! param vec4 uShadowTint 0,0,0,0 Shadow tint (rgb, w strength)
//! param vec4 uHighlightTint 0,0,0,0 Highlight tint (rgb, w strength)
//! param float uSaturation 1.0 0.0 2.0 Saturation
//! param float uVibrance 0.0 -1.0 1.0 Vibrance (muted colours more than vivid ones)
//! param float uVignette 0.0 0.0 1.0 Vignette
//! param float uGrain 0.0 0.0 0.2 Film grain

// The look, last: what a colourist does to a finished picture. It takes the frame as the effects
// before it left it - linear light, 0 to 1 once the Sky effect's shoulder has had it - grades in
// a perceptual space (gamma 2.2, where "half" looks like half), and gives it back linear.
//
//   shoulder       exposure raised on a picture that already reaches white would clip a sunlit
//                  street to a flat white: above this the rest of the way is taken slowly
//   white balance  a warm/cool and a green/magenta shift, before anything else
//   lift/gamma/gain  the three wheels: the blacks, the middle, the whites (rgb each, w all)
//   contrast       an S about the pivot that never clips: a toe for the shadows, a shoulder
//                  for the highlights - film's response, not a straight line through grey
//   split toning   a colour into the shadows, another into the highlights (cool and warm)
//   saturation     all colour; vibrance - the muted more than the already vivid
//   vignette       the lens darkening its corners; grain - moving, finer in the lights

float lumaOf(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
float hashG(vec2 p) { vec3 q = fract(vec3(p.xyx) * 0.1031); q += dot(q, q.yzx + 33.33); return fract((q.x + q.y) * q.z); }

void main() {
	vec3 c = max(texture_2D(uTex0, vTexcoord).rgb, vec3(0.0)) * exp2(uExposure);
	if (uShoulder > 0.0)
	{
		vec3 over = max(c - uShoulder, 0.0);
		c = min(c, vec3(uShoulder)) + (1.0 - uShoulder) * (1.0 - exp(-over / (1.0 - uShoulder)));
	}
	// white balance (a von Kries-ish scale: warm lifts red and lowers blue, tint trades green)
	c *= vec3(1.0 + 0.12 * uTemperature - 0.04 * uTint, 1.0 + 0.08 * uTint, 1.0 - 0.12 * uTemperature - 0.04 * uTint);

	vec3 g = pow(c, vec3(1.0 / 2.2));
	// lift, gamma, gain
	vec3 lift = uLift.rgb + uLift.w, gain = uGain.rgb * uGain.w, gam = max(uGamma.rgb * uGamma.w, vec3(0.05));
	g = gain * (g + lift * (1.0 - g));
	g = pow(max(g, vec3(0.0)), 1.0 / gam);
	// the S: each side of the pivot bent by the contrast, so it never leaves 0..1
	vec3 x = clamp(g, 0.0, 1.0);
	vec3 lo = uPivot * pow(x / uPivot, vec3(uContrast));
	vec3 hi = 1.0 - (1.0 - uPivot) * pow((1.0 - x) / (1.0 - uPivot), vec3(uContrast));
	g = mix(lo, hi, step(uPivot, x)) + max(g - 1.0, vec3(0.0));
	// split toning, by where the pixel sits between black and white
	float l = lumaOf(g);
	float ws = (1.0 - smoothstep(0.0, 0.55, l)), wh = smoothstep(0.45, 1.0, l);
	g += (uShadowTint.rgb - 0.5 * lumaOf(uShadowTint.rgb)) * uShadowTint.w * ws;
	g += (uHighlightTint.rgb - 0.5 * lumaOf(uHighlightTint.rgb)) * uHighlightTint.w * wh;
	// saturation and vibrance
	l = lumaOf(g);
	float mx = max(g.r, max(g.g, g.b)), mn = min(g.r, min(g.g, g.b));
	float sat = (mx - mn) / max(mx, 1e-4);
	g = mix(vec3(l), g, uSaturation * (1.0 + uVibrance * (1.0 - sat)));
	// vignette (round on the screen, whatever its shape)
	vec2 v = (vTexcoord - 0.5) * vec2(uLens.y, 1.0) / max(uLens.y, 1.0) * 2.0;
	g *= 1.0 - uVignette * smoothstep(0.45, 1.35, length(v));
	// grain: new every frame, a touch more in the shadows
	float n = hashG(gl_FragCoord.xy + fract(uTime * 7.31) * 1931.0) - 0.5;
	g += n * uGrain * (1.0 - 0.6 * l);
	FragColor = vec4(pow(max(g, vec3(0.0)), vec3(2.2)), 1.0);
}
)PYROSGRADE";
	}

}
