//============================================================================
// Name        : TerrainHorizon.cpp
// Description : The terrain's own shadow, baked as a horizon map
//============================================================================

#include <Pyros3D/Rendering/Terrain/TerrainHorizon.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <algorithm>
#include <cmath>

namespace p3d {

	TerrainHorizon::~TerrainHorizon()
	{
		delete texture;
	}

	f32 TerrainHorizon::ShadeAt(const f32 x, const f32 z, const Vec3 &toSun) const
	{
		if (resolution == 0) return 1.f;
		const f32 u = (x - rect.x) * rect.z, v = (z - rect.y) * rect.w;
		if (u < 0.f || v < 0.f || u > 1.f || v > 1.f) return 1.f;
		const int N = (int)resolution;
		const int i = std::min(N - 1, (int)(u * (f32)N)), j = std::min(N - 1, (int)(v * (f32)N));
		f32 turn = atan2f(toSun.z, toSun.x) / 6.28318531f;
		if (turn < 0.f) turn += 1.f;
		const f32 k = turn * (f32)Directions;
		const int k0 = ((int)floorf(k)) % Directions, k1 = (k0 + 1) % Directions;
		const unsigned char* h = &horizons[((size_t)j * N + i) * Directions];
		const f32 t = k - floorf(k);
		const f32 horizon = ((f32)h[k0] * (1.f - t) + (f32)h[k1] * t) / 255.f;
		const f32 lo = horizon - softness, hi = horizon + softness;
		const f32 s = std::max(0.f, std::min(1.f, (toSun.y - lo) / (hi - lo)));
		return s * s * (3.f - 2.f * s);
	}

	std::shared_ptr<TerrainHorizon> TerrainHorizon::Bake(SceneGraph* scene, const uint32 wanted, const f32 reach)
	{
		if (scene == NULL) return std::shared_ptr<TerrainHorizon>();
		const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
		f32 minX = 0.f, minZ = 0.f, maxX = 0.f, maxZ = 0.f;
		bool any = false;
		for (size_t i = 0; i < tiles.size(); i++)
		{
			const f32 size = tiles[i].Size();
			if (size <= 0.f) continue;
			const Vec3 &o = tiles[i].origin;
			if (!any) { minX = o.x; minZ = o.z; maxX = o.x + size; maxZ = o.z + size; any = true; }
			else { minX = std::min(minX, o.x); minZ = std::min(minZ, o.z); maxX = std::max(maxX, o.x + size); maxZ = std::max(maxZ, o.z + size); }
		}
		if (!any || maxX - minX < 1.f || maxZ - minZ < 1.f) return std::shared_ptr<TerrainHorizon>();

		const int N = (int)std::max(64u, std::min(2048u, wanted));
		const f32 sizeX = maxX - minX, sizeZ = maxZ - minZ;
		const f32 stepX = sizeX / (f32)N, stepZ = sizeZ / (f32)N;

		// the ground, once, at the middle of every texel
		std::vector<f32> height((size_t)N * N, 0.f);
		f32 lowest = 1e30f;
		for (int j = 0; j < N; j++)
			for (int i = 0; i < N; i++)
			{
				f32 h = 0.f;
				if (TerrainEditor::HeightAt(scene, minX + ((f32)i + 0.5f) * stepX, minZ + ((f32)j + 0.5f) * stepZ, h)) lowest = std::min(lowest, h);
				else h = -1e30f;       // (no tile here: filled in below)
				height[(size_t)j * N + i] = h;
			}
		if (lowest > 1e29f) return std::shared_ptr<TerrainHorizon>();
		for (size_t k = 0; k < height.size(); k++) if (height[k] < -1e29f) height[k] = lowest;

		std::shared_ptr<TerrainHorizon> out(new TerrainHorizon());
		out->resolution = (uint32)N;
		out->rect = Vec4(minX, minZ, 1.f / sizeX, 1.f / sizeZ);
		out->horizons.assign((size_t)N * N * Directions, 0);

		// How far along a direction the ground is asked: close together near
		// to, further apart further off - a hill's edge matters most where it
		// is nearest.
		std::vector<f32> along;
		for (f32 d = 1.5f; d * std::min(stepX, stepZ) <= reach && along.size() < 40; d *= 1.3f) along.push_back(d);

		for (int k = 0; k < Directions; k++)
		{
			const f32 angle = (f32)k * (6.28318531f / (f32)Directions);
			const f32 dx = cosf(angle), dz = sinf(angle);
			// (a step of one texel in the direction, in texels of x and of z)
			const f32 tx = dx / stepX * std::min(stepX, stepZ), tz = dz / stepZ * std::min(stepX, stepZ);
			const f32 metresPerStep = std::min(stepX, stepZ);
			for (int j = 0; j < N; j++)
				for (int i = 0; i < N; i++)
				{
					const f32 here = height[(size_t)j * N + i];
					f32 highest = 0.f;       // the sine of the horizon's height; the flat is 0
					for (size_t s = 0; s < along.size(); s++)
					{
						const int x = (int)((f32)i + tx * along[s] + 0.5f), z = (int)((f32)j + tz * along[s] + 0.5f);
						if (x < 0 || z < 0 || x >= N || z >= N) break;
						const f32 rise = height[(size_t)z * N + x] - here;
						if (rise <= 0.f) continue;
						const f32 run = along[s] * metresPerStep;
						const f32 sine = rise / sqrtf(rise * rise + run * run);
						if (sine > highest) highest = sine;
					}
					out->horizons[((size_t)j * N + i) * Directions + k] = (unsigned char)std::min(255.f, highest * 255.f + 0.5f);
				}
		}

		// Four pictures side by side, four directions each in their channels.
		const int W = N * (Directions / 4);
		std::vector<unsigned char> pixels((size_t)W * N * 4);
		for (int j = 0; j < N; j++)
			for (int i = 0; i < N; i++)
			{
				const unsigned char* h = &out->horizons[((size_t)j * N + i) * Directions];
				for (int c = 0; c < Directions / 4; c++)
				{
					unsigned char* px = &pixels[((size_t)j * W + (size_t)c * N + i) * 4];
					px[0] = h[c * 4]; px[1] = h[c * 4 + 1]; px[2] = h[c * 4 + 2]; px[3] = h[c * 4 + 3];
				}
			}
		out->texture = new Texture();
		out->texture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, W, N, false);
		out->texture->UpdateData(&pixels[0]);
		out->texture->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
		out->texture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		return out;
	}

}
