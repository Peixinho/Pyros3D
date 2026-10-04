//============================================================================
// Name        : VolumetricSmokeEffect.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : See VolumetricSmokeEffect.h.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/Effects/VolumetricSmokeEffect.h>
#include <Pyros3D/Rendering/PostEffects/VolumetricSmoke.h>
#include <chrono>
#include <cmath>
#include <cstring>

namespace p3d {

	namespace {

		// std140 offsets of VolSmokeParams, shared by the shader below and
		// PreDraw(). The three the manager fills by name come first.
		enum {
			OffProj = 0,
			OffInverseView = 64,
			OffNearFar = 128,
			OffP0 = 144,        // time, cell size, density, step
			OffP1 = 160,        // noise scale, erosion, shadow, max steps
			OffSunDir = 176,
			OffSunColor = 192,
			OffAmbient = 208,
			OffWind = 224,
			OffBox = 240,       // vec4[64]: grid low corner, growth
			OffColor = 1264,    // vec4[64]: colour, density
			OffBoundsMin = 2288,   // vec4[64]: box around the smoke itself
			OffBoundsMax = 3312,
			OffLightPos = 4336,    // vec4[8]: position, radius
			OffLightColor = 4464,  // vec4[8]: colour, cos of the outer cone (-1: point)
			OffLightDir = 4592,    // vec4[8]: spot direction, cos of the inner cone
			BlockSize = 4720,
			MaxLocalLights = 8
		};

		const char* kPrelude =
			"#define varying_in in\n"
			"#define varying_out out\n"
			"#define attribute_in in\n"
			"#define texture_2D texture\n"
			"#define texture_cube texture\n"
#if defined(GLES3)
			"precision highp float;\n"
#endif
			"#if defined(VULKAN)\n"
			"#define UBO_BINDING(n) layout(std140, binding = n)\n"
			"#define SAMPLER_BINDING(n) layout(set = 1, binding = n)\n"
			"#define IO_LOCATION(n) layout(location = n)\n"
			"#else\n"
			"#define UBO_BINDING(n) layout(std140)\n"
			"#define SAMPLER_BINDING(n)\n"
			"#define IO_LOCATION(n)\n"
			"#endif\n"
			"IO_LOCATION(0) out vec4 FragColor;\n"
			"IO_LOCATION(0) varying_in vec2 vTexcoord;\n";

