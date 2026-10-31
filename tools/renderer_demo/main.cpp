// The renderer demo: Erangel's Island on the new device layer (the device layer, WebGPU - on Metal here).
// Its real terrain, its scanned trees where they stand, its grass from the density maps; lit physically,
// a sky, haze, HDR and a filmic curve. Stage 1 of the visuals (shadows, TAA, AO next).
//   renderer_demo [scene.json]       env: PYROS_CAM="x y z yaw pitch"  PYROS_SECONDS=n  PYROS_PROFILE_LOG=file
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <Pyros3D/Utils/Json/json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <Pyros3D/Ext/stb/stb_image.h>
#include <SDL.h>
#include <SDL_metal.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace p3d::gfx;
using json = nlohmann::json;

// ---------------------------------------------------------------- math
struct V3 { float x, y, z; };
static V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
static float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 Cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
static V3 Norm(V3 a) { float l = sqrtf(Dot(a, a)); return l > 0 ? a * (1.f / l) : a; }
struct M4 { float m[16]; };
static M4 Mul(const M4 &a, const M4 &b) { M4 r; for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) { float s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k]; r.m[c * 4 + rr] = s; } return r; }
static M4 Perspective(float fovy, float aspect, float zn) { float f = 1.f / tanf(fovy / 2); M4 r = {}; r.m[0] = f / aspect; r.m[5] = f; r.m[11] = -1.f; r.m[14] = zn; return r; }   // reversed Z, infinite
static M4 View(V3 eye, V3 fwd)
{
	V3 f = Norm(fwd), s = Norm(Cross(f, { 0, 1, 0 })), u = Cross(s, f);
	M4 r = { { s.x, u.x, -f.x, 0, s.y, u.y, -f.y, 0, s.z, u.z, -f.z, 0, -Dot(s, eye), -Dot(u, eye), Dot(f, eye), 1 } };
	return r;
}

// ---------------------------------------------------------------- loading
struct Image { int w = 0, h = 0; std::vector<uint8_t> px; };
static bool LoadImage(const std::string &path, Image &out)
{
	int n = 0; unsigned char* d = stbi_load(path.c_str(), &out.w, &out.h, &n, 4);
	if (!d) { fprintf(stderr, "no image %s\n", path.c_str()); return false; }
	out.px.assign(d, d + (size_t)out.w * out.h * 4); stbi_image_free(d); return true;
}
// a texture with its whole mip chain (a cut-out's alpha kept at its coverage, as BlockCompress does)
static Texture MakeTexture(Device* dev, const Image &im, bool srgb, bool cutout)
{
	int levels = 1; while ((im.w >> levels) > 0 || (im.h >> levels) > 0) levels++;
	TextureDesc td; td.width = im.w; td.height = im.h; td.mips = levels; td.format = srgb ? Format::RGBA8_sRGB : Format::RGBA8;
	std::vector<std::vector<uint8_t> > mips(levels); mips[0] = im.px;
	float cover0 = 0; if (cutout) { size_t c = 0; for (size_t i = 3; i < im.px.size(); i += 4) c += im.px[i] >= 115; cover0 = (float)c / (im.px.size() / 4); }
	int w = im.w, h = im.h;
	for (int l = 1; l < levels; l++)
	{
		const int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
		const std::vector<uint8_t> &a = mips[l - 1]; std::vector<uint8_t> &b = mips[l]; b.assign((size_t)nw * nh * 4, 0);
		for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++)
		{
			int as = 0; float c[3] = { 0, 0, 0 }, cp[3] = { 0, 0, 0 };
			for (int k = 0; k < 4; k++)
			{
				const int sx = std::min(w - 1, x * 2 + (k & 1)), sy = std::min(h - 1, y * 2 + (k >> 1));
				const uint8_t* p = &a[((size_t)sy * w + sx) * 4];
				as += p[3]; for (int ch = 0; ch < 3; ch++) { cp[ch] += p[ch]; c[ch] += p[ch] * p[3]; }
			}
			uint8_t* o = &b[((size_t)y * nw + x) * 4];
			for (int ch = 0; ch < 3; ch++) o[ch] = (uint8_t)(cutout && as > 0 ? c[ch] / as : cp[ch] / 4);
			o[3] = (uint8_t)(as / 4);
		}
		if (cutout && cover0 > 0)
		{
			float lo = 1, hi = 8;
			for (int it = 0; it < 12; it++) { float mid = (lo + hi) / 2; size_t cc = 0; for (size_t i = 3; i < b.size(); i += 4) cc += b[i] * mid >= 115; if ((float)cc / (b.size() / 4) < cover0) lo = mid; else hi = mid; }
			for (size_t i = 3; i < b.size(); i += 4) b[i] = (uint8_t)std::min(255.f, b[i] * hi);
		}
		w = nw; h = nh;
	}
	std::vector<TextureData> data(levels);
	for (int l = 0; l < levels; l++) { data[l].mip = l; data[l].bytes = mips[l].data(); data[l].size = mips[l].size(); }
	return dev->CreateTexture(td, data.data(), levels);
}

struct Vertex { float p[3], n[3], uv[2]; };
struct Mesh { Buffer vb, ib; uint32_t count = 0; std::string texture; float radius = 0; };
// an OBJ as tools/blender_billboard_tree.py and tools/blender_grass_cards.py write them (Y up, metres)
static bool LoadObj(Device* dev, const std::string &path, Mesh &out)
{
	std::ifstream f(path); if (!f) { fprintf(stderr, "no obj %s\n", path.c_str()); return false; }
	std::vector<V3> P, N; std::vector<float> T; std::vector<Vertex> verts; std::vector<uint32_t> idx;
	std::map<std::string, uint32_t> seen;
	std::string line, mtl;
	while (std::getline(f, line))
	{
		if (line.compare(0, 2, "v ") == 0) { V3 v; sscanf(line.c_str() + 2, "%f %f %f", &v.x, &v.y, &v.z); P.push_back(v); }
		else if (line.compare(0, 3, "vn ") == 0) { V3 v; sscanf(line.c_str() + 3, "%f %f %f", &v.x, &v.y, &v.z); N.push_back(v); }
		else if (line.compare(0, 3, "vt ") == 0) { float u, v; sscanf(line.c_str() + 3, "%f %f", &u, &v); T.push_back(u); T.push_back(1.f - v); }
		else if (line.compare(0, 7, "mtllib ") == 0) mtl = line.substr(7);
		else if (line.compare(0, 2, "f ") == 0)
		{
			char* s = &line[2]; std::vector<uint32_t> face;
			while (*s)
			{
				while (*s == ' ') s++;
				if (!*s) break;
				char* e = s; while (*e && *e != ' ') e++;
				std::string key(s, e);
				std::map<std::string, uint32_t>::iterator it = seen.find(key);
				if (it == seen.end())
				{
					int vi = 0, ti = 0, ni = 0; sscanf(key.c_str(), "%d/%d/%d", &vi, &ti, &ni);
					Vertex v = {}; V3 p = P[vi - 1]; v.p[0] = p.x; v.p[1] = p.y; v.p[2] = p.z;
					if (ni > 0) { V3 n = N[ni - 1]; v.n[0] = n.x; v.n[1] = n.y; v.n[2] = n.z; } else v.n[1] = 1;
					if (ti > 0) { v.uv[0] = T[(ti - 1) * 2]; v.uv[1] = T[(ti - 1) * 2 + 1]; }
					out.radius = std::max(out.radius, sqrtf(p.x * p.x + p.z * p.z) + 0.f);
					it = seen.insert(std::make_pair(key, (uint32_t)verts.size())).first; verts.push_back(v);
				}
				face.push_back(it->second);
				s = e;
			}
			for (size_t k = 2; k < face.size(); k++) { idx.push_back(face[0]); idx.push_back(face[k - 1]); idx.push_back(face[k]); }
		}
	}
	if (!mtl.empty())
	{
		std::ifstream m(path.substr(0, path.find_last_of('/') + 1) + mtl);
		while (std::getline(m, line)) if (line.compare(0, 7, "map_Kd ") == 0) out.texture = line.substr(7);
	}
	BufferDesc vb; vb.size = verts.size() * sizeof(Vertex); vb.usage = BufVertex; out.vb = dev->CreateBuffer(vb, verts.data());
	BufferDesc ib; ib.size = idx.size() * 4; ib.usage = BufIndex; out.ib = dev->CreateBuffer(ib, idx.data());
	out.count = (uint32_t)idx.size();
	return true;
}

