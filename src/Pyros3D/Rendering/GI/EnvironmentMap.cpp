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
#include <cmath>
#include <memory>
#include <vector>

namespace p3d {

	namespace {
		std::shared_ptr<Texture> g_texture;
		std::string g_path;
		f32 g_yaw = 0.f;
		bool g_loaded = false;
		SphericalHarmonicsL2 g_sh;
		Vec3 g_sun(0.f, 1.f, 0.f);

		void MakeBlack()
		{
			const f32 black[4] = { 0.f, 0.f, 0.f, 1.f };
			g_texture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA32F, 1, 1, false);
			g_texture->UpdateData((void*)black);
			g_texture->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		}
	}

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
		ProjectEquirectToSH(px, (uint32)w, (uint32)h, 4, yawDegrees, g_sh, &g_sun);
		stbi_image_free(px);
		g_path = path; g_yaw = yawDegrees; g_loaded = true;
		return true;
	}

	void EnvironmentMap::Clear()
	{
		if (!g_loaded) return;
		g_loaded = false; g_path.clear(); g_yaw = 0.f; g_sh.Clear(); g_sun = Vec3(0.f, 1.f, 0.f);
		if (g_texture) MakeBlack();
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