		const char* kMarch = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;   // scene depth
SAMPLER_BINDING(1) uniform sampler2D uTex1;   // cloud atlas
UBO_BINDING(44) uniform VolSmokeParams {
	mat4 matProj;
	mat4 uInverseView;
	vec4 uNearFar;
	vec4 uP0;        // time, cell size, density, step
	vec4 uP1;        // noise scale, erosion, shadow, max steps
	vec4 uSunDir;
	vec4 uSunColor;
	vec4 uAmbient;
	vec4 uWind;
	vec4 uBox[64];   // grid low corner, growth
	vec4 uCol[64];   // colour, density
	vec4 uBMin[64];  // box around the smoke itself; w: emission (fire)
	vec4 uBMax[64];  // w: for fire, 0 flames .. 1 blast
	vec4 uLPos[8];   // position, radius
	vec4 uLCol[8];   // colour, cos of the outer cone (-1: point light)
	vec4 uLDir[8];   // spot direction, cos of the inner cone
};

#define CLOUDS 64
// As many as there are clouds, on purpose. This was 8, then 12: a ray that
// crossed more boxes than that simply never heard about the rest, and in a
// pile of overlapping clouds whole chunks of the nearer ones went missing -
// whichever happened to have the higher slot numbers.
#define MAX_HITS 64
// Must match kWallFloor in VolumetricSmoke.cpp.
#define WALL_FLOOR 0.2

const vec3 GRID = vec3(32.0, 16.0, 32.0);
const vec2 ATLAS = vec2(1024.0, 1024.0);

float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

float vnoise(vec3 x)
{
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash13(i), hash13(i + vec3(1, 0, 0)), f.x),
	               mix(hash13(i + vec3(0, 1, 0)), hash13(i + vec3(1, 1, 0)), f.x), f.y),
	           mix(mix(hash13(i + vec3(0, 0, 1)), hash13(i + vec3(1, 0, 1)), f.x),
	               mix(hash13(i + vec3(0, 1, 1)), hash13(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p)
{
	// Big rolls with a finer boil on top; the gap between the two octaves
	// is what keeps it from reading as one uniform fuzz.
	return 0.56 * vnoise(p) + 0.28 * vnoise(p * 2.7 + 7.1) + 0.16 * vnoise(p * 6.1 + 3.7);
}

// How much smoke cloud `i` has at world point p, 0..1, before the billows
// are cut into it. Two taps: the atlas holds the grid as flat slices, so the
// blend between one slice and the next is done here.
// x: the field. y: how much of the sun gets to this point past the world.
vec2 cloudSample(int i, vec3 p)
{
	vec3 v = (p - uBox[i].xyz) / uP0.y;
	if (any(lessThan(v, vec3(0.5))) || any(greaterThan(v, GRID - 0.5))) return vec2(0.0, 1.0);
	float fy = v.y - 0.5;
	float y0 = floor(fy);
	float ty = fy - y0;
	float y1 = min(y0 + 1.0, GRID.y - 1.0);
	float row = floor(float(i) / 8.0);
	vec2 tile = vec2(float(i) - row * 8.0, row) * 128.0;
	float r0 = floor(y0 / 4.0), r1 = floor(y1 / 4.0);
	vec2 uv0 = (tile + vec2(y0 - r0 * 4.0, r0) * 32.0 + v.xz) / ATLAS;
	vec2 uv1 = (tile + vec2(y1 - r1 * 4.0, r1) * 32.0 + v.xz) / ATLAS;
	vec3 s = mix(textureLod(uTex1, uv0, 0.0).rgb, textureLod(uTex1, uv1, 0.0).rgb, ty);
	// Zero at a wall, full a few centimetres off it.
	float wall = clamp((s.r / WALL_FLOOR - 1.0) * 4.0, 0.0, 1.0);
	// The fill reaches a voxel when growth passes its arrival time; the
	// last quarter of the fill is the cloud's soft outer edge.
	float reached = clamp((uBox[i].w - s.g) * 2.6, 0.0, 1.0);
	return vec2(wall * reached * uCol[i].a, s.b);
}

float cloudField(int i, vec3 p) { return cloudSample(i, p).x; }

void main()
{
	float n = uNearFar.x, f = uNearFar.y;
	// Same UV -> clip mapping as MotionBlurEffect: on Vulkan and Metal the
	// top row is v = 0, and the projection handed to effects is the
	// unflipped one.
#if defined(VULKAN)
	vec2 ndc = vec2(vTexcoord.x * 2.0 - 1.0, 1.0 - vTexcoord.y * 2.0);
#else
	vec2 ndc = vTexcoord * 2.0 - 1.0;
#endif
	vec3 viewRay = vec3(ndc.x / matProj[0][0], ndc.y / matProj[1][1], -1.0);
	float rayScale = length(viewRay);
	vec3 rd = normalize(mat3(uInverseView) * viewRay);
	vec3 ro = uInverseView[3].xyz;

	float depth = texture(uTex0, vTexcoord).r;
	float tScene = 1.0e6;
	if (depth < 0.99999) tScene = (n * f) / (depth * (n - f) + f) * rayScale;

	// Which clouds does this ray cross, and where.
	int hitId[MAX_HITS];
	float hitIn[MAX_HITS];
	float hitOut[MAX_HITS];
	int hits = 0;
	float tNear = 1.0e9, tFar = -1.0;
	vec3 safe = vec3(abs(rd.x) < 1e-5 ? 1e-5 : rd.x, abs(rd.y) < 1e-5 ? 1e-5 : rd.y, abs(rd.z) < 1e-5 ? 1e-5 : rd.z);
	vec3 inv = 1.0 / safe;
	for (int i = 0; i < CLOUDS; i++) {
		if (uCol[i].a <= 0.0) continue;
		vec3 a = (uBMin[i].xyz - ro) * inv;
		vec3 b = (uBMax[i].xyz - ro) * inv;
		vec3 lo = min(a, b), hi = max(a, b);
		float t0 = max(max(lo.x, lo.y), max(lo.z, 0.0));
		float t1 = min(min(hi.x, hi.y), min(hi.z, tScene));
		if (t1 <= t0) continue;
		if (hits < MAX_HITS) {
			hitId[hits] = i; hitIn[hits] = t0; hitOut[hits] = t1;
			hits++;
			tNear = min(tNear, t0); tFar = max(tFar, t1);
		}
	}
	if (hits == 0) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }

	// Every pixel starts its samples at a different point along the first
	// step, which turns the banding a fixed step would draw into grain.
	float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	vec3 L = uSunDir.xyz;
	float sigma = uP0.z;
	vec3 drift = uWind.xyz * uP0.x;

	vec3 light = vec3(0.0);
	float T = 1.0;
	// Samples sit on one grid of distances from the eye whichever cloud's
	// box the ray meets first, so the edge of a box is not a seam.
	float t = (floor(tNear / uP0.w) + jitter) * uP0.w;
	if (t < tNear) t += uP0.w;
	int maxSteps = int(uP1.w);
	// The surface of a cloud is a quarter of a metre from clear to solid,
	// thinner than one step, so where a ray happened to take its first
	// sample inside decided how bright that pixel came out - and since each
	// pixel starts at a different offset, neighbours disagreed in a
	// cross-hatch. Finding the surface, going back a step and crossing it in
	// short strides puts every ray's first sample at nearly the same depth.
	int fine = 0;
	bool wasClear = true;
	for (int s = 0; s < 96; s++) {
		if (s >= maxSteps || t >= tFar) break;
		float coarse = uP0.w * max(1.0, t / 30.0);
		float dt = fine > 0 ? coarse * 0.3 : coarse;
		if (fine > 0) fine--;
		vec3 p = ro + rd * t;

		bool inside = false;
		// The clouds thickest at p, to be asked first when looking for
		// what shades p: in a pile one of them is almost always solid
		// toward the sun as well, and the search can stop there.
		int thickId[3];
		float thickW[3];
		int thickCount = 0;
		float field = 0.0, wsum = 0.0, sunSeen = 0.0;
		// Fire: how much of what is here is burning, and how far up its
		// cloud this point is.
		float fire = 0.0, fireHeight = 0.0, fireBlast = 0.0;
		vec3 albedo = vec3(0.0);
		for (int k = 0; k < MAX_HITS; k++) {
			if (k >= hits) break;
			if (t < hitIn[k] || t > hitOut[k]) continue;
			inside = true;
			vec2 cs = cloudSample(hitId[k], p);
			float w = cs.x;
			field = max(field, w);
			albedo += uCol[hitId[k]].rgb * w;
			sunSeen += cs.y * w;
			wsum += w;
			if (w > 0.0) {
				if (thickCount < 3) { thickId[thickCount] = hitId[k]; thickW[thickCount] = w; thickCount++; }
				else {
					int low = thickW[0] < thickW[1] ? 0 : 1;
					if (thickW[2] < thickW[low]) low = 2;
					if (w > thickW[low]) { thickId[low] = hitId[k]; thickW[low] = w; }
				}
			}
			int ci = hitId[k];
			if (uBMin[ci].w > 0.0) {
				fire += uBMin[ci].w * w;
				fireBlast += uBMax[ci].w * w;
				fireHeight += clamp((p.y - uBMin[ci].y) / max(uBMax[ci].y - uBMin[ci].y, 0.1), 0.0, 1.0) * w;
			}
		}
		if (!inside) {
			// Between two clouds: jump to the next one instead of walking.
			float next = 1.0e9;
			for (int k = 0; k < MAX_HITS; k++) {
				if (k >= hits) break;
				if (hitIn[k] > t) next = min(next, hitIn[k]);
			}
			if (next > 1.0e8) break;
			t = (floor(next / coarse) + jitter) * coarse;
			if (t <= next) t += coarse;
			continue;
		}

		if (field > 0.004) {
			float billow = fbm(p * uP1.x - drift);
			float d = clamp((field * 1.1 - billow * uP1.y) * 4.0, 0.0, 1.0);
			// Fire is not lit smoke. It is drawn as tongues: noise stretched
			// tall and streaming upward, cut by a threshold that climbs with
			// height, so the base burns nearly solid and only the strongest
			// licks reach the top. It adds its own light and barely hides
			// what is behind it.
			float burning = fire > 0.0 ? fire / wsum : 0.0;
			if (burning > 0.0) {
				// An explosion is the same fire with the height taken out of
				// it: it burns through and through, and boils instead of
				// streaming up.
				float blast = clamp(fireBlast / max(fire, 1e-4), 0.0, 1.0);
				float up = fireHeight / wsum * (1.0 - blast);
				float climb = uP0.x * mix(3.4, 1.2, blast);
				vec3 fp = mix(vec3(p.x * 2.3, p.y * 0.85 - climb, p.z * 2.3), p * 1.5 - vec3(0.0, climb, 0.0), blast);
				float lick = 0.62 * vnoise(fp) + 0.38 * vnoise(fp * 2.4 + vec3(5.2, -uP0.x * 2.1, 1.7));
				float cut = mix(0.20 + 0.68 * up, 0.08, blast);
				float flame = clamp((lick - cut) * 4.5, 0.0, 1.0) * smoothstep(0.0, 0.3, field);
				// the heart of a blast is white hot
				flame = mix(flame, max(flame, smoothstep(0.35, 0.9, field)), blast);
				if (flame > 0.0) {
					// hottest low and in the thick of it: red at the tips,
					// orange through the body, yellow at the heart
					float heat = clamp(flame * (1.2 - up) * mix(1.0, 0.95 + 0.6 * field, blast), 0.0, 1.0);
					vec3 glow = mix(vec3(0.80, 0.09, 0.01), vec3(1.0, 0.40, 0.04), smoothstep(0.0, 0.45, heat));
					glow = mix(glow, vec3(1.0, 0.80, 0.28), smoothstep(0.6, 1.0, heat));
					// A blast is white hot nearly all the way through, with
					// only its ragged rim cooling to orange. HDR on purpose.
					glow = mix(glow, vec3(1.6, 1.45, 1.1), blast * smoothstep(0.12, 0.6, field));
					vec3 tint = albedo / wsum;
					// Laid over what is behind rather than summed with it: a
					// deep fire then settles on the colour of its nearest
					// flames, where adding every layer up just went white.
					float cover = 1.0 - exp(-flame * mix(1.8, 5.0, blast) * dt);
					light += T * cover * tint * glow * min(burning, 3.0);
					T *= 1.0 - cover;
				}
				// whatever share of this point is fire is not smoke
				d *= 1.0 - min(burning, 1.0);
			}
			if (d > 0.0 && wasClear && fine == 0) {
				t = max(t - coarse, tNear);
				fine = 9;
				wasClear = false;
				continue;
			}
			wasClear = d <= 0.0;
			if (d > 0.0) {
				albedo /= wsum;
				sunSeen /= wsum;
				// How much smoke lies between here and the sun. Two taps
				// of the plain field - the billows are too fine to matter
				// to a shadow and cost eight hashes a tap.
				float tau = 0.0;
				for (int j = 0; j < 2; j++) {
					float dist = j == 0 ? 0.7 : 2.2;
					float seg = j == 0 ? 1.4 : 2.4;
					vec3 q = p + L * dist;
					float fq = 0.0;
					// Every cloud the ray crosses that q lies in - not only
					// the ones with smoke at p. A cloud shades the air beside
					// it too, and asking only p's own clouds cut each one's
					// shadow off square at the edge of its own smoke: the
					// faint boxes in a pile of clouds.
					for (int k = 0; k < 3; k++) {
						if (k >= thickCount) break;
						fq = max(fq, cloudField(thickId[k], q));
					}
					for (int k = 0; k < MAX_HITS; k++) {
						// Past the point where the clamp below saturates,
						// a thicker cloud would change nothing: stopping
						// here is exact, not an approximation.
						if (k >= hits || fq > 0.5 * uP1.y + 0.34) break;
						int c = hitId[k];
						if (any(lessThan(q, uBMin[c].xyz)) || any(greaterThan(q, uBMax[c].xyz))) continue;
						fq = max(fq, cloudField(c, q));
					}
					tau += clamp((fq - 0.5 * uP1.y) * 3.0, 0.0, 1.0) * seg;
				}
				float sun = exp(-tau * sigma * uP1.z);
				// Light that has bounced around inside the cloud: never
				// lets the shaded side go to black.
				// The floor is that bounce - and under a roof there is no
				// sun to bounce.
				sun = (0.25 + 0.75 * sun) * sunSeen;
				vec3 incoming = uSunColor.rgb * sun + uAmbient.rgb * (1.0 - 0.4 * field);
				// Lamps: the scene's own falloff, dimmed by the smoke between
				// the lamp and here the cheap way - by how deep in we are.
				int lamps = int(uSunDir.w);
				for (int j = 0; j < 8; j++) {
					if (j >= lamps) break;
					vec3 toLamp = uLPos[j].xyz - p;
					float dist = length(toLamp);
					float att = clamp(1.0 - dist / uLPos[j].w, 0.0, 1.0);
					if (att <= 0.0) continue;
					if (uLCol[j].w > -0.5) {
						float c = dot(-toLamp / max(dist, 1e-4), uLDir[j].xyz);
						att *= smoothstep(uLCol[j].w, uLDir[j].w, c);
					}
					incoming += uLCol[j].rgb * att * (1.0 - 0.55 * field);
				}
				vec3 lit = albedo * incoming;

				float a = 1.0 - exp(-d * sigma * dt);
				light += T * a * lit;
				T *= 1.0 - a;
				if (T < 0.02) { T = 0.0; break; }
			}
		} else {
			// Clear air inside a cloud's box: cover it in longer strides.
			wasClear = true;
			if (fine == 0) t += dt;
		}
		t += dt;
	}
	FragColor = vec4(light, T);
}
)GLSL";