// ---------------------------------------------------------------- shaders
// What every shader shares: the frame's numbers (group 0, the draw's data - the same for every draw here)
// and the light: the sun, the sky's light from above and the ground's from below, and the haze.
static const char* kCommon = R"(
struct Frame {
  viewProj : mat4x4<f32>,
  camPos : vec4<f32>,      // xyz, w time
  sunDir : vec4<f32>,      // towards the sun, w its intensity
  sunColor : vec4<f32>,
  skyTop : vec4<f32>,      // the sky's light from straight up, w exposure
  skyHorizon : vec4<f32>,  // w: haze density
  camRight : vec4<f32>,    // xyz, w tan(fov/2) * aspect
  camUp : vec4<f32>,       // xyz, w tan(fov/2)
  camFwd : vec4<f32>,
};
@group(0) @binding(0) var<uniform> F : Frame;

const PI = 3.14159265;
fn ggx(n : vec3<f32>, v : vec3<f32>, l : vec3<f32>, rough : f32, f0 : vec3<f32>) -> vec3<f32> {
  let h = normalize(v + l);
  let a = rough * rough; let a2 = a * a;
  let nh = max(dot(n, h), 0.0); let nv = max(dot(n, v), 1e-4); let nl = max(dot(n, l), 0.0);
  let d = a2 / (PI * pow(nh * nh * (a2 - 1.0) + 1.0, 2.0));
  let k = (rough + 1.0) * (rough + 1.0) / 8.0;
  let g = (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
  let fr = f0 + (vec3<f32>(1.0) - f0) * pow(1.0 - max(dot(h, v), 0.0), 5.0);
  return d * g * fr / (4.0 * nv * nl + 1e-4);
}
// the sky's light on a surface facing n: the sky above, the ground's bounce below
fn skyLight(n : vec3<f32>) -> vec3<f32> {
  let up = n.y * 0.5 + 0.5;
  let ground = vec3<f32>(0.18, 0.17, 0.12) * F.sunDir.w * max(F.sunDir.y, 0.0) * 0.12;
  return mix(ground, F.skyTop.rgb, up);
}
fn skyColor(dir : vec3<f32>) -> vec3<f32> {
  let d = normalize(dir);
  let h = clamp(d.y, -0.2, 1.0);
  var c = mix(F.skyHorizon.rgb, F.skyTop.rgb * 0.9, pow(max(h, 0.0), 0.45));
  if (h < 0.0) { c = mix(F.skyHorizon.rgb, vec3<f32>(0.25, 0.27, 0.27) * F.skyHorizon.rgb, min(-h * 5.0, 1.0)); }
  let mu = max(dot(d, F.sunDir.xyz), 0.0);
  c += F.sunColor.rgb * F.sunDir.w * (0.0035 * pow(mu, 8.0) + 0.012 * pow(mu, 64.0));       // the glow round the sun
  if (mu > 0.99996) { c += F.sunColor.rgb * F.sunDir.w * 60.0; }                             // and the sun
  return c;
}
// the sun's shadow: three cascades, each its own map (group 1, every material has them), a 3 x 3 filter
struct Shadow { m : array<mat4x4<f32>, 3>, splits : vec4<f32>, texel : vec4<f32> };
fn shadowAt(sm : texture_depth_2d, cmp : sampler_comparison, m : mat4x4<f32>, wp : vec3<f32>, bias : f32, texel : f32) -> f32 {
  let c = m * vec4<f32>(wp, 1.0);
  let uv = vec2<f32>(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
  if (any(uv < vec2<f32>(0.0)) || any(uv > vec2<f32>(1.0))) { return 1.0; }
  var s = 0.0;
  for (var y = -1; y <= 1; y++) { for (var x = -1; x <= 1; x++) {
    s += textureSampleCompareLevel(sm, cmp, uv + vec2<f32>(f32(x), f32(y)) * texel, c.z + bias);
  } }
  return s / 9.0;
}
// haze: thicker low down, the sky's colour along the view, the sun's in its direction
fn haze(c : vec3<f32>, wp : vec3<f32>) -> vec3<f32> {
  let v = wp - F.camPos.xyz; let dist = length(v);
  let fall = 0.012;
  let k = F.skyHorizon.w * exp(-fall * max(F.camPos.y, 0.0)) * (1.0 - exp(-fall * v.y)) / max(fall * v.y, 1e-4);
  let amount = 1.0 - exp(-dist * (select(F.skyHorizon.w, k, abs(v.y) > 1e-3)));
  let toward = pow(max(dot(normalize(v), F.sunDir.xyz), 0.0), 6.0);
  let fogc = F.skyHorizon.rgb * 0.95 + F.sunColor.rgb * F.sunDir.w * 0.05 * toward;
  return mix(c, fogc, clamp(amount, 0.0, 1.0));
}
)";

static const char* kSky = R"(
struct VO { @builtin(position) pos : vec4<f32>, @location(0) ndc : vec2<f32> };
@vertex fn vs_main(@builtin(vertex_index) i : u32) -> VO {
  var p = array<vec2<f32>, 3>(vec2<f32>(-1.0, -1.0), vec2<f32>(3.0, -1.0), vec2<f32>(-1.0, 3.0));
  var o : VO; o.pos = vec4<f32>(p[i], 0.0, 1.0); o.ndc = p[i]; return o;
}
@fragment fn fs_main(i : VO) -> @location(0) vec4<f32> {
  let dir = F.camFwd.xyz + F.camRight.xyz * (i.ndc.x * F.camRight.w) + F.camUp.xyz * (i.ndc.y * F.camUp.w);
  return vec4<f32>(skyColor(dir), 1.0);
}
)";

static const char* kTerrain = R"(
@group(1) @binding(0) var splat : texture_2d<f32>;
@group(1) @binding(1) var l0 : texture_2d<f32>;
@group(1) @binding(2) var l1 : texture_2d<f32>;
@group(1) @binding(3) var l2 : texture_2d<f32>;
@group(1) @binding(4) var l3 : texture_2d<f32>;
@group(1) @binding(5) var smp : sampler;
@group(1) @binding(6) var clampSmp : sampler;
@group(1) @binding(7) var<uniform> SH : Shadow;
@group(1) @binding(8) var sm0 : texture_depth_2d;
@group(1) @binding(9) var sm1 : texture_depth_2d;
@group(1) @binding(10) var sm2 : texture_depth_2d;
@group(1) @binding(11) var cmp : sampler_comparison;
fn sunShadow(wp : vec3<f32>, n : vec3<f32>) -> f32 {
  let d = length(wp - F.camPos.xyz);
  let p = wp + n * 0.04 * (1.0 + d * 0.05);   // (pushed out along its normal: no acne)
  if (d < SH.splits.x) { return shadowAt(sm0, cmp, SH.m[0], p, 0.0004, SH.texel.x); }
  if (d < SH.splits.y) { return shadowAt(sm1, cmp, SH.m[1], p, 0.0004, SH.texel.x); }
  if (d < SH.splits.z) { return mix(shadowAt(sm2, cmp, SH.m[2], p, 0.0006, SH.texel.x), 1.0, smoothstep(SH.splits.z * 0.85, SH.splits.z, d)); }
  return 1.0;
}

