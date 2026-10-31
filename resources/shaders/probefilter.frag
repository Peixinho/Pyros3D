#version 450
// A reflection probe's six views, filtered into one level of its block in the probe atlas: every texel a direction (the
// block laid out as the old renderer's - longitude across, from the pole down), what a surface of this roughness
// reflects toward it - the views summed under its GGX lobe (Karis 2013, the split sum's first half: N = V = R)
layout(set = 0, binding = 0, std140) uniform DrawData { vec4 level; vec4 params; } uDraw;   // level: origin xy, size zw (texels); params: x roughness, y samples
layout(set = 1, binding = 1) uniform texture2D uFace0;
layout(set = 1, binding = 2) uniform texture2D uFace1;
layout(set = 1, binding = 3) uniform texture2D uFace2;
layout(set = 1, binding = 4) uniform texture2D uFace3;
layout(set = 1, binding = 5) uniform texture2D uFace4;
layout(set = 1, binding = 6) uniform texture2D uFace5;
layout(set = 1, binding = 7) uniform sampler uLinearClamp;
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// The views' ways (SceneRenderer::ProbeFaceBasis: the same): +X -X +Y -Y +Z -Z, each with its up; its right is forward x up
vec3 Forward(int i) { return i == 0 ? vec3(1, 0, 0) : i == 1 ? vec3(-1, 0, 0) : i == 2 ? vec3(0, 1, 0) : i == 3 ? vec3(0, -1, 0) : i == 4 ? vec3(0, 0, 1) : vec3(0, 0, -1); }
vec3 Up(int i) { return i == 2 ? vec3(0, 0, 1) : i == 3 ? vec3(0, 0, -1) : vec3(0, 1, 0); }

vec3 SeenToward(vec3 d)
{
	vec3 a = abs(d);
	int i = (a.x >= a.y && a.x >= a.z) ? (d.x > 0.0 ? 0 : 1) : (a.y >= a.z) ? (d.y > 0.0 ? 2 : 3) : (d.z > 0.0 ? 4 : 5);
	vec3 f = Forward(i), u = Up(i), r = cross(f, u);
	float z = dot(d, f);
	// (the picture's first row at its top: up is toward row 0)
	vec2 uv = vec2(0.5 + 0.5 * dot(d, r) / z, 0.5 - 0.5 * dot(d, u) / z);
	if (i == 0) return textureLod(sampler2D(uFace0, uLinearClamp), uv, 0.0).rgb;
	if (i == 1) return textureLod(sampler2D(uFace1, uLinearClamp), uv, 0.0).rgb;
	if (i == 2) return textureLod(sampler2D(uFace2, uLinearClamp), uv, 0.0).rgb;
	if (i == 3) return textureLod(sampler2D(uFace3, uLinearClamp), uv, 0.0).rgb;
	if (i == 4) return textureLod(sampler2D(uFace4, uLinearClamp), uv, 0.0).rgb;
	return textureLod(sampler2D(uFace5, uLinearClamp), uv, 0.0).rgb;
}

vec2 Hammersley(uint i, uint n)
{
	uint b = i;
	b = (b << 16u) | (b >> 16u);
	b = ((b & 0x55555555u) << 1u) | ((b & 0xAAAAAAAAu) >> 1u);
	b = ((b & 0x33333333u) << 2u) | ((b & 0xCCCCCCCCu) >> 2u);
	b = ((b & 0x0F0F0F0Fu) << 4u) | ((b & 0xF0F0F0F0u) >> 4u);
	b = ((b & 0x00FF00FFu) << 8u) | ((b & 0xFF00FF00u) >> 8u);
	return vec2(float(i) / float(n), float(b) * 2.3283064365386963e-10);
}

void main()
{
	// (this texel's direction: across, the longitude from -Z; down, from the pole)
	vec2 uv = (gl_FragCoord.xy - uDraw.level.xy) / uDraw.level.zw;
	float phi = (uv.x - 0.5) * 2.0 * PI, theta = uv.y * PI;
	vec3 n = vec3(sin(theta) * sin(phi), cos(theta), -sin(theta) * cos(phi));
	float roughness = uDraw.params.x;
	if (roughness <= 0.0) { outColor = vec4(SeenToward(n), 1.0); return; }
	vec3 up = abs(n.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
	vec3 tx = normalize(cross(up, n)), ty = cross(n, tx);
	float a = roughness * roughness;
	uint count = uint(uDraw.params.y);
	vec3 sum = vec3(0.0);
	float weight = 0.0;
	for (uint i = 0u; i < count; i++)
	{
		vec2 xi = Hammersley(i, count);
		float p = 2.0 * PI * xi.x;
		float cosT = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y)), sinT = sqrt(1.0 - cosT * cosT);
		vec3 h = tx * (sinT * cos(p)) + ty * (sinT * sin(p)) + n * cosT;
		vec3 l = 2.0 * dot(n, h) * h - n;
		float nl = dot(n, l);
		if (nl > 0.0) { sum += SeenToward(l) * nl; weight += nl; }
	}
	outColor = vec4(sum / max(weight, 1e-4), 1.0);
}
