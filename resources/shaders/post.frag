#version 450
#include "common.glsl"
// The picture's lens and shutter, on the HDR picture before the temporal resolve - which smooths their noise (each off
// unless asked for):
// light shafts from the sun through what stands against the sky, depth of field - a lens focused at one distance - and
// motion blur, along each pixel's own move on screen (the depth prepass's).
layout(set = 0, binding = 0, std140) uniform DrawData
{
	vec4 shafts;     // x how strong (0 off), y how fast they fade along their length, z how far toward the sun they reach (0..1)
	vec4 sun;        // xy the sun on the screen (0..1, y down), z 1 when it is in front of the eye, w its height over the horizon
	vec4 dof;        // x the distance in focus (m), y how deep the sharp band is (m), z the largest blur (pixels; 0 off)
	vec4 motion;     // x how strong (the shutter: 1 a whole frame's move; 0 off), y the most a pixel is smeared (pixels)
} uDraw;
layout(set = 1, binding = 1) uniform texture2D uScene;
layout(set = 1, binding = 2) uniform sampler uLinearClamp;
layout(set = 1, binding = 3) uniform texture2D uVelocity;
layout(set = 1, binding = 4) uniform texture2D uDepth;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

float DepthAt(vec2 uv)
{
	ivec2 size = textureSize(sampler2D(uDepth, uLinearClamp), 0);
	return texelFetch(sampler2D(uDepth, uLinearClamp), clamp(ivec2(uv * vec2(size)), ivec2(0), size - 1), 0).r;
}

float InterleavedNoise(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

// How blurred a point at this depth is, in pixels - a thin lens: the blur circle grows as |1 - focus/distance|, so far
// off it settles to the lens's largest (a background behind a subject is all equally soft, as in a photograph) and near
// the lens it grows fast; nothing within the sharp band round the focus
float Blur(float depth)
{
	float d = depth <= 0.0 ? 1e5 : DepthDistance(depth);
	float f = max(uDraw.dof.x, 0.1);
	float band = uDraw.dof.y * 0.5;
	float near = max(f - band, 0.05), far = f + band;
	float k = d < near ? (near / d - 1.0) : (d > far ? (1.0 - far / d) : 0.0);
	return uDraw.dof.z * clamp(k * 1.6, 0.0, 1.0);
}

void main()
{
	vec3 c = texture(sampler2D(uScene, uLinearClamp), vUV).rgb;
	vec2 px = uFrame.viewport.zw;

	// Depth of field, gathered: a disc of samples round the pixel as wide as the largest blur near it, each counted only
	// as far as its own blur reaches back to here (scatter as gather) - a sharp thing never smears, a blurred one in
	// front spreads over what is behind it, and a sharp one in front is not hazed by the soft background round it.
	// Up to 96 samples in rings, so a wide blur is a smooth one and not copies of the picture laid over each other (the
	// first, 31 samples over 24 px, broke up into stripes); bright points weighted up into soft discs - bokeh.
	if (uDraw.dof.z > 0.0)
	{
		float d0 = DepthAt(vUV), r0 = Blur(d0);
		// (how wide to look: this pixel's own blur, or the largest a nearer blurred thing could reach here with)
		float R = max(r0, uDraw.dof.z * 0.5);
		vec3 sum = vec3(0.0); float weight = 0.0;
		const float golden = 2.39996323;
		float spin = InterleavedNoise(gl_FragCoord.xy + fract(uFrame.sky.z * 7.31) * 113.0) * 6.2831853;
		float shift = InterleavedNoise(gl_FragCoord.yx + fract(uFrame.sky.z * 3.17) * 71.0);
		int N = int(clamp(R * R * 0.30, 12.0, 128.0));
		for (int i = 0; i < 128; i++)
		{
			if (i >= N) break;
			float t = sqrt((float(i) + shift) / float(N));
			float a = float(i) * golden + spin;
			vec2 o = vec2(cos(a), sin(a)) * t * R;
			vec2 uv = vUV + o * px;
			float ds = DepthAt(uv), rs = Blur(ds);
			float dist = length(o);
			float reach = ds < d0 ? min(rs, r0) : rs;
			float w = clamp(reach - dist + 1.0, 0.0, 1.0);
			// (a blurred sample's light spread over its whole circle: weighted by its area's inverse, so a big soft thing
			// does not outweigh what is in focus; never past a sharp one's)
			w /= max(1.0, rs * rs * 0.04);
			vec3 cs = texture(sampler2D(uScene, uLinearClamp), uv + px * 0.5 * (rs > 3.0 ? 1.0 : 0.0)).rgb;
			float lum = dot(cs, vec3(0.2126, 0.7152, 0.0722));
			w *= 1.0 + 2.0 * clamp(lum - 1.5, 0.0, 4.0) * clamp(rs / 4.0, 0.0, 1.0);
			sum += cs * w; weight += w;
		}
		// (the pixel itself, always: a sharp one is itself)
		float self = 1.0 / max(1.0, r0 * r0 * 0.04);
		c = (sum + c * self) / (weight + self);
	}

	// Motion blur: along the pixel's move on screen this frame, as long as the shutter was open
	if (uDraw.motion.x > 0.0)
	{
		vec2 v = texture(sampler2D(uVelocity, uLinearClamp), vUV).xy;
		// (nothing drawn there - the sky: the velocity's "no surface" mark, 10000 px. Taken as a move, every sky pixel was
		// smeared 24 px along one diagonal and the edges against it bled into it)
		if (abs(v.x) > 1000.0) v = vec2(0.0);
		v *= uDraw.motion.x;
		float len = length(v / px);
		if (len > 0.5)
		{
			v *= min(1.0, uDraw.motion.y / len);
			vec3 sum = c; float n = 1.0;
			for (int i = 0; i < 8; i++)
			{
				float t = (float(i) + 0.5) / 8.0 - 0.5;
				sum += texture(sampler2D(uScene, uLinearClamp), vUV - v * t).rgb; n += 1.0;
			}
			c = sum / n;
		}
	}

	// Light shafts: the sky's light toward the sun, gathered along the way from here - what stands against the sky
	// (anything with a depth) gives none, so its shadow streams out from it
	if (uDraw.shafts.x > 0.0 && uDraw.sun.z > 0.5)
	{
		vec2 toSun = (uDraw.sun.xy - vUV) * uDraw.shafts.z;
		vec2 stride = toSun / 32.0;
		vec2 uv = vUV + stride * InterleavedNoise(gl_FragCoord.xy);
		float decay = 1.0, light = 0.0;
		vec3 tint = vec3(0.0);
		for (int i = 0; i < 32; i++)
		{
			uv += stride;
			if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;
			float sky = DepthAt(uv) <= 0.0 ? 1.0 : 0.0;
			vec3 s = texture(sampler2D(uScene, uLinearClamp), uv).rgb;
			tint += s * sky * decay;
			light += sky * decay;
			decay *= uDraw.shafts.y;
		}
		// (strongest looking toward the sun, nothing looking away: their glare)
		float facing = clamp(1.0 - length(uDraw.sun.xy - vUV) * 1.2, 0.0, 1.0);
		c += tint / 32.0 * uDraw.shafts.x * facing * facing * clamp(uDraw.sun.w * 4.0, 0.0, 1.0);
	}
	outColor = vec4(c, 1.0);
}