struct VO { @builtin(position) pos : vec4<f32>, @location(0) wp : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32>) -> VO {
  var o : VO; o.pos = F.viewProj * vec4<f32>(p, 1.0); o.wp = p; o.n = n; o.uv = uv; return o;
}
@fragment fn fs_main(i : VO) -> @location(0) vec4<f32> {
  let n = normalize(i.n);
  var w = textureSample(splat, clampSmp, i.uv);
  // (each layer at two sizes, turned: no four-metre tile pattern from a distance)
  let a = i.wp.xz / 4.0;
  let b = vec2<f32>(i.wp.x * 0.8 + i.wp.z * 0.6, i.wp.z * 0.8 - i.wp.x * 0.6) / 19.0;
  let far = clamp(length(i.wp - F.camPos.xyz) / 80.0, 0.25, 0.85);
  let c0 = mix(textureSample(l0, smp, a), textureSample(l0, smp, b), far);
  let c1 = mix(textureSample(l1, smp, a), textureSample(l1, smp, b), far);
  let c2 = mix(textureSample(l2, smp, a * 0.6), textureSample(l2, smp, b), far);
  let c3 = mix(textureSample(l3, smp, a), textureSample(l3, smp, b), far);
  // rock on the steep, and each layer's say grows with its height (heightblend)
  let steep = smoothstep(0.62, 0.78, 1.0 - n.y);
  w = vec4<f32>(w.r * (1.0 - steep), w.g * (1.0 - steep), max(w.b, steep), w.a * (1.0 - steep));
  let hgt = vec4<f32>(c0.a, c1.a, c2.a, c3.a);
  let say = w + hgt * 0.5 * smoothstep(vec4<f32>(0.0), vec4<f32>(0.15), w);
  let top = max(max(say.r, say.g), max(say.b, say.a)) - 0.18;
  var bl = max(say - vec4<f32>(top), vec4<f32>(0.0)); bl = bl / max(bl.r + bl.g + bl.b + bl.a, 1e-4);
  let albedo = c0.rgb * bl.r + c1.rgb * bl.g + c2.rgb * bl.b + c3.rgb * bl.a;
  let rough = dot(bl, vec4<f32>(0.95, 0.9, 0.75, 0.9));
  let v = normalize(F.camPos.xyz - i.wp); let l = F.sunDir.xyz;
  let nl = max(dot(n, l), 0.0);
  let direct = (albedo / PI + ggx(n, v, l, rough, vec3<f32>(0.04))) * F.sunColor.rgb * F.sunDir.w * nl * sunShadow(i.wp, n);
  let ambient = albedo * skyLight(n);
  return vec4<f32>(haze(direct + ambient, i.wp), 1.0);
}
)";

// trees and grass: instanced (a matrix an instance, binding 1), cut out, two-sided; leaves let light through
static const char* kMesh = R"(
@group(1) @binding(0) var tex : texture_2d<f32>;
@group(1) @binding(1) var smp : sampler;
struct VO { @builtin(position) pos : vec4<f32>, @location(0) wp : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32>, @location(3) tint : f32 };
@vertex fn vs_main(@location(0) p : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32>,
                   @location(3) m0 : vec4<f32>, @location(4) m1 : vec4<f32>, @location(5) m2 : vec4<f32>, @location(6) m3 : vec4<f32>) -> VO {
  let model = mat4x4<f32>(m0, m1, m2, m3);
  var wp = (model * vec4<f32>(p, 1.0)).xyz;
  // wind: the higher, the more; each instance its own phase
  let phase = m3.x * 0.37 + m3.z * 0.21;
  let sway = sin(F.camPos.w * 1.3 + phase + wp.y * 0.4) * 0.04 * max(p.y, 0.0);
  wp = wp + vec3<f32>(sway, 0.0, sway * 0.6);
  var o : VO; o.pos = F.viewProj * vec4<f32>(wp, 1.0); o.wp = wp;
  o.n = normalize((model * vec4<f32>(n, 0.0)).xyz); o.uv = uv;
  o.tint = 0.88 + 0.24 * fract(sin(phase * 12.9898) * 43758.5453);
  return o;
}
struct Mat { color : vec4<f32>, params : vec4<f32> };   // params: x alpha cut (0 opaque), y light through, z roughness
@group(1) @binding(2) var<uniform> M : Mat;
@group(1) @binding(3) var<uniform> SH : Shadow;
@group(1) @binding(4) var sm0 : texture_depth_2d;
@group(1) @binding(5) var sm1 : texture_depth_2d;
@group(1) @binding(6) var sm2 : texture_depth_2d;
@group(1) @binding(7) var cmp : sampler_comparison;
fn sunShadow(wp : vec3<f32>, n : vec3<f32>) -> f32 {
  let d = length(wp - F.camPos.xyz);
  let p = wp + n * 0.04 * (1.0 + d * 0.05);   // (pushed out along its normal: no acne)
  if (d < SH.splits.x) { return shadowAt(sm0, cmp, SH.m[0], p, 0.0004, SH.texel.x); }
  if (d < SH.splits.y) { return shadowAt(sm1, cmp, SH.m[1], p, 0.0004, SH.texel.x); }
  if (d < SH.splits.z) { return mix(shadowAt(sm2, cmp, SH.m[2], p, 0.0006, SH.texel.x), 1.0, smoothstep(SH.splits.z * 0.85, SH.splits.z, d)); }
  return 1.0;
}

@fragment fn fs_main(i : VO, @builtin(front_facing) front : bool) -> @location(0) vec4<f32> {
  let t = textureSample(tex, smp, i.uv);
  if (M.params.x > 0.0 && t.a < M.params.x) { discard; }
  var n = normalize(i.n); if (!front) { n = -n; }
  if (M.params.y > 0.0) { n = normalize(mix(n, vec3<f32>(0.0, 1.0, 0.0), 0.35)); }   // (a leaf is lit as its crown is, more than as itself)
  let albedo = t.rgb * M.color.rgb * i.tint;
  let v = normalize(F.camPos.xyz - i.wp); let l = F.sunDir.xyz;
  let nl = max(dot(n, l), 0.0);
  var direct = (albedo / PI + ggx(n, v, l, M.params.z, vec3<f32>(0.03))) * nl;
  // through the leaf: from behind it, toward the sun
  direct += albedo * M.params.y * pow(max(dot(-v, l), 0.0), 3.0) * 0.6 + albedo * M.params.y * max(-dot(n, l), 0.0) * 0.25;
  let shade = sunShadow(i.wp, n);
  let c = direct * F.sunColor.rgb * F.sunDir.w * shade + albedo * skyLight(n) * (0.75 + 0.25 * shade);
  return vec4<f32>(haze(c, i.wp), 1.0);
}
)";

static const char* kShadowTerrain = R"(
@vertex fn vs_main(@location(0) p : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32>) -> @builtin(position) vec4<f32> {
  return F.viewProj * vec4<f32>(p, 1.0);
}
)";
static const char* kShadowMesh = R"(
@group(1) @binding(0) var tex : texture_2d<f32>;
@group(1) @binding(1) var smp : sampler;
struct Mat { color : vec4<f32>, params : vec4<f32> };
@group(1) @binding(2) var<uniform> M : Mat;
struct VO { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec3<f32>, @location(1) n : vec3<f32>, @location(2) uv : vec2<f32>,
                   @location(3) m0 : vec4<f32>, @location(4) m1 : vec4<f32>, @location(5) m2 : vec4<f32>, @location(6) m3 : vec4<f32>) -> VO {
  let model = mat4x4<f32>(m0, m1, m2, m3);
  var wp = (model * vec4<f32>(p, 1.0)).xyz;
  let phase = m3.x * 0.37 + m3.z * 0.21;
  let sway = sin(F.camPos.w * 1.3 + phase + wp.y * 0.4) * 0.04 * max(p.y, 0.0);
  wp = wp + vec3<f32>(sway, 0.0, sway * 0.6);
  var o : VO; o.pos = F.viewProj * vec4<f32>(wp, 1.0); o.uv = uv; return o;
}
@fragment fn fs_main(i : VO) {
  if (M.params.x > 0.0 && textureSample(tex, smp, i.uv).a < M.params.x) { discard; }
}
)";

static const char* kTonemap = R"(
@group(1) @binding(0) var hdr : texture_2d<f32>;
@group(1) @binding(1) var smp : sampler;
struct VO { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@builtin(vertex_index) i : u32) -> VO {
  var p = array<vec2<f32>, 3>(vec2<f32>(-1.0, -1.0), vec2<f32>(3.0, -1.0), vec2<f32>(-1.0, 3.0));
  var o : VO; o.pos = vec4<f32>(p[i], 0.0, 1.0); o.uv = vec2<f32>(p[i].x * 0.5 + 0.5, 0.5 - p[i].y * 0.5); return o;
}
fn aces(x : vec3<f32>) -> vec3<f32> {
  // (ACES fitted, Hill: the input and output matrices round the RRT+ODT curve)
  let i = mat3x3<f32>(vec3<f32>(0.59719, 0.07600, 0.02840), vec3<f32>(0.35458, 0.90834, 0.13383), vec3<f32>(0.04823, 0.01566, 0.83777));
  let o = mat3x3<f32>(vec3<f32>(1.60475, -0.10208, -0.00327), vec3<f32>(-0.53108, 1.10813, -0.07276), vec3<f32>(-0.07367, -0.00605, 1.07602));
  let v = i * x;
  let a = v * (v + 0.0245786) - 0.000090537;
  let b = v * (0.983729 * v + 0.4329510) + 0.238081;
  return clamp(o * (a / b), vec3<f32>(0.0), vec3<f32>(1.0));
}
@fragment fn fs_main(i : VO) -> @location(0) vec4<f32> {
  let c = textureSample(hdr, smp, i.uv).rgb * F.skyTop.w;
  return vec4<f32>(aces(c), 1.0);
}
)";

