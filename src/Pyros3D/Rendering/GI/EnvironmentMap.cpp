//============================================================================
// Name        : EnvironmentMap.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : An HDR panorama a scene is lit by and seen against.
//============================================================================

#include <Pyros3D/Rendering/GI/EnvironmentMap.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/stb/stb_image.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace p3d {

	namespace {
		std::shared_ptr<Texture> g_texture, g_radiance;
		f32 g_intensity = 1.f;
		std::string g_path;
		f32 g_yaw = 0.f;
		bool g_loaded = false;
		SphericalHarmonicsL2 g_sh;
		Vec3 g_sun(0.f, 1.f, 0.f);

		void MakeBlack(Texture *t)
		{
			const f32 black[4] = { 0.f, 0.f, 0.f, 1.f };
			t->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, 1, 1, false);
			t->UpdateData((void*)black);
			t->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		}
		void MakeBlack() { MakeBlack(g_texture.get()); }

		// ---------------------------------------------------------------- reflections
		// The sky blurred for six roughnesses (0, .2, ... 1), each an equirect of its own size -
		// 512x256 down to 32x16 - stacked top to bottom in one 512x512 RGBA32F. A rougher surface
		// reflects a wider cone of the sky; the blur is a gaussian of that cone's width, along the
		// image's two axes (wider across where the panorama is stretched, toward the poles). Not
		// the GGX integral, but its width and its weight, and cheap enough to make at load.
		// PyrosShader.glsl's EnvSpecular reads the same layout.
		const uint32 kAtlasW = 512, kLevels = 6;
		uint32 LevelW(uint32 atlasW, uint32 k) { return std::max<uint32>(atlasW >> k, 32u); }
		// (the sharpest level, kept on the CPU for SampleRadiance)
		std::vector<f32> g_level0;
		uint32 g_level0W = 0, g_level0H = 0;

		void Downsample(const f32 *src, uint32 sw, uint32 sh, uint32 sstride, std::vector<f32> &dst, uint32 dw, uint32 dh)
		{
			dst.assign((size_t)dw * dh * 3, 0.f);
			for (uint32 y = 0; y < dh; y++)
			{
				const uint32 y0 = y * sh / dh, y1 = std::max(y0 + 1, (y + 1) * sh / dh);
				for (uint32 x = 0; x < dw; x++)
				{
					const uint32 x0 = x * sw / dw, x1 = std::max(x0 + 1, (x + 1) * sw / dw);
					f32 r = 0.f, g = 0.f, b = 0.f; uint32 n = 0;
					for (uint32 yy = y0; yy < y1 && yy < sh; yy++)
						for (uint32 xx = x0; xx < x1 && xx < sw; xx++)
						{
							const f32 *p = src + ((size_t)yy * sw + xx) * sstride;
							r += p[0]; g += p[1]; b += p[2]; n++;
						}
					f32 *d = &dst[((size_t)y * dw + x) * 3];
					if (n) { d[0] = r / n; d[1] = g / n; d[2] = b / n; }
				}
			}
		}

		void Blur(std::vector<f32> &img, uint32 w, uint32 h, f32 sigma)
		{
			if (sigma <= 0.f) return;
			const f32 kPI = 3.14159265358979f;
			std::vector<f32> tmp(img.size());
			const f32 dPhi = 2.f * kPI / w, dTheta = kPI / h;
			for (uint32 y = 0; y < h; y++)                    // across: wraps round
			{
				const f32 st = std::max(sinf(((f32)y + 0.5f) / h * kPI), 0.02f);
				const f32 s = std::min(sigma / (dPhi * st), (f32)w * 0.5f);
				const int r = std::min((int)ceilf(s * 2.5f), (int)w / 2);
				for (uint32 x = 0; x < w; x++)
				{
					f32 acc[3] = { 0, 0, 0 }, ws = 0.f;
					for (int i = -r; i <= r; i++)
					{
						const f32 k = s > 0.f ? expf(-0.5f * (i * i) / (s * s)) : 1.f;
						const uint32 xx = (uint32)(((int)x + i) % (int)w + (int)w) % w;
						const f32 *p = &img[((size_t)y * w + xx) * 3];
						acc[0] += p[0] * k; acc[1] += p[1] * k; acc[2] += p[2] * k; ws += k;
					}
					f32 *d = &tmp[((size_t)y * w + x) * 3];
					d[0] = acc[0] / ws; d[1] = acc[1] / ws; d[2] = acc[2] / ws;
				}
			}
			const f32 s = sigma / dTheta;
			const int r = std::min((int)ceilf(s * 2.5f), (int)h);
			for (uint32 y = 0; y < h; y++)                    // down: stops at the poles
				for (uint32 x = 0; x < w; x++)
				{
					f32 acc[3] = { 0, 0, 0 }, ws = 0.f;
					for (int i = -r; i <= r; i++)
					{
						const int yy = (int)y + i;
						if (yy < 0 || yy >= (int)h) continue;
						const f32 k = expf(-0.5f * (i * i) / (s * s));
						const f32 *p = &tmp[((size_t)yy * w + x) * 3];
						acc[0] += p[0] * k; acc[1] += p[1] * k; acc[2] += p[2] * k; ws += k;
					}
					f32 *d = &img[((size_t)y * w + x) * 3];
					d[0] = acc[0] / ws; d[1] = acc[1] / ws; d[2] = acc[2] / ws;
				}
		}

		void BuildRadiance(const f32 *px, uint32 w, uint32 h)
		{
			std::vector<f32> atlas;
			BuildRadianceStack(px, w, h, 4, kAtlasW, atlas);
			g_radiance->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, kAtlasW, RadianceStackHeight(kAtlasW), false);
			g_radiance->UpdateData(atlas.data());
			g_radiance->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
			g_level0W = kAtlasW; g_level0H = kAtlasW / 2;
			g_level0.assign(atlas.begin(), atlas.begin() + (size_t)g_level0W * g_level0H * 4);
		}
	}
	uint32 RadianceStackHeight(const uint32 atlasW)
	{
		uint32 h = 0;
		for (uint32 k = 0; k < kLevels; k++) h += LevelW(atlasW, k) / 2;
		return h;
	}
	void BuildRadianceStack(const f32 *px, const uint32 w, const uint32 h, const uint32 stride, const uint32 atlasW, std::vector<f32> &atlas)
	{
		atlas.assign((size_t)atlasW * RadianceStackHeight(atlasW) * 4, 0.f);
		uint32 oy = 0;
		for (uint32 k = 0; k < kLevels; k++)
		{
			const uint32 lw = LevelW(atlasW, k), lh = lw / 2;
			std::vector<f32> lvl;
			Downsample(px, w, h, stride, lvl, lw, lh);
			const f32 rough = (f32)k / (f32)(kLevels - 1);
			Blur(lvl, lw, lh, rough * rough * 1.2f);
			for (uint32 y = 0; y < lh; y++)
				for (uint32 x = 0; x < lw; x++)
				{
					const f32 *s = &lvl[((size_t)y * lw + x) * 3];
					f32 *d = &atlas[((size_t)(oy + y) * atlasW + x) * 4];
					d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 1.f;
				}
			oy += lh;
		}
	}
	bool EnvironmentMap::SampleRadiance(const Vec3 &dIn, Vec3 &out)
	{
		if (!g_loaded || g_level0.empty()) return false;
		// (turned as EnvSpecular turns it: uAmbientParams.z is this yaw in radians)
		const f32 a = g_yaw * 3.14159265f / 180.f, cy = cosf(a), sy = sinf(a);
		const Vec3 d(dIn.x * cy - dIn.z * sy, dIn.y, dIn.x * sy + dIn.z * cy);
		const f32 l = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
		if (l <= 0.f) return false;
		const f32 u = 0.5f + atan2f(d.x / l, -d.z / l) * 0.15915494f, v = acosf(std::max(-1.f, std::min(1.f, d.y / l))) * 0.31830989f;
		const uint32 x = std::min<uint32>((uint32)(u * g_level0W), g_level0W - 1), y = std::min<uint32>((uint32)(v * g_level0H), g_level0H - 1);
		const f32 *p = &g_level0[((size_t)y * g_level0W + x) * 4];
		out = Vec3(p[0], p[1], p[2]);
		return true;
	}
	Texture* EnvironmentMap::GetRadianceTexture()
	{
		if (!g_radiance) { g_radiance = std::make_shared<Texture>(); MakeBlack(g_radiance.get()); }
		return g_radiance.get();
	}
	void EnvironmentMap::SetIntensity(const f32 intensity) { g_intensity = intensity; }
	f32 EnvironmentMap::GetIntensity() { return g_loaded ? g_intensity : 0.f; }

	Texture* EnvironmentMap::GetTexture()
	{
		if (!g_texture) { g_texture = std::make_shared<Texture>(); MakeBlack(); }
		return g_texture.get();
	}

	bool EnvironmentMap::Load(const std::string &path, const f32 yawDegrees)
	{
		GetTexture();
		if (g_loaded && path == g_path && yawDegrees == g_yaw) return true;
		int w = 0, h = 0, n = 0;
		f32 *px = stbi_loadf(path.c_str(), &w, &h, &n, 4);
		if (!px || w <= 0 || h <= 0)
		{
			if (px) stbi_image_free(px);
			echo("ERROR: EnvironmentMap - could not read " + path);
			return false;
		}
		// RGBA32F: the one float layout every backend uploads the same way. Unfiltered - not every
		// GPU can filter 32-bit floats - so the Sky effect filters it itself, four texels at a time.
		g_texture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, w, h, false);
		g_texture->UpdateData(px);
		g_texture->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		// Lighting and reflections from it without its sun. A daylight sky's sun is a few pixels
		// thousands of times brighter than the rest (a clear one, 55 000 against a mean of 1.5): kept
		// in, it outweighs the whole sky in nine SH numbers and burns a white disc into every
		// reflection, when the scene's sun - the directional light, with its shadows - is what
		// should carry it. So the light is worked out from a copy capped at 64x the image's mean;
		// the picture drawn as the sky keeps its sun.
		std::vector<f32> lit(px, px + (size_t)w * h * 4);
		{
			f64 sum = 0.0;
			for (size_t i = 0; i < (size_t)w * h; i++) sum += lit[i * 4] * 0.2126 + lit[i * 4 + 1] * 0.7152 + lit[i * 4 + 2] * 0.0722;
			const f32 cap = std::max(1.f, (f32)(sum / ((f64)w * h)) * 64.f);
			for (size_t i = 0; i < (size_t)w * h; i++)
			{
				f32 *q = &lit[i * 4];
				const f32 l = q[0] * 0.2126f + q[1] * 0.7152f + q[2] * 0.0722f;
				if (l > cap) { const f32 k = cap / l; q[0] *= k; q[1] *= k; q[2] *= k; }
			}
		}
		// (the sun's direction from the image as it is)
		{ SphericalHarmonicsL2 unused; ProjectEquirectToSH(px, (uint32)w, (uint32)h, 4, yawDegrees, unused, &g_sun); }
		ProjectEquirectToSH(lit.data(), (uint32)w, (uint32)h, 4, yawDegrees, g_sh, NULL);
		GetRadianceTexture();
		BuildRadiance(lit.data(), (uint32)w, (uint32)h);
		stbi_image_free(px);
		g_path = path; g_yaw = yawDegrees; g_loaded = true;
		return true;
	}

	void EnvironmentMap::Clear()
	{
		if (!g_loaded) return;
		g_loaded = false; g_path.clear(); g_yaw = 0.f; g_sh.Clear(); g_sun = Vec3(0.f, 1.f, 0.f);
		g_level0.clear(); g_level0W = g_level0H = 0;
		if (g_texture) MakeBlack();
		if (g_radiance) MakeBlack(g_radiance.get());
	}

	bool EnvironmentMap::IsLoaded() { return g_loaded; }
	const std::string &EnvironmentMap::GetPath() { return g_path; }
	f32 EnvironmentMap::GetYaw() { return g_yaw; }
	const SphericalHarmonicsL2 &EnvironmentMap::GetSH() { return g_sh; }
	Vec3 EnvironmentMap::GetSunDirection() { return g_sun; }

	void ProjectEquirectToSH(const f32 *pixels, const uint32 width, const uint32 height, const uint32 stride,
		const f32 yawDegrees, SphericalHarmonicsL2 &outSH, Vec3 *outSun)
	{
		outSH.Clear();
		if (!pixels || width == 0 || height == 0) return;
		// Nine numbers need nothing like every pixel: a grid of about 512 across is exact enough.
		const uint32 step = width > 512 ? width / 512 : 1;
		const f32 kPI = 3.14159265358979f;
		const f32 yaw = yawDegrees * kPI / 180.f, cy = cosf(yaw), sy = sinf(yaw);
		const f32 dTheta = kPI * (f32)step / (f32)height, dPhi = 2.f * kPI * (f32)step / (f32)width;
		f32 best = -1.f;
		for (uint32 y = step / 2; y < height; y += step)
		{
			const f32 theta = ((f32)y + 0.5f) / (f32)height * kPI;
			const f32 st = sinf(theta), ct = cosf(theta);
			const f32 dw = st * dTheta * dPhi;
			for (uint32 x = step / 2; x < width; x += step)
			{
				const f32 phi = (((f32)x + 0.5f) / (f32)width - 0.5f) * 2.f * kPI;
				// the image's own direction, then turned by the yaw
				const f32 ix = st * sinf(phi), iz = -st * cosf(phi);
				const Vec3 d(ix * cy + iz * sy, ct, -ix * sy + iz * cy);
				const f32 *p = pixels + ((size_t)y * width + x) * stride;
				const Vec3 c(p[0], p[1], p[2]);
				f32 basis[9];
				EvaluateSHBasis(d, basis);
				for (uint32 i = 0; i < 9; i++) outSH.coefficients[i] += c * (basis[i] * dw);
				if (outSun && theta < kPI * 0.5f)
				{
					const f32 lum = c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f;
					if (lum > best) { best = lum; *outSun = d; }
				}
			}
		}
	}
};
