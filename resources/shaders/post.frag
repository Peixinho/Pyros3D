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

// How blurred a point at this depth is, in pixels: nothing within the sharp band round the focus, more the further out
float Blur(float depth)
{
	float d = depth <= 0.0 ? 1e5 : DepthDistance(depth);
	float off = max(abs(d - uDraw.dof.x) - uDraw.dof.y * 0.5, 0.0);
	return uDraw.dof.z * clamp(off / max(uDraw.dof.x, 1.0), 0.0, 1.0);
}

void main()
{
	vec3 c = texture(sampler2D(uScene, uLinearClamp), vUV).rgb;
	vec2 px = uFrame.viewport.zw;

	// Depth of field: a disc gathered round the pixel, as wide as its own blur; a sample counts only as far as its own blur
	// reaches back to here - a sharp thing does not smear, a near blurred one spreads over what is behind it
	if (uDraw.dof.z > 0.0)
	{
		float d0 = DepthAt(vUV), r0 = Blur(d0);
		vec3 sum = c; float weight = 1.0;
		const float golden = 2.39996323;
		// (the disc turned and its rings shifted by a noise that changes each pixel and each frame: no ring of samples
		// repeats a thin edge as rings - the noise the temporal resolve after this averages away)
		float spin = InterleavedNoise(gl_FragCoord.xy + fract(uFrame.sky.z * 7.31) * 113.0) * 6.2831853;
		float shift = InterleavedNoise(gl_FragCoord.yx + fract(uFrame.sky.z * 3.17) * 71.0);
		for (int i = 1; i < 32; i++)
		{
			float t = sqrt((float(i) - shift) / 31.0);
			float a = float(i) * golden + spin;
			vec2 o = vec2(cos(a), sin(a)) * t * uDraw.dof.z;
			vec2 uv = vUV + o * px;
			float ds = DepthAt(uv), rs = Blur(ds);
			float dist = length(o);
			// (a sample counts where its own blur reaches here; one behind this point - reversed Z: a smaller depth - no
			// further than this point's own blur, so a blurred background does not haze over a sharp thing in front)
			float reach = ds < d0 ? min(rs, r0) : rs;
			float w = smoothstep(dist - 1.0, dist, reach);
			sum += texture(sampler2D(uScene, uLinearClamp), uv).rgb * w; weight += w;
		}
		c = sum / weight;
	}

	// Motion blur: along the pixel's move on screen this frame, as long as the shutter was open
	if (uDraw.motion.x > 0.0)
	{
		vec2 v = texture(sampler2D(uVelocity, uLinearClamp), vUV).xy * uDraw.motion.x;
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