// ---------------------------------------------------------------- the demo
struct FrameData {
	M4 viewProj; float camPos[4], sunDir[4], sunColor[4], skyTop[4], skyHorizon[4], camRight[4], camUp[4], camFwd[4];
};
struct MatData { float color[4], params[4]; };
struct Cell { float cx, cz; uint32_t first, count; };     // a 32 m square's instances, a range of the part's buffer
struct Part { Mesh mesh; Pipeline pipe; BindGroup group, shadowGroup; Buffer instances; uint32_t count = 0; std::vector<Cell> cells; float minD = 0.f, maxD = 1e9f; };   // (drawn for squares minD..maxD from the eye)

int main(int argc, char** argv)
{
	setvbuf(stdout, nullptr, _IOLBF, 0);
	const std::string scenePath = argc > 1 ? argv[1] : "demo_scene.json";
	std::ifstream sf(scenePath); if (!sf) { fprintf(stderr, "no %s (export_erangel.py makes it)\n", scenePath.c_str()); return 1; }
	json S; sf >> S;
	SDL_Init(SDL_INIT_VIDEO);
	const int W = getenv("PYROS_W") ? atoi(getenv("PYROS_W")) : 1600, H = getenv("PYROS_H") ? atoi(getenv("PYROS_H")) : 900;
	SDL_Window* win = SDL_CreateWindow("The renderer - Erangel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_METAL | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_MetalView view = SDL_Metal_CreateView(win);
	int pw, ph; SDL_Metal_GetDrawableSize(win, &pw, &ph);
	SurfaceSource ss; ss.kind = SurfaceSource::MetalLayer; ss.a = SDL_Metal_GetLayer(view); ss.width = pw; ss.height = ph; ss.vsync = true;
	Device* dev = CreateWebGPUDevice(ss);
	if (!dev) return 1;
	printf("%s, %dx%d\n", dev->Caps().name.c_str(), pw, ph);
	auto t0 = std::chrono::steady_clock::now();

	SamplerDesc rs; rs.anisotropy = 8.f; Sampler repeat = dev->CreateSampler(rs);
	SamplerDesc cs; cs.u = cs.v = Address::Clamp; Sampler clampS = dev->CreateSampler(cs);
	auto shader = [&](const char* body, const char* name) { std::string src = std::string(kCommon) + body; ShaderDesc d; d.wgsl = strdup(src.c_str()); d.debugName = name; return dev->CreateShader(d); };
	Shader skyS = shader(kSky, "sky"), terrS = shader(kTerrain, "terrain"), meshS = shader(kMesh, "mesh"), toneS = shader(kTonemap, "tonemap");
	Shader shTerrS = shader(kShadowTerrain, "shadow terrain"), shMeshS = shader(kShadowMesh, "shadow mesh");
	// ---- the sun's shadow maps: three cascades
	const uint32_t SM = 2048; Texture smap[3];
	for (int c = 0; c < 3; c++) { TextureDesc sd; sd.width = sd.height = SM; sd.format = Format::D32F; sd.usage = TexDepthTarget | TexSampled; smap[c] = dev->CreateTexture(sd); }
	SamplerDesc cmpd; cmpd.compare = true; cmpd.u = cmpd.v = Address::Clamp; cmpd.anisotropy = 1.f; Sampler cmpS = dev->CreateSampler(cmpd);
	struct ShadowData { M4 m[3]; float splits[4], texel[4]; };
	BufferDesc sub; sub.size = sizeof(ShadowData); sub.usage = BufUniform; Buffer shadowUB = dev->CreateBuffer(sub, nullptr);

	// ---- targets
	TextureDesc hd; hd.width = pw; hd.height = ph; hd.format = Format::RGBA16F; hd.usage = TexColorTarget | TexSampled; Texture hdr = dev->CreateTexture(hd);
	TextureDesc dd; dd.width = pw; dd.height = ph; dd.format = Format::D32F; dd.usage = TexDepthTarget; Texture depth = dev->CreateTexture(dd);

	// ---- pipelines
	auto base = [&](GraphicsPipelineDesc &p, Shader s) { p.vertex = s; p.fragment = s; p.drawDataBytes = sizeof(FrameData); p.colorCount = 1; p.colorFormats[0] = Format::RGBA16F; p.depthFormat = Format::D32F; p.depthCompare = Compare::Greater; };
	GraphicsPipelineDesc skyP; base(skyP, skyS); skyP.depthTest = false; skyP.depthWrite = false; skyP.cull = Cull::None;
	Pipeline skyPipe = dev->CreatePipeline(skyP);
	GraphicsPipelineDesc terP; base(terP, terrS);
	terP.material = { { 0, BindingKind::Texture2D, false, true }, { 1, BindingKind::Texture2D, false, true }, { 2, BindingKind::Texture2D, false, true }, { 3, BindingKind::Texture2D, false, true },
		{ 4, BindingKind::Texture2D, false, true }, { 5, BindingKind::Sampler, false, true }, { 6, BindingKind::Sampler, false, true },
		{ 7, BindingKind::Uniform, false, true }, { 8, BindingKind::DepthTexture, false, true }, { 9, BindingKind::DepthTexture, false, true }, { 10, BindingKind::DepthTexture, false, true }, { 11, BindingKind::ComparisonSampler, false, true } };
	terP.bindings = { { 0, sizeof(Vertex), false } };
	terP.attributes = { { 0, 0, Format::Unknown, 0 }, { 1, 0, Format::Unknown, 12 }, { 2, 0, Format::RG32F, 24 } };
	terP.frontCounterClockwise = true; terP.cull = Cull::None;
	Pipeline terPipe = dev->CreatePipeline(terP);
	GraphicsPipelineDesc meshP; base(meshP, meshS);
	meshP.material = { { 0, BindingKind::Texture2D, false, true }, { 1, BindingKind::Sampler, false, true }, { 2, BindingKind::Uniform, false, true },
		{ 3, BindingKind::Uniform, false, true }, { 4, BindingKind::DepthTexture, false, true }, { 5, BindingKind::DepthTexture, false, true }, { 6, BindingKind::DepthTexture, false, true }, { 7, BindingKind::ComparisonSampler, false, true } };
	meshP.bindings = { { 0, sizeof(Vertex), false }, { 1, 64, true } };
	meshP.attributes = { { 0, 0, Format::Unknown, 0 }, { 1, 0, Format::Unknown, 12 }, { 2, 0, Format::RG32F, 24 },
		{ 3, 1, Format::RGBA32F, 0 }, { 4, 1, Format::RGBA32F, 16 }, { 5, 1, Format::RGBA32F, 32 }, { 6, 1, Format::RGBA32F, 48 } };
	meshP.cull = Cull::Back;     // (cards and tufts are there twice, a side each: drawn both, they fought, and the back one was lit as the ground)
	Pipeline meshPipe = dev->CreatePipeline(meshP);
	// shadow pipelines: depth only, the light's orthographic view in F.viewProj; slope bias against acne
	GraphicsPipelineDesc shTerP = terP; shTerP.vertex = shTerrS; shTerP.fragment = Shader(); shTerP.material.clear(); shTerP.colorCount = 0; shTerP.depthBiasSlope = -1.5f; shTerP.cull = Cull::None;
	Pipeline shTerPipe = dev->CreatePipeline(shTerP);
	GraphicsPipelineDesc shMeshP = meshP; shMeshP.vertex = shMeshS; shMeshP.fragment = shMeshS; shMeshP.colorCount = 0; shMeshP.depthBiasSlope = -1.5f;   // (reversed Z: away from the sun is smaller) shMeshP.cull = Cull::None;
	shMeshP.material = { { 0, BindingKind::Texture2D, false, true }, { 1, BindingKind::Sampler, false, true }, { 2, BindingKind::Uniform, false, true } };
	Pipeline shMeshPipe = dev->CreatePipeline(shMeshP);
	GraphicsPipelineDesc toneP; toneP.vertex = toneS; toneP.fragment = toneS; toneP.drawDataBytes = sizeof(FrameData); toneP.colorCount = 1; toneP.colorFormats[0] = Format::Unknown; toneP.cull = Cull::None;
	toneP.material = { { 0, BindingKind::Texture2D, false, true }, { 1, BindingKind::Sampler, false, true } };
	Pipeline tonePipe = dev->CreatePipeline(toneP);
	while (!dev->PipelineReady(skyPipe) || !dev->PipelineReady(terPipe) || !dev->PipelineReady(meshPipe) || !dev->PipelineReady(tonePipe) || !dev->PipelineReady(shTerPipe) || !dev->PipelineReady(shMeshPipe)) dev->BeginFrame();

	// ---- terrain: the tiles' heightmaps and splat maps, stitched into one grid a metre apart
	const json &T = S["terrain"];
	const int TX = T["tilesX"], TZ = T["tilesZ"]; const float tile = T["tileSize"], hs = T["heightScale"], ho = T["heightOffset"];
	const std::string tdir = T["dir"];
	const int per = 128, G = TX * per + 1;      // (129 samples a tile, the edges shared)
	std::vector<float> height((size_t)G * G, 0.f); Image splat; splat.w = splat.h = G; splat.px.assign((size_t)G * G * 4, 0);
	for (int tz = 0; tz < TZ; tz++) for (int tx = 0; tx < TX; tx++)
	{
		const std::string stem = tdir + "/" + std::to_string(tx) + "_" + std::to_string(tz);
		int w, h, n; unsigned short* hp = stbi_load_16((stem + ".png").c_str(), &w, &h, &n, 1);
		Image sp; LoadImage(stem + "_splat.png", sp);
		for (int y = 0; y <= per; y++) for (int x = 0; x <= per; x++)
		{
			const int gx = tx * per + x, gz = tz * per + y;
			if (hp) height[(size_t)gz * G + gx] = ho + hs * hp[std::min(h - 1, y) * w + std::min(w - 1, x)] / 65535.f;
			if (!sp.px.empty()) memcpy(&splat.px[((size_t)gz * G + gx) * 4], &sp.px[((size_t)std::min(sp.h - 1, y) * sp.w + std::min(sp.w - 1, x)) * 4], 4);
		}
		if (hp) stbi_image_free(hp);
	}
	const float half = TX * tile * 0.5f, step = tile / per;
	auto heightAt = [&](float x, float z) {
		const float gx = std::max(0.f, std::min((float)G - 1.001f, (x + half) / step)), gz = std::max(0.f, std::min((float)G - 1.001f, (z + half) / step));
		const int ix = (int)gx, iz = (int)gz; const float fx = gx - ix, fz = gz - iz;
		const float a = height[(size_t)iz * G + ix], b = height[(size_t)iz * G + ix + 1], c = height[(size_t)(iz + 1) * G + ix], d = height[(size_t)(iz + 1) * G + ix + 1];
		return (a * (1 - fx) + b * fx) * (1 - fz) + (c * (1 - fx) + d * fx) * fz;
	};
	std::vector<Vertex> tv((size_t)G * G); std::vector<uint32_t> ti;
	for (int z = 0; z < G; z++) for (int x = 0; x < G; x++)
	{
		Vertex &v = tv[(size_t)z * G + x];
		v.p[0] = -half + x * step; v.p[2] = -half + z * step; v.p[1] = height[(size_t)z * G + x];
		const float hl = height[(size_t)z * G + std::max(0, x - 1)], hr = height[(size_t)z * G + std::min(G - 1, x + 1)];
		const float hd_ = height[(size_t)std::max(0, z - 1) * G + x], hu = height[(size_t)std::min(G - 1, z + 1) * G + x];
		V3 n = Norm({ hl - hr, 2.f * step, hd_ - hu }); v.n[0] = n.x; v.n[1] = n.y; v.n[2] = n.z;
		v.uv[0] = (x + 0.5f) / G; v.uv[1] = (z + 0.5f) / G;
	}
	// patches of 32 x 32 samples, each at 1, 2, 4 and 8 m; a skirt hangs from every edge (a copy of the grid
	// 2 m lower) so neighbours at different detail show no gap
	const uint32_t skirt = (uint32_t)tv.size();
	for (size_t k = 0; k < (size_t)G * G; k++) { Vertex v = tv[k]; v.p[1] -= 2.f; tv.push_back(v); }
	const int PS = 32, PN = (G - 1) / PS;
	struct Patch { float cx, cz; uint32_t first[4], count[4]; };
	std::vector<Patch> patches;
	for (int pz = 0; pz < PN; pz++) for (int px = 0; px < PN; px++)
	{
		Patch pa; pa.cx = -half + (px + 0.5f) * PS * step; pa.cz = -half + (pz + 0.5f) * PS * step;
		for (int l = 0; l < 4; l++)
		{
			const int st = 1 << l; pa.first[l] = (uint32_t)ti.size();
			auto quad = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) { ti.push_back(a); ti.push_back(c); ti.push_back(b); ti.push_back(b); ti.push_back(c); ti.push_back(d); };
			for (int z = 0; z < PS; z += st) for (int x = 0; x < PS; x += st)
			{
				const uint32_t a = (pz * PS + z) * G + px * PS + x;
				quad(a, a + st, a + st * G, a + st * G + st);
			}
			for (int k = 0; k < PS; k += st)
			{
				const uint32_t top = (pz * PS) * G + px * PS + k, bot = (pz * PS + PS) * G + px * PS + k;
				const uint32_t lef = (pz * PS + k) * G + px * PS, rig = (pz * PS + k) * G + px * PS + PS;
				quad(top + skirt, top + st + skirt, top, top + st);
				quad(bot, bot + st, bot + skirt, bot + st + skirt);
				quad(lef, lef + st * G, lef + skirt, lef + st * G + skirt);
				quad(rig + skirt, rig + st * G + skirt, rig, rig + st * G);
			}
			pa.count[l] = (uint32_t)ti.size() - pa.first[l];
		}
		patches.push_back(pa);
	}
	BufferDesc tvb; tvb.size = tv.size() * sizeof(Vertex); tvb.usage = BufVertex; Buffer terVB = dev->CreateBuffer(tvb, tv.data());
	BufferDesc tib; tib.size = ti.size() * 4; tib.usage = BufIndex; Buffer terIB = dev->CreateBuffer(tib, ti.data());
	Texture splatT = MakeTexture(dev, splat, false, false);
	Texture layers[4];
	for (int i = 0; i < 4; i++) { Image im; LoadImage(T["layers"][i], im); layers[i] = MakeTexture(dev, im, true, false); }
	BindGroup terGroup;
	{
		Device::BindGroupEntry e[12]; for (int i = 0; i < 12; i++) e[i].binding = i;
		e[0].texture = splatT; for (int i = 0; i < 4; i++) e[1 + i].texture = layers[i]; e[5].sampler = repeat; e[6].sampler = clampS;
		e[7].buffer = shadowUB; for (int c = 0; c < 3; c++) e[8 + c].texture = smap[c]; e[11].sampler = cmpS;
		terGroup = dev->CreateBindGroup(terPipe, e, 12);
	}

	// ---- trees: one instanced draw a part of a kind (trunk, boughs, cards), at Island's places
	std::map<std::string, std::vector<M4> > placed;
	for (const json &t : S["trees"])
	{
		const float x = t["pos"][0], z = t["pos"][2], yaw = t["yaw"], sc = t["scale"];
		const float y = heightAt(x, z) - 0.15f;
		const float c = cosf(yaw) * sc, s = sinf(yaw) * sc;
		M4 m = { { c, 0, -s, 0, 0, sc, 0, 0, s, 0, c, 0, x, y, z, 1 } };
		placed[t["name"]].push_back(m);
	}
	// (each kind's trees sorted by 32 m square, so a pass draws only the squares it can see)
	std::map<std::string, std::vector<Cell> > placedCells;
	for (auto &kv : placed)
	{
		std::map<std::pair<int, int>, std::vector<M4> > byCell;
		for (const M4 &m : kv.second) byCell[std::make_pair((int)floorf(m.m[12] / 32.f), (int)floorf(m.m[14] / 32.f))].push_back(m);
		kv.second.clear();
		for (auto &c : byCell)
		{
			Cell cell = { (c.first.first + 0.5f) * 32.f, (c.first.second + 0.5f) * 32.f, (uint32_t)kv.second.size(), (uint32_t)c.second.size() };
			kv.second.insert(kv.second.end(), c.second.begin(), c.second.end());
			placedCells[kv.first].push_back(cell);
		}
	}
	std::vector<Part> parts;
	std::map<std::string, Texture> texCache;
	auto texture = [&](const std::string &path, bool cutout) {
		std::map<std::string, Texture>::iterator it = texCache.find(path);
		if (it != texCache.end()) return it->second;
		Image im; Texture t;
		if (path.empty()) { im.w = im.h = 4; im.px.resize(64); for (int k = 0; k < 16; k++) { im.px[k * 4] = 88; im.px[k * 4 + 1] = 70; im.px[k * 4 + 2] = 54; im.px[k * 4 + 3] = 255; } t = MakeTexture(dev, im, true, false); }
		else if (LoadImage(path, im)) t = MakeTexture(dev, im, true, cutout);
		texCache[path] = t; return t;
	};
	struct Mats { BindGroup lit, shadow; };
	auto material2 = [&](Texture t, const MatData &md) {
		BufferDesc ub; ub.size = sizeof(MatData); ub.usage = BufUniform; Buffer b = dev->CreateBuffer(ub, &md);
		Device::BindGroupEntry e[8]; for (int i = 0; i < 8; i++) e[i].binding = i;
		e[0].texture = t; e[1].sampler = repeat; e[2].buffer = b; e[3].buffer = shadowUB; for (int c = 0; c < 3; c++) e[4 + c].texture = smap[c]; e[7].sampler = cmpS;
		Mats m; m.lit = dev->CreateBindGroup(meshPipe, e, 8); m.shadow = dev->CreateBindGroup(shMeshPipe, e, 3);
		return m;
	};
	const std::string tc = S["treeCache"], ta = S["treeAssets"];
	// the scanned trees whole (branches, twigs, leaves: tools/renderer_demo/export_ph_full.py) near the eye,
	// the cards past kFullTo
	const float kFullTo = getenv("PYROS_FULL_TO") ? (float)atof(getenv("PYROS_FULL_TO")) : 70.f;
	const std::string fullDir = std::string(S["erangel"]) + "/tools/cache/ph_full";
	auto fullOf = [](const std::string &n) -> std::pair<std::string, std::string> {
		const char v = n.empty() ? 'a' : n.back();
		if (n.compare(0, 4, "fir_") == 0) return std::make_pair(std::string("fir_tree_01"), std::string("fir_tree_01_") + v + "_LOD1");
		if (n.compare(0, 5, "pine_") == 0) return std::make_pair(std::string("pine_tree_01"), std::string("pine_tree_01_") + v + "_LOD2");
		if (n == "broadleaf_a") return std::make_pair(std::string("tree_small_02"), std::string("tree_small_02_LOD1"));
		return std::make_pair(std::string(), std::string());
	};
	for (auto &kv : placed)
	{
		const std::string &name = kv.first;
		const std::pair<std::string, std::string> full = fullOf(name);
		const size_t firstPart = parts.size();
		for (const char* partName : { "trunk", "boughs", "cards" })
		{
			if (getenv("PYROS_NO_BOUGHS") && !strcmp(partName, "boughs")) continue;
			Part p; if (!LoadObj(dev, tc + "/" + name + "/" + name + "_" + partName + ".obj", p.mesh)) continue;
			const bool cards = !strcmp(partName, "cards");
			std::string tex = ta + "/" + name + "/textures/" + name + (cards ? "_cards.png" : !strcmp(partName, "trunk") ? "_trunk.jpg" : "_bark.jpg");
			// (boughs with no bark picture of their own - the broadleaf's, the bushes' - take the trunk's, or a
			// plain bark brown)
			if (!cards && !std::ifstream(tex)) tex = ta + "/" + name + "/textures/" + name + "_trunk.jpg";
			if (!cards && !std::ifstream(tex)) tex = "";
			const bool conifer = name.find("pine") != std::string::npos || name.find("fir") != std::string::npos;
			MatData md = { { 1, 1, 1, 1 }, { cards ? 0.4f : 0.f, cards ? 1.f : 0.f, cards ? 0.7f : 0.85f, 0 } };
			if (cards && conifer) { md.color[0] = 0.86f; md.color[1] = 1.1f; md.color[2] = 0.82f; }
			{ auto mm = material2(texture(tex, cards), md); p.group = mm.lit; p.shadowGroup = mm.shadow; }
			BufferDesc ib; ib.size = kv.second.size() * sizeof(M4); ib.usage = BufVertex; p.instances = dev->CreateBuffer(ib, kv.second.data());
			p.count = (uint32_t)kv.second.size(); p.cells = placedCells[name];
			parts.push_back(p);
		}
		if (full.first.empty() || getenv("PYROS_NO_FULL")) continue;
		for (size_t i = firstPart; i < parts.size(); i++) parts[i].minD = kFullTo;
		Buffer shared = parts.size() > firstPart ? parts[firstPart].instances : Buffer();
		for (const char* mat : { "twig", "leaves", "bark", "branches", "dead_branches", "trunk", "trunk_a", "trunk_b", "trunk_c" })
		{
			const std::string obj = fullDir + "/" + full.first + "/" + full.second + "__" + full.first + "_" + mat + ".obj";
			if (!std::ifstream(obj)) continue;
			Part p; if (!LoadObj(dev, obj, p.mesh)) continue;
			const bool leaf = !strcmp(mat, "twig") || !strcmp(mat, "leaves");
			MatData md = { { 1, 1, 1, 1 }, { leaf ? 0.45f : 0.f, leaf ? 1.f : 0.f, leaf ? 0.65f : 0.9f, 0 } };
			{ auto mm = material2(texture(fullDir + "/" + full.first + "/tex/" + full.first + "_" + mat + ".png", leaf), md); p.group = mm.lit; p.shadowGroup = mm.shadow; }
			p.instances = shared; p.count = (uint32_t)kv.second.size(); p.cells = placedCells[name]; p.maxD = kFullTo;
			parts.push_back(p);
		}
	}

	// ---- grass: tuft groups in 32 m chunks, the near ones drawn
	struct Chunk { float cx, cz; Buffer inst[2]; uint32_t n[2] = { 0, 0 }; };
	const int CH = (int)(TX * tile / 32.f);
	std::vector<std::vector<M4> > chunkInst((size_t)CH * CH * 2);
	for (const json &g : S["grass"])
	{
		const float x = g[0], z = g[1], a = g[2], sc = g[3]; const int kind = g[4];
		const int cx = std::max(0, std::min(CH - 1, (int)((x + half) / 32.f))), cz = std::max(0, std::min(CH - 1, (int)((z + half) / 32.f)));
		const float c = cosf(a) * sc, s = sinf(a) * sc;
		M4 m = { { c, 0, -s, 0, 0, sc, 0, 0, s, 0, c, 0, x, heightAt(x, z) - 0.03f, z, 1 } };
		chunkInst[((size_t)cz * CH + cx) * 2 + kind].push_back(m);
	}
	std::vector<Chunk> chunks;
	for (int cz = 0; cz < CH; cz++) for (int cx = 0; cx < CH; cx++)
	{
		Chunk c; c.cx = -half + (cx + 0.5f) * 32.f; c.cz = -half + (cz + 0.5f) * 32.f;
		for (int k = 0; k < 2; k++)
		{
			std::vector<M4> &v = chunkInst[((size_t)cz * CH + cx) * 2 + k];
			if (v.empty()) continue;
			BufferDesc ib; ib.size = v.size() * sizeof(M4); ib.usage = BufVertex; c.inst[k] = dev->CreateBuffer(ib, v.data()); c.n[k] = (uint32_t)v.size();
		}
		chunks.push_back(c);
	}
	Mesh grassMesh[2]; BindGroup grassGroup[2], grassShadow[2];
	const std::string gc = S["grassCache"], ga = S["grassAssets"];
	LoadObj(dev, gc + "/lush/lush_group_0.obj", grassMesh[0]); LoadObj(dev, gc + "/dry/dry_group_0.obj", grassMesh[1]);
	{
		MatData lush = { { 1.05f, 1.08f, 1.0f, 1 }, { 0.35f, 0.6f, 0.9f, 0 } }, dry = { { 0.86f, 0.9f, 0.78f, 1 }, { 0.35f, 0.6f, 0.9f, 0 } };
		auto g0 = material2(texture(ga + "/textures/grass_lush.png", true), lush); grassGroup[0] = g0.lit; grassShadow[0] = g0.shadow;
		auto g1 = material2(texture(ga + "/textures/grass_dry.png", true), dry); grassGroup[1] = g1.lit; grassShadow[1] = g1.shadow;
	}
	BindGroup toneGroup; { Device::BindGroupEntry e[2]; e[0].binding = 0; e[0].texture = hdr; e[1].binding = 1; e[1].sampler = clampS; toneGroup = dev->CreateBindGroup(tonePipe, e, 2); }
	printf("loaded in %.1f s: %zu tree parts, %zu grass chunks\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), parts.size(), chunks.size());

	// ---- camera
	float cam[5] = { 0.f, 0.f, 0.f, 45.f, -3.f };
	if (getenv("PYROS_CAM")) sscanf(getenv("PYROS_CAM"), "%f %f %f %f %f", &cam[0], &cam[1], &cam[2], &cam[3], &cam[4]);
	V3 eye = { cam[0], cam[1], cam[2] }; float yaw = cam[3] * 3.14159265f / 180.f, pitch = cam[4] * 3.14159265f / 180.f;
	if (eye.y == 0.f) eye.y = heightAt(eye.x, eye.z) + 1.7f;
	const double seconds = getenv("PYROS_SECONDS") ? atof(getenv("PYROS_SECONDS")) : 1e9;
	FILE* prof = getenv("PYROS_PROFILE_LOG") ? fopen(getenv("PYROS_PROFILE_LOG"), "w") : nullptr;
	auto start = std::chrono::steady_clock::now(), last = start;
	bool quit = false, drag = false; int frames = 0;
	while (!quit)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT) quit = true;
			if (e.type == SDL_MOUSEBUTTONDOWN) drag = true;
			if (e.type == SDL_MOUSEBUTTONUP) drag = false;
			if (e.type == SDL_MOUSEMOTION && drag) { yaw += e.motion.xrel * 0.004f; pitch = std::max(-1.5f, std::min(1.5f, pitch - e.motion.yrel * 0.004f)); }
		}
		auto now = std::chrono::steady_clock::now();
		const double t = std::chrono::duration<double>(now - start).count();
		const float dt = (float)std::chrono::duration<double>(now - last).count(); last = now;
		if (t > seconds) break;
		V3 fwd = { sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch) };
		const Uint8* k = SDL_GetKeyboardState(nullptr);
		const float speed = (k[SDL_SCANCODE_LSHIFT] ? 30.f : 6.f) * dt;
		V3 right = Norm(Cross(fwd, { 0, 1, 0 }));
		if (k[SDL_SCANCODE_W]) eye = eye + fwd * speed;
		if (k[SDL_SCANCODE_S]) eye = eye - fwd * speed;
		if (k[SDL_SCANCODE_D]) eye = eye + right * speed;
		if (k[SDL_SCANCODE_A]) eye = eye - right * speed;
		eye.y = std::max(eye.y, heightAt(eye.x, eye.z) + 1.2f);

		// ---- the frame's numbers: a late morning sun
		const float fovy = 60.f * 3.14159265f / 180.f, aspect = (float)pw / ph;
		FrameData fd = {};
		fd.viewProj = Mul(Perspective(fovy, aspect, 0.1f), View(eye, fwd));
		V3 up = Cross(right, fwd);
		fd.camPos[0] = eye.x; fd.camPos[1] = eye.y; fd.camPos[2] = eye.z; fd.camPos[3] = (float)t;
		V3 sun = Norm({ 0.45f, 0.62f, -0.35f });
		fd.sunDir[0] = sun.x; fd.sunDir[1] = sun.y; fd.sunDir[2] = sun.z; fd.sunDir[3] = 3.2f;
		fd.sunColor[0] = 1.0f; fd.sunColor[1] = 0.94f; fd.sunColor[2] = 0.84f;
		fd.skyTop[0] = 0.32f; fd.skyTop[1] = 0.52f; fd.skyTop[2] = 0.92f; fd.skyTop[3] = 1.0f;          // w: exposure
		fd.skyHorizon[0] = 0.72f; fd.skyHorizon[1] = 0.80f; fd.skyHorizon[2] = 0.90f; fd.skyHorizon[3] = 0.0016f;   // w: haze
		fd.camRight[0] = right.x; fd.camRight[1] = right.y; fd.camRight[2] = right.z; fd.camRight[3] = tanf(fovy / 2) * aspect;
		fd.camUp[0] = up.x; fd.camUp[1] = up.y; fd.camUp[2] = up.z; fd.camUp[3] = tanf(fovy / 2);
		fd.camFwd[0] = fwd.x; fd.camFwd[1] = fwd.y; fd.camFwd[2] = fwd.z;

		// ---- the cascades: each a box round its slice of the view, turned to the sun, moved in whole texels
		// (so it does not swim as the eye moves)
		const float splits[4] = { 0.1f, 12.f, 40.f, 120.f };
		static ShadowData sd = {};
		static FrameData cascadeFd[3]; static V3 cascCentre[3]; static float cascRadius[3];
		bool cascadeNow[3] = { true, (frames & 1) == 0 || frames < 2, (frames & 1) == 1 || frames < 2 };
		{
			V3 ls = Norm({ fd.sunDir[0], fd.sunDir[1], fd.sunDir[2] });
			V3 lf = ls * -1.f;
			for (int c = 0; c < 3; c++)
			{
				if (!cascadeNow[c]) { cascadeFd[c].camPos[3] = fd.camPos[3]; continue; }
				const float n0 = splits[c], f0 = splits[c + 1];
				const float th = tanf(fovy / 2), tw = th * aspect;
				// the slice's bounding sphere
				const float mid = (n0 + f0) * 0.5f;
				V3 centre = eye + fwd * mid;
				const float rx = tw * f0, ry = th * f0;
				const float radius = sqrtf((f0 - mid) * (f0 - mid) + rx * rx + ry * ry) * (c ? 1.1f : 1.f);   // (the margin a frame's movement needs)
				const float texelW = radius * 2.f / SM;
				cascCentre[c] = centre; cascRadius[c] = radius;
				M4 lv = View({ 0, 0, 0 }, lf);
				// the centre in light space, snapped to texels
				float lx = lv.m[0] * centre.x + lv.m[4] * centre.y + lv.m[8] * centre.z;
				float ly = lv.m[1] * centre.x + lv.m[5] * centre.y + lv.m[9] * centre.z;
				float lz = lv.m[2] * centre.x + lv.m[6] * centre.y + lv.m[10] * centre.z;
				lx = floorf(lx / texelW) * texelW; ly = floorf(ly / texelW) * texelW;
				// orthographic, reversed Z: the near end (toward the sun, 200 m back) is 1, the far 0
				const float depthRange = 400.f;
				M4 o = {};
				o.m[0] = 1.f / radius; o.m[5] = 1.f / radius; o.m[10] = 1.f / depthRange; o.m[15] = 1.f;
				// depth = (z_view - (lz - 200)) / 400: 1 at 200 m toward the sun from the slice, 0 at 200 m past it
				o.m[12] = -lx / radius; o.m[13] = -ly / radius; o.m[14] = 0.5f - lz / depthRange;
				M4 vp = Mul(o, lv);
				sd.m[c] = vp;
				cascadeFd[c] = fd; cascadeFd[c].viewProj = vp;
			}
			sd.splits[0] = splits[1]; sd.splits[1] = splits[2]; sd.splits[2] = splits[3]; sd.texel[0] = 1.f / SM;
		}
		dev->Upload(shadowUB, 0, &sd, sizeof(sd));

		dev->BeginFrame();
		Texture back = dev->AcquireBackbuffer();
		if (!back) continue;
		auto r0 = std::chrono::steady_clock::now();
		CommandList* cl = dev->NewCommandList("frame");
		const int repeat = getenv("PYROS_REPEAT") ? atoi(getenv("PYROS_REPEAT")) : 1;   // (throughput test: the scene drawn n times a frame)
		for (int rep = 0; rep < repeat; rep++) {
		for (int c = 0; c < (getenv("PYROS_NO_SHADOWS") ? 0 : 3); c++)
		{
			if (!cascadeNow[c]) continue;
			PassDesc sp; sp.depth.texture = smap[c]; sp.depth.clear = 0.f; sp.debugName = c == 0 ? "Shadow0" : c == 1 ? "Shadow1" : "Shadow2";
			cl->BeginPass(sp);
			cl->SetPipeline(shTerPipe); cl->DrawData(&cascadeFd[c], sizeof(FrameData));
			cl->SetVertexBuffer(0, terVB, 0); cl->SetIndexBuffer(terIB, 0, true);
			for (const Patch &pa : patches)
			{
				const float dx = pa.cx - cascCentre[c].x, dz = pa.cz - cascCentre[c].z, r = cascRadius[c] + 30.f;
				if (dx * dx + dz * dz > r * r) continue;
				const int l = c == 0 ? 1 : c == 1 ? 2 : 3;
				cl->DrawIndexed(pa.count[l], 1, pa.first[l], 0, 0);
			}
			cl->SetPipeline(shMeshPipe); cl->DrawData(&cascadeFd[c], sizeof(FrameData));
			for (Part &p : parts)
			{
				bool bound = false;
				for (const Cell &cell : p.cells)
				{
					// (the square's trees reach 25 m from its middle; the sun's slant carries their shadow in)
					const float dx = cell.cx - cascCentre[c].x, dz = cell.cz - cascCentre[c].z, r = cascRadius[c] + 50.f;
					if (dx * dx + dz * dz > r * r) continue;
					const float ex = cell.cx - eye.x, ez = cell.cz - eye.z, ed = sqrtf(ex * ex + ez * ez);
					if (ed < p.minD || ed >= p.maxD) continue;
					if (!bound) { cl->SetBindGroup(p.shadowGroup); cl->SetVertexBuffer(0, p.mesh.vb, 0); cl->SetVertexBuffer(1, p.instances, 0); cl->SetIndexBuffer(p.mesh.ib, 0, true); bound = true; }
					cl->DrawIndexed(p.mesh.count, cell.count, 0, 0, cell.first);
				}
			}
			if (c < 1)       // (grass casts in the near cascade only)
				for (int kind = 0; kind < 2; kind++)
				{
					cl->SetBindGroup(grassShadow[kind]);
					cl->SetVertexBuffer(0, grassMesh[kind].vb, 0); cl->SetIndexBuffer(grassMesh[kind].ib, 0, true);
					for (Chunk &ch : chunks)
					{
						if (!ch.n[kind]) continue;
						const float dx = ch.cx - eye.x, dz = ch.cz - eye.z;
						if (dx * dx + dz * dz > (splits[c + 1] + 30.f) * (splits[c + 1] + 30.f)) continue;
						cl->SetVertexBuffer(1, ch.inst[kind], 0);
						cl->DrawIndexed(grassMesh[kind].count, ch.n[kind], 0, 0, 0);
					}
				}
			cl->EndPass();
		}
		PassDesc scene; scene.colorCount = 1; scene.colors[0].texture = hdr; scene.colors[0].load = Load::DontCare;
		scene.depth.texture = depth; scene.depth.clear = 0.f; scene.debugName = "Scene";
		cl->BeginPass(scene);
		cl->SetPipeline(skyPipe); cl->DrawData(&fd, sizeof(fd)); cl->Draw(3, 1, 0, 0);
		cl->SetPipeline(terPipe); cl->DrawData(&fd, sizeof(fd)); cl->SetBindGroup(terGroup);
		cl->SetVertexBuffer(0, terVB, 0); cl->SetIndexBuffer(terIB, 0, true);
		for (const Patch &pa : patches)
		{
			const float dx = pa.cx - eye.x, dz = pa.cz - eye.z, d = sqrtf(dx * dx + dz * dz);
			if (d > 50.f && (dx * fwd.x + dz * fwd.z) / (d * std::max(1e-3f, sqrtf(fwd.x * fwd.x + fwd.z * fwd.z))) < cosf(fovy * 0.5f * aspect + 0.5f)) continue;
			const int l = d < 60.f ? 0 : d < 130.f ? 1 : d < 260.f ? 2 : 3;
			cl->DrawIndexed(pa.count[l], 1, pa.first[l], 0, 0);
		}
		cl->SetPipeline(meshPipe); cl->DrawData(&fd, sizeof(fd));
		for (Part &p : parts)
		{
			bool bound = false;
			for (const Cell &cell : p.cells)
			{
				// (squares behind the eye skipped: everything past the fov's half-angle plus the square's reach)
				const float dx = cell.cx - eye.x, dz = cell.cz - eye.z, d = sqrtf(dx * dx + dz * dz);
				if (d < p.minD || d >= p.maxD) continue;
				if (d > 40.f && (dx * fwd.x + dz * fwd.z) / (d * std::max(1e-3f, sqrtf(fwd.x * fwd.x + fwd.z * fwd.z))) < cosf(fovy * 0.5f * aspect + 0.6f)) continue;
				if (!bound) { cl->SetBindGroup(p.group); cl->SetVertexBuffer(0, p.mesh.vb, 0); cl->SetVertexBuffer(1, p.instances, 0); cl->SetIndexBuffer(p.mesh.ib, 0, true); bound = true; }
				cl->DrawIndexed(p.mesh.count, cell.count, 0, 0, cell.first);
			}
		}
		for (int kind = 0; kind < (getenv("PYROS_NO_GRASS") ? 0 : 2); kind++)
		{
			cl->SetBindGroup(grassGroup[kind]);
			cl->SetVertexBuffer(0, grassMesh[kind].vb, 0); cl->SetIndexBuffer(grassMesh[kind].ib, 0, true);
			for (Chunk &c : chunks)
			{
				if (!c.n[kind]) continue;
				const float dx = c.cx - eye.x, dz = c.cz - eye.z;
				const float d = sqrtf(dx * dx + dz * dz);
				if (d > 90.f) continue;
				// (thinned with distance: a chunk's tufts are in no order, so its first ones are an even spread)
				const float keep = std::max(0.f, std::min(1.f, 1.f - (d - 30.f) / 75.f));
				const uint32_t n = (uint32_t)(c.n[kind] * keep);
				if (!n) continue;
				cl->SetVertexBuffer(1, c.inst[kind], 0);
				cl->DrawIndexed(grassMesh[kind].count, n, 0, 0, 0);
			}
		}
		cl->EndPass();
		}
		PassDesc out; out.colorCount = 1; out.colors[0].texture = back; out.colors[0].load = Load::DontCare; out.debugName = "Tonemap";
		cl->BeginPass(out);
		cl->SetPipeline(tonePipe); cl->DrawData(&fd, sizeof(fd)); cl->SetBindGroup(toneGroup); cl->Draw(3, 1, 0, 0);
		cl->EndPass();
		dev->Submit(&cl, 1);
		const double rec = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - r0).count();
		dev->Present();
		frames++;
		const FrameStats &st = dev->LastFrameStats();
		if (prof)
		{
			fprintf(prof, "frame=%d t=%.2f ms=%.3f CPU.RecordSubmit=%.3f GPU.Total=%.3f", frames, t, dt * 1000.0, rec, st.gpuMs);
			for (const FrameStats::Pass &p : st.passes) fprintf(prof, " GPU.%s=%.3f", p.name.c_str(), p.gpuMs);
			fprintf(prof, " #Draws=%u\n", st.draws);
		}
	}
	if (prof) fclose(prof);
	delete dev;
	SDL_Metal_DestroyView(view); SDL_DestroyWindow(win); SDL_Quit();
	return 0;
}