		const char* kComposite = R"GLSL(
SAMPLER_BINDING(0) uniform sampler2D uTex0;   // the frame
SAMPLER_BINDING(1) uniform sampler2D uTex1;   // the march, half resolution
SAMPLER_BINDING(2) uniform sampler2D uTex2;   // scene depth
UBO_BINDING(44) uniform VolSmokeCompositeParams {
	vec4 uNearFar;
	vec4 uFlash;     // colour, amount: the whole screen washed out
};

float linearDepth(float d)
{
	return (uNearFar.x * uNearFar.y) / (d * (uNearFar.x - uNearFar.y) + uNearFar.y);
}

void main()
{
	vec4 scene = texture(uTex0, vTexcoord);
	ivec2 isz = textureSize(uTex1, 0);
	vec2 sz = vec2(isz);
	vec2 pos = vTexcoord * sz - 0.5;
	float here = linearDepth(texture(uTex2, vTexcoord).r);

	// 4x4 half-res texels under a tent two texels wide. The march's jitter
	// pattern repeats every few pixels, and anything narrower than its
	// period lets it through.
	ivec2 corner = ivec2(floor(pos)) - 1;
	vec4 acc = vec4(0.0);
	float wsum = 0.0;
	for (int j = 0; j < 16; j++) {
		ivec2 o = ivec2(j - (j / 4) * 4, j / 4);
		ivec2 tc = clamp(corner + o, ivec2(0), isz - 1);
		// The depth that half-res pixel marched to.
		float there = linearDepth(texture(uTex2, (vec2(tc) + 0.5) / sz).r);
		vec2 d = abs(vec2(tc) - pos);
		float w = max(0.0, 1.0 - d.x * 0.5) * max(0.0, 1.0 - d.y * 0.5);
		w = (w + 0.001) / (0.01 + abs(there - here) / here);
		acc += texelFetch(uTex1, tc, 0) * w;
		wsum += w;
	}
	vec4 smoke = acc / wsum;
	vec3 frame = scene.rgb * smoke.a + smoke.rgb;
	// Being blinded: the frame is washed toward the flash colour, and what
	// little shows through at first is only its brightest parts.
	frame = mix(frame, uFlash.rgb, uFlash.a);
	FragColor = vec4(frame, scene.a);
}
)GLSL";

		f64 NowSeconds()
		{
			return std::chrono::duration<f64>(std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		inline void Put(std::vector<uchar> &block, const uint32 offset, const f32 x, const f32 y, const f32 z, const f32 w)
		{
			const f32 v[4] = { x, y, z, w };
			memcpy(&block[offset], v, sizeof(v));
		}
	}

	VolumetricSmokeEffect::VolumetricSmokeEffect(const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		// rgb is light, and scenes are lit in HDR.
		UseHDRAttachment();
		UseRTT(RTT::Depth);

		atlas = new Texture();
		atlas->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, VolumetricSmoke::AtlasWidth, VolumetricSmoke::AtlasHeight, false);
		atlas->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		atlas->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
		atlasVersion = 0xffffffffu;
		UseCustomTexture(atlas);

		density = 3.0f;
		stepSize = 0.35f;
		maxSteps = 56.f;
		noiseScale = 0.45f;
		noiseErosion = 0.7f;
		shadow = 0.3f;
		startTime = NowSeconds();

		FragmentShaderString = std::string(kPrelude) + kMarch;
		CompileShaders();

		AddUniform(Uniform("matProj", Uniforms::PostEffects::ProjectionFromScene));
		AddUniform(Uniform("uInverseView", Uniforms::PostEffects::InverseViewFromScene));
		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));

		extraUniformsBinding = 44;
		extraUniformsBlockName = "VolSmokeParams";
		extraUniformsSize = BlockSize;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["matProj"] = OffProj;
		extraUniformOffsets["uInverseView"] = OffInverseView;
		extraUniformOffsets["uNearFar"] = OffNearFar;
	}

	VolumetricSmokeEffect::~VolumetricSmokeEffect()
	{
		delete atlas;
	}

	void VolumetricSmokeEffect::PreDraw()
	{
		// The clouds are plain CPU state anyone may have changed since the
		// last frame, so everything about them is packed fresh here.
		if (atlasVersion != VolumetricSmoke::GetAtlasVersion())
		{
			atlasVersion = VolumetricSmoke::GetAtlasVersion();
			const std::vector<uchar> &data = VolumetricSmoke::GetAtlas();
			atlas->UpdateData((void*)&data[0]);
		}

		// Wrapped, so the noise coordinates stay small enough for a float
		// to tell neighbouring metres apart after a long session.
		const f32 time = (f32)fmod(NowSeconds() - startTime, 3600.0);
		const Vec3 &sun = VolumetricSmoke::GetLightDirection();
		const Vec3 &sunColor = VolumetricSmoke::GetLightColor();
		const Vec3 &ambient = VolumetricSmoke::GetAmbient();
		const Vec3 &wind = VolumetricSmoke::GetWind();

		std::vector<uchar> &b = extraUniformsScratch;
		Put(b, OffP0, time, VolumetricSmoke::GetCellSize(), density, stepSize);
		Put(b, OffP1, noiseScale, noiseErosion, shadow, maxSteps);
		// The lamps that can reach a cloud at all, nearest reach first come
		// first served: eight is plenty for what stands next to a cloud.
		const std::vector<VolumetricSmoke::LocalLight> &lamps = VolumetricSmoke::GetLocalLights();
		uint32 used = 0;
		for (size_t l = 0; l < lamps.size() && used < MaxLocalLights; l++)
		{
			const VolumetricSmoke::LocalLight &lamp = lamps[l];
			bool reaches = false;
			for (uint32 i = 0; i < VolumetricSmoke::MaxClouds && !reaches; i++)
			{
				const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud(i);
				if (!c.active)
					continue;
				// distance from the lamp to the cloud's box
				const f32 dx = lamp.position.x < c.boundsMin.x ? c.boundsMin.x - lamp.position.x : (lamp.position.x > c.boundsMax.x ? lamp.position.x - c.boundsMax.x : 0.f);
				const f32 dy = lamp.position.y < c.boundsMin.y ? c.boundsMin.y - lamp.position.y : (lamp.position.y > c.boundsMax.y ? lamp.position.y - c.boundsMax.y : 0.f);
				const f32 dz = lamp.position.z < c.boundsMin.z ? c.boundsMin.z - lamp.position.z : (lamp.position.z > c.boundsMax.z ? lamp.position.z - c.boundsMax.z : 0.f);
				reaches = dx * dx + dy * dy + dz * dz < lamp.radius * lamp.radius;
			}
			if (!reaches)
				continue;
			Put(b, OffLightPos + used * 16, lamp.position.x, lamp.position.y, lamp.position.z, lamp.radius > 0.01f ? lamp.radius : 0.01f);
			Put(b, OffLightColor + used * 16, lamp.color.x, lamp.color.y, lamp.color.z, lamp.cosOuter);
			Put(b, OffLightDir + used * 16, lamp.direction.x, lamp.direction.y, lamp.direction.z, lamp.cosInner);
			used++;
		}
		Put(b, OffSunDir, sun.x, sun.y, sun.z, (f32)used);
		Put(b, OffSunColor, sunColor.x, sunColor.y, sunColor.z, 0.f);
		Put(b, OffAmbient, ambient.x, ambient.y, ambient.z, 0.f);
		Put(b, OffWind, wind.x, wind.y, wind.z, 0.f);
		for (uint32 i = 0; i < VolumetricSmoke::MaxClouds; i++)
		{
			const VolumetricSmoke::Cloud &c = VolumetricSmoke::GetCloud(i);
			Put(b, OffBox + i * 16, c.boxMin.x, c.boxMin.y, c.boxMin.z, c.growth);
			Put(b, OffColor + i * 16, c.color.x, c.color.y, c.color.z, c.active ? c.density : 0.f);
			Put(b, OffBoundsMin + i * 16, c.boundsMin.x, c.boundsMin.y, c.boundsMin.z, c.emission);
			Put(b, OffBoundsMax + i * 16, c.boundsMax.x, c.boundsMax.y, c.boundsMax.z, c.blast);
		}
	}

	// ------------------------------------------------------------ composite

	VolumetricSmokeCompositeEffect::VolumetricSmokeCompositeEffect(const uint32 TexColor, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();
		UseRTT(TexColor);
		Build();
	}

	VolumetricSmokeCompositeEffect::VolumetricSmokeCompositeEffect(Texture* color, const uint32 Width, const uint32 Height) : IEffect(Width, Height)
	{
		UseHDRAttachment();
		UseCustomTexture(color);
		Build();
	}

	void VolumetricSmokeCompositeEffect::Build()
	{
		UseRTT(RTT::LastRTT);
		UseRTT(RTT::Depth);

		FragmentShaderString = std::string(kPrelude) + kComposite;
		CompileShaders();

		AddUniform(Uniform("uNearFar", Uniforms::PostEffects::NearFarPlane));
		extraUniformsBinding = 44;
		extraUniformsBlockName = "VolSmokeCompositeParams";
		extraUniformsSize = 32;
		extraUniformsScratch.resize(extraUniformsSize, 0);
		extraUniformOffsets["uNearFar"] = 0;
	}

	void VolumetricSmokeCompositeEffect::PreDraw()
	{
		const Vec3 &c = VolumetricSmoke::GetScreenFlashColor();
		Put(extraUniformsScratch, 16, c.x, c.y, c.z, VolumetricSmoke::GetScreenFlash());
	}

	VolumetricSmokeCompositeEffect::~VolumetricSmokeCompositeEffect()
	{
	}

};
