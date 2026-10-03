//============================================================================
// Name        : CaveVolume.cpp
// Author      : Duarte Peixinho
// Description : See CaveVolume.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Terrains/CaveVolume.h>
#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace p3d {

	namespace {
		const size_t kChunkBytes = (size_t)CaveVolume::kChunk * CaveVolume::kChunk * CaveVolume::kChunk;

		// Gradient noise in space, about -1..1.
		f32 Perlin3(const f32 x, const f32 y, const f32 z, const uint32 seed)
		{
			auto grad = [seed](const int32 ix, const int32 iy, const int32 iz, const f32 dx, const f32 dy, const f32 dz) {
				uint32 h = (uint32)ix * 374761393u + (uint32)iy * 668265263u + (uint32)iz * 2147483647u + seed * 2246822519u;
				h = (h ^ (h >> 13)) * 1274126177u;
				h ^= h >> 16;
				// One of the twelve edge directions of a cube.
				switch (h % 12u)
				{
				case 0: return dx + dy; case 1: return -dx + dy; case 2: return dx - dy; case 3: return -dx - dy;
				case 4: return dx + dz; case 5: return -dx + dz; case 6: return dx - dz; case 7: return -dx - dz;
				case 8: return dy + dz; case 9: return -dy + dz; case 10: return dy - dz; default: return -dy - dz;
				}
			};
			const f32 fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
			const int32 x0 = (int32)fx, y0 = (int32)fy, z0 = (int32)fz;
			const f32 tx = x - fx, ty = y - fy, tz = z - fz;
			auto fade = [](const f32 t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); };
			const f32 u = fade(tx), v = fade(ty), w = fade(tz);
			auto lerp = [](const f32 a, const f32 b, const f32 t) { return a + (b - a) * t; };
			const f32 a = lerp(grad(x0, y0, z0, tx, ty, tz), grad(x0 + 1, y0, z0, tx - 1.f, ty, tz), u);
			const f32 b = lerp(grad(x0, y0 + 1, z0, tx, ty - 1.f, tz), grad(x0 + 1, y0 + 1, z0, tx - 1.f, ty - 1.f, tz), u);
			const f32 c = lerp(grad(x0, y0, z0 + 1, tx, ty, tz - 1.f), grad(x0 + 1, y0, z0 + 1, tx - 1.f, ty, tz - 1.f), u);
			const f32 d = lerp(grad(x0, y0 + 1, z0 + 1, tx, ty - 1.f, tz - 1.f), grad(x0 + 1, y0 + 1, z0 + 1, tx - 1.f, ty - 1.f, tz - 1.f), u);
			return lerp(lerp(a, b, v), lerp(c, d, v), w);
		}

		uint64 CellKey(const int32 x, const int32 y, const int32 z)
		{
			const uint64 bias = 1u << 20;
			return ((uint64)(x + (int64)bias) & 0x1fffff) | (((uint64)(y + (int64)bias) & 0x1fffff) << 21) | (((uint64)(z + (int64)bias) & 0x1fffff) << 42);
		}

		void Put32(std::vector<uchar> &out, const uint32 v) { for (int i = 0; i < 4; i++) out.push_back((uchar)(v >> (8 * i))); }
		uint32 Take32(const std::vector<uchar> &in, size_t &at) { uint32 v = 0; for (int i = 0; i < 4; i++) v |= (uint32)in[at++] << (8 * i); return v; }
	}

	uchar CaveVolume::GetIn(const Field &field, const int32 x, const int32 y, const int32 z)
	{
		const int32 cx = Floor16(x), cy = Floor16(y), cz = Floor16(z);
		Field::const_iterator it = field.find(Key(cx, cy, cz));
		if (it == field.end()) return 0;
		return it->second[((size_t)(y - cy * kChunk) * kChunk + (size_t)(z - cz * kChunk)) * kChunk + (size_t)(x - cx * kChunk)];
	}

	void CaveVolume::SetIn(Field &field, const int32 x, const int32 y, const int32 z, const uchar value)
	{
		const int32 cx = Floor16(x), cy = Floor16(y), cz = Floor16(z);
		const Key key(cx, cy, cz);
		Field::iterator it = field.find(key);
		if (it == field.end())
		{
			if (value == 0) return;	// zero is what a missing chunk already is
			it = field.insert(std::make_pair(key, std::vector<uchar>(kChunkBytes, 0))).first;
		}
		it->second[((size_t)(y - cy * kChunk) * kChunk + (size_t)(z - cz * kChunk)) * kChunk + (size_t)(x - cx * kChunk)] = value;
	}

	void CaveVolume::DropEmpty(Field &field)
	{
		for (Field::iterator it = field.begin(); it != field.end();)
		{
			bool any = false;
			for (size_t i = 0; i < it->second.size() && !any; i++) any = it->second[i] != 0;
			if (any) ++it; else it = field.erase(it);
		}
	}

	f32 CaveVolume::SampleIn(const Field &field, const f32 voxel, const Vec3 &local)
	{
		if (field.empty()) return 0.f;
		const f32 fx = local.x / voxel, fy = local.y / voxel, fz = local.z / voxel;
		const f32 bx = std::floor(fx), by = std::floor(fy), bz = std::floor(fz);
		const int32 x = (int32)bx, y = (int32)by, z = (int32)bz;
		const f32 u = fx - bx, v = fy - by, w = fz - bz;
		auto at = [&field](const int32 ix, const int32 iy, const int32 iz) { return GetIn(field, ix, iy, iz) / 255.f; };
		const f32 a = at(x, y, z) + (at(x + 1, y, z) - at(x, y, z)) * u;
		const f32 b = at(x, y, z + 1) + (at(x + 1, y, z + 1) - at(x, y, z + 1)) * u;
		const f32 c = at(x, y + 1, z) + (at(x + 1, y + 1, z) - at(x, y + 1, z)) * u;
		const f32 d = at(x, y + 1, z + 1) + (at(x + 1, y + 1, z + 1) - at(x, y + 1, z + 1)) * u;
		const f32 low = a + (b - a) * w, high = c + (d - c) * w;
		return low + (high - low) * v;
	}

	uchar CaveVolume::Get(const int32 x, const int32 y, const int32 z) const { return GetIn(chunks, x, y, z); }
	void CaveVolume::Set(const int32 x, const int32 y, const int32 z, const uchar air) { SetIn(chunks, x, y, z, air); }
	void CaveVolume::DropEmptyChunks() { DropEmpty(chunks); }
	f32 CaveVolume::AirAt(const Vec3 &local) const { return SampleIn(chunks, voxel, local); }
	f32 CaveVolume::HoleAt(const Vec3 &local) const { return SampleIn(holes, voxel, local); }

	bool CaveVolume::CutHole(const Vec3 &centre, const f32 radius, const bool open)
	{
		if (radius <= 0.f) return false;
		const f32 reach = radius + voxel;
		const int32 x0 = std::max(-kApron, (int32)std::floor((centre.x - reach) / voxel)), x1 = std::min(cells + kApron, (int32)std::ceil((centre.x + reach) / voxel));
		const int32 z0 = std::max(-kApron, (int32)std::floor((centre.z - reach) / voxel)), z1 = std::min(cells + kApron, (int32)std::ceil((centre.z + reach) / voxel));
		const int32 y0 = (int32)std::floor((centre.y - reach) / voxel), y1 = (int32)std::ceil((centre.y + reach) / voxel);
		bool changed = false;
		for (int32 y = y0; y <= y1; y++)
			for (int32 z = z0; z <= z1; z++)
				for (int32 x = x0; x <= x1; x++)
				{
					const Vec3 p(x * voxel, y * voxel, z * voxel);
					const f32 inside = std::min(std::max(0.5f + (radius - p.distance(centre)) / (2.f * voxel), 0.f), 1.f);
					if (inside <= 0.f) continue;
					const uchar was = GetIn(holes, x, y, z);
					const uchar q = (uchar)std::lround((open ? inside : 1.f - inside) * 255.f);
					const uchar now = open ? std::max(was, q) : std::min(was, q);
					if (now == was) continue;
					SetIn(holes, x, y, z, now);
					changed = true;
				}
		if (changed && !open) DropEmpty(holes);
		return changed;
	}

	// The mesh, less what lies in a hole: each triangle is kept, dropped, or
	// cut along the line where the hole field passes one half - so the
	// opening's edge is as round as the brush that cut it.
	void CaveVolume::ClipToHoles(CaveMeshData &mesh) const
	{
		if (holes.empty() || mesh.index.empty()) return;
		std::vector<f32> m(mesh.vertex.size());
		bool any = false;
		for (size_t i = 0; i < m.size(); i++) { m[i] = HoleAt(mesh.vertex[i]); any = any || m[i] > 0.5f; }
		if (!any) return;
		std::unordered_map<uint64, uint32> cuts;	// an edge's cut point, shared by the triangles either side
		auto cut = [&](uint32 a, uint32 b) -> uint32 {
			if (a > b) std::swap(a, b);
			const uint64 key = ((uint64)a << 32) | b;
			std::unordered_map<uint64, uint32>::const_iterator it = cuts.find(key);
			if (it != cuts.end()) return it->second;
			const f32 t = (0.5f - m[a]) / (m[b] - m[a]);
			const uint32 index = (uint32)mesh.vertex.size();
			mesh.vertex.push_back(mesh.vertex[a] + (mesh.vertex[b] - mesh.vertex[a]) * t);
			const Vec3 n = mesh.normal[a] + (mesh.normal[b] - mesh.normal[a]) * t;
			mesh.normal.push_back(n.magnitude() > 1e-6f ? n.normalize() : mesh.normal[a]);
			cuts[key] = index;
			return index;
		};
		std::vector<uint32> kept;
		kept.reserve(mesh.index.size());
		for (size_t i = 0; i + 2 < mesh.index.size(); i += 3)
		{
			const uint32 v[3] = { mesh.index[i], mesh.index[i + 1], mesh.index[i + 2] };
			const bool in[3] = { m[v[0]] > 0.5f, m[v[1]] > 0.5f, m[v[2]] > 0.5f };
			const int gone = in[0] + in[1] + in[2];
			if (gone == 3) continue;
			if (gone == 0) { kept.push_back(v[0]); kept.push_back(v[1]); kept.push_back(v[2]); continue; }
			// What is left, walked in the triangle's own order.
			uint32 poly[4];
			uint32 count = 0;
			for (int k = 0; k < 3; k++)
			{
				const int a = k, b = (k + 1) % 3;
				if (!in[a]) poly[count++] = v[a];
				if (in[a] != in[b]) poly[count++] = cut(v[a], v[b]);
			}
			for (uint32 k = 1; k + 1 < count; k++) { kept.push_back(poly[0]); kept.push_back(poly[k]); kept.push_back(poly[k + 1]); }
		}
		mesh.index.swap(kept);
		if (mesh.index.empty()) mesh = CaveMeshData();
	}

	bool CaveVolume::Carve(const Vec3 &centre, const f32 radius, const bool air)
	{
		if (radius <= 0.f) return false;
		const f32 reach = radius + voxel;
		const int32 x0 = std::max(-kApron, (int32)std::floor((centre.x - reach) / voxel)), x1 = std::min(cells + kApron, (int32)std::ceil((centre.x + reach) / voxel));
		const int32 z0 = std::max(-kApron, (int32)std::floor((centre.z - reach) / voxel)), z1 = std::min(cells + kApron, (int32)std::ceil((centre.z + reach) / voxel));
		const int32 y0 = (int32)std::floor((centre.y - reach) / voxel), y1 = (int32)std::ceil((centre.y + reach) / voxel);
		bool changed = false;
		for (int32 y = y0; y <= y1; y++)
			for (int32 z = z0; z <= z1; z++)
				for (int32 x = x0; x <= x1; x++)
				{
					const Vec3 p(x * voxel, y * voxel, z * voxel);
					// Linear in distance across the rim, so the half-way
					// surface is the sphere.
					const f32 inside = std::min(std::max(0.5f + (radius - p.distance(centre)) / (2.f * voxel), 0.f), 1.f);
					if (inside <= 0.f) continue;
					const uchar was = Get(x, y, z);
					const uchar q = (uchar)std::lround((air ? inside : 1.f - inside) * 255.f);
					const uchar now = air ? std::max(was, q) : std::min(was, q);
					if (now == was) continue;
					Set(x, y, z, now);
					changed = true;
				}
		if (changed && !air) DropEmptyChunks();
		return changed;
	}

	bool CaveVolume::Brush(const BrushMode mode, const Vec3 &centre, const f32 radius, const f32 amount, const f32 hardness, const f32 level)
	{
		if (radius <= 0.f || amount <= 0.f) return false;
		const f32 reach = radius + voxel;
		const int32 x0 = std::max(-kApron, (int32)std::floor((centre.x - reach) / voxel)), x1 = std::min(cells + kApron, (int32)std::ceil((centre.x + reach) / voxel));
		const int32 z0 = std::max(-kApron, (int32)std::floor((centre.z - reach) / voxel)), z1 = std::min(cells + kApron, (int32)std::ceil((centre.z + reach) / voxel));
		const int32 y0 = (int32)std::floor((centre.y - reach) / voxel), y1 = (int32)std::ceil((centre.y + reach) / voxel);
		if (x1 < x0 || z1 < z0) return false;
		// Smoothing reads every point's neighbours as they were before this
		// dab - one already smoothed would drag its neighbour after it.
		const int32 nx = x1 - x0 + 3, ny = y1 - y0 + 3, nz = z1 - z0 + 3;
		std::vector<uchar> before;
		if (mode == Smooth)
		{
			before.resize((size_t)nx * ny * nz);
			for (int32 y = 0; y < ny; y++)
				for (int32 z = 0; z < nz; z++)
					for (int32 x = 0; x < nx; x++) before[((size_t)y * nz + z) * nx + x] = Get(x0 - 1 + x, y0 - 1 + y, z0 - 1 + z);
		}
		auto was8 = [&](const int32 x, const int32 y, const int32 z) -> f32 {
			return before[((size_t)(y - y0 + 1) * nz + (z - z0 + 1)) * nx + (x - x0 + 1)] / 255.f;
		};
		dabs++;
		bool changed = false;
		for (int32 y = y0; y <= y1; y++)
			for (int32 z = z0; z <= z1; z++)
				for (int32 x = x0; x <= x1; x++)
				{
					const Vec3 p(x * voxel, y * voxel, z * voxel);
					const f32 d = p.distance(centre);
					// The soft sphere a whole dab leaves, and how hard this
					// dab works here: most at the centre.
					const f32 inside = std::min(std::max(0.5f + (radius - d) / (2.f * voxel), 0.f), 1.f);
					if (inside <= 0.f) continue;
					const f32 t = std::min(d / radius, 1.f);
					const f32 fall = hardness >= 1.f ? 1.f : std::min(std::max((1.f - t) / std::max(1.f - hardness, 1e-3f), 0.f), 1.f);
					const f32 step = amount * (0.15f + 0.85f * fall);
					const uchar was8bit = Get(x, y, z);
					const f32 was = was8bit / 255.f;
					f32 now = was;
					switch (mode)
					{
					case Dig: now = std::max(was, std::min(inside, was + step)); break;
					case Fill: now = std::min(was, std::max(1.f - inside, was - step)); break;
					case Smooth:
					{
						const f32 mean = (was8(x, y, z) * 2.f + was8(x - 1, y, z) + was8(x + 1, y, z) + was8(x, y - 1, z) + was8(x, y + 1, z)
							+ was8(x, y, z - 1) + was8(x, y, z + 1)) / 8.f;
						now = was + (mean - was) * std::min(1.f, step * 4.f) * inside;
					}
					break;
					case Level:
					{
						const f32 target = std::min(std::max(0.5f + (p.y - level) / (2.f * voxel), 0.f), 1.f);
						now = was + (target - was) * std::min(1.f, step) * inside;
					}
					break;
					}
					// A dab's change is often less than one of the 255 steps a
					// point has: rounded at random (a hash, the same each run)
					// it adds up over dabs instead of never happening.
					uint32 h = (uint32)x * 374761393u + (uint32)y * 668265263u + (uint32)z * 2147483647u + dabs * 2246822519u;
					h = (h ^ (h >> 13)) * 1274126177u;
					const f32 dither = (f32)((h ^ (h >> 16)) & 0xffff) / 65536.f;
					const uchar q = (uchar)std::min(255.f, std::max(0.f, std::floor(now * 255.f + dither)));
					if (q == was8bit) continue;
					Set(x, y, z, q);
					changed = true;
				}
		if (changed && mode != Dig) DropEmptyChunks();
		return changed;
	}

	bool CaveVolume::Generate(const HeightfieldData &ground, const Vec3 &origin, const Vec3 &centre, const f32 radius, const Noise &noise)
	{
		const f32 size = std::max(4.f, noise.size);
		const f32 width = std::min(std::max(noise.width, 0.f), 0.5f);
		if (width <= 0.f || noise.maxDepth <= noise.minDepth) return false;
		// The noise changes by about 1.5 / size a metre; the rim is a ramp
		// two voxels wide.
		const f32 soft = 3.f * voxel / size;
		const f32 fadeMetres = 8.f;
		bool changed = false;
		for (int32 z = -kApron; z <= cells + kApron; z++)
			for (int32 x = -kApron; x <= cells + kApron; x++)
			{
				const f32 lx = x * voxel, lz = z * voxel;
				f32 radial = 1.f;
				if (radius > 0.f)
				{
					const f32 d = std::sqrt((lx - centre.x) * (lx - centre.x) + (lz - centre.z) * (lz - centre.z));
					radial = std::min(std::max((radius - d) / fadeMetres, 0.f), 1.f);
					if (radial <= 0.f) continue;
				}
				const f32 h = ground.HeightAt(std::min(std::max(lx, 0.f), ground.size), std::min(std::max(lz, 0.f), ground.size));
				const int32 y0 = (int32)std::floor((h - noise.maxDepth) / voxel), y1 = (int32)std::ceil((h - noise.minDepth) / voxel);
				for (int32 y = y0; y <= y1; y++)
				{
					const f32 ly = y * voxel;
					const f32 depth = h - ly;
					// Tunnels thin to nothing at the top and bottom of the band
					// and at the edge of the area, rather than being cut off.
					const f32 band = std::min(std::max(std::min(depth - noise.minDepth, noise.maxDepth - depth) / fadeMetres, 0.f), 1.f);
					const f32 w = width * band * radial;
					if (w <= 0.f) continue;
					const Vec3 p((origin.x + lx) / size, (origin.y + ly) / size, (origin.z + lz) / size);
					// Stretched along y: caves that wander more than they climb.
					const f32 n1 = Perlin3(p.x, p.y * 1.6f, p.z, noise.seed);
					const f32 n2 = Perlin3(p.x + 31.7f, p.y * 1.6f + 11.3f, p.z - 17.1f, noise.seed + 7919u);
					const f32 f = std::max(std::fabs(n1), std::fabs(n2));
					const f32 air = std::min(std::max(0.5f + (w - f) / soft, 0.f), 1.f);
					const uchar q = (uchar)std::lround(air * 255.f);
					if (q <= Get(x, y, z)) continue;
					Set(x, y, z, q);
					changed = true;
				}
			}
		return changed;
	}

	f32 CaveVolume::OpeningAt(const Vec3 &surface) const
	{
		if (chunks.empty()) return 0.f;
		// The air at the surface, or just under it (a crust thinner than
		// half a voxel is not ground worth keeping). The terrain goes where
		// that passes 0.15: well out on the brush's soft rim, two thirds of
		// a voxel outside the wall. Kept as a gentle ramp around that
		// level, not a step - the terrain's cut is interpolated between
		// grid points, and a step would make it a staircase.
		const f32 air = std::max(AirAt(surface), AirAt(surface - Vec3(0.f, voxel * 0.5f, 0.f)));
		return std::min(std::max(0.5f + (air - 0.15f), 0.f), 1.f);
	}

	void CaveVolume::BuildMesh(const GroundFn &ground, const bool lastX, const bool lastZ, CaveMeshData &out) const
	{
		out = CaveMeshData();
		if (chunks.empty()) return;
		const uint32 kNone = 0xffffffffu;

		// How much rock a point is, 0..1: under the ground and not dug. The
		// ground is a ramp one voxel either side of its surface, linear in
		// height, so the surface the mesher finds is the ground's own.
		std::unordered_map<uint64, f32> columnHeight;
		auto heightAt = [&](const int32 x, const int32 z) -> f32 {
			const uint64 key = CellKey(x, 0, z);
			std::unordered_map<uint64, f32>::const_iterator it = columnHeight.find(key);
			if (it != columnHeight.end()) return it->second;
			// Under the terrain's surface by a few centimetres - and by more
			// where the ground is steep: a lattice follows a slope in steps,
			// and the steeper it is the further its surface strays from the
			// terrain's. It must stray downward, or rock shows through the
			// grass.
			const f32 here = ground(x * voxel, z * voxel);
			const f32 rise = std::max(std::max(std::fabs(ground((x + 1) * voxel, z * voxel) - here), std::fabs(ground((x - 1) * voxel, z * voxel) - here)),
				std::max(std::fabs(ground(x * voxel, (z + 1) * voxel) - here), std::fabs(ground(x * voxel, (z - 1) * voxel) - here)));
			const f32 h = here - GroundDrop() - 0.3f * rise;
			columnHeight[key] = h;
			return h;
		};
		auto rock = [&](const int32 x, const int32 y, const int32 z) -> f32 {
			const f32 undug = 1.f - Get(x, y, z) / 255.f;
			if (!ground) return undug;
			return std::min(undug, std::min(std::max(0.5f + (heightAt(x, z) - y * voxel) / (2.f * voxel), 0.f), 1.f));
		};
		auto rockAt = [&](const Vec3 &p) -> f32 {
			const f32 undug = 1.f - AirAt(p);
			if (!ground) return undug;
			return std::min(undug, std::min(std::max(0.5f + (ground(p.x, p.z) - GroundDrop() - p.y) / (2.f * voxel), 0.f), 1.f));
		};

		// The cells digging touched - any corner with air in it - and the
		// cells round those: where the rock's surface is not simply the
		// terrain's, plus the ring of ground the terrain's edge rests on.
		std::unordered_map<uint64, uint32> touched;	// cell -> its vertex, or kNone
		std::vector<std::tuple<int32, int32, int32> > order;
		for (std::map<Key, std::vector<uchar> >::const_iterator it = chunks.begin(); it != chunks.end(); ++it)
		{
			const int32 bx = std::get<0>(it->first) * kChunk, by = std::get<1>(it->first) * kChunk, bz = std::get<2>(it->first) * kChunk;
			const std::vector<uchar> &data = it->second;
			for (int32 ly = 0; ly < kChunk; ly++)
				for (int32 lz = 0; lz < kChunk; lz++)
					for (int32 lx = 0; lx < kChunk; lx++)
					{
						if (data[((size_t)ly * kChunk + lz) * kChunk + lx] == 0) continue;
						// The 8 cells sharing this point, and two more each way
						// across the ground (one up and down): the ring has
						// to reach past where the terrain's cut can wander.
						for (int32 dy = -2; dy <= 1; dy++)
							for (int32 dz = -3; dz <= 2; dz++)
								for (int32 dx = -3; dx <= 2; dx++)
								{
									const int32 x = bx + lx + dx, y = by + ly + dy, z = bz + lz + dz;
									if (touched.insert(std::make_pair(CellKey(x, y, z), kNone)).second)
										order.push_back(std::make_tuple(x, y, z));
								}
					}
		}

		// Each cell's vertex: the mean of where the surface crosses its edges.
		for (size_t i = 0; i < order.size(); i++)
		{
			const int32 x = std::get<0>(order[i]), y = std::get<1>(order[i]), z = std::get<2>(order[i]);
			f32 a[8];
			for (int k = 0; k < 8; k++) a[k] = rock(x + (k & 1), y + ((k >> 1) & 1), z + ((k >> 2) & 1));
			static const int edges[12][2] = { {0,1},{2,3},{4,5},{6,7}, {0,2},{1,3},{4,6},{5,7}, {0,4},{1,5},{2,6},{3,7} };
			Vec3 sum;
			uint32 count = 0;
			for (int e = 0; e < 12; e++)
			{
				const int i0 = edges[e][0], i1 = edges[e][1];
				if ((a[i0] > 0.5f) == (a[i1] > 0.5f)) continue;
				const f32 t = (0.5f - a[i0]) / (a[i1] - a[i0]);
				const Vec3 p0((f32)(i0 & 1), (f32)((i0 >> 1) & 1), (f32)((i0 >> 2) & 1)), p1((f32)(i1 & 1), (f32)((i1 >> 1) & 1), (f32)((i1 >> 2) & 1));
				sum = sum + p0 + (p1 - p0) * t;
				count++;
			}
			if (count == 0) continue;
			touched[CellKey(x, y, z)] = (uint32)out.vertex.size();
			out.vertex.push_back((Vec3((f32)x, (f32)y, (f32)z) + sum * (1.f / count)) * voxel);
		}
		auto vertexOf = [&](const int32 x, const int32 y, const int32 z) -> uint32 {
			std::unordered_map<uint64, uint32>::const_iterator it = touched.find(CellKey(x, y, z));
			return it == touched.end() ? kNone : it->second;
		};

		// A quad across the lattice edge from m along `axis`, facing the
		// open side - where all four cells round the edge are ours.
		auto edge = [&](const int32 mx, const int32 my, const int32 mz, const int axis) {
			const f32 low = rock(mx, my, mz), high = rock(mx + (axis == 0), my + (axis == 1), mz + (axis == 2));
			if ((low > 0.5f) == (high > 0.5f)) return;
			// Which tile makes it: the one whose lattice the edge is on, short
			// of the far border - that plane is the next tile's first.
			auto owns = [this](const int32 v, const bool last) { return v >= 0 && (v < cells || (v == cells && last)); };
			if (axis == 0 && !(mx >= 0 && mx <= cells - 1 && owns(mz, lastZ))) return;
			if (axis == 1 && !(owns(mx, lastX) && owns(mz, lastZ))) return;
			if (axis == 2 && !(owns(mx, lastX) && mz >= 0 && mz <= cells - 1)) return;
			uint32 q[4];
			if (axis == 0) { q[0] = vertexOf(mx, my - 1, mz - 1); q[1] = vertexOf(mx, my, mz - 1); q[2] = vertexOf(mx, my, mz); q[3] = vertexOf(mx, my - 1, mz); }
			else if (axis == 1) { q[0] = vertexOf(mx - 1, my, mz - 1); q[1] = vertexOf(mx, my, mz - 1); q[2] = vertexOf(mx, my, mz); q[3] = vertexOf(mx - 1, my, mz); }
			else { q[0] = vertexOf(mx - 1, my - 1, mz); q[1] = vertexOf(mx, my - 1, mz); q[2] = vertexOf(mx, my, mz); q[3] = vertexOf(mx - 1, my, mz); }
			if (q[0] == kNone || q[1] == kNone || q[2] == kNone || q[3] == kNone) return;
			Vec3 open(axis == 0 ? 1.f : 0.f, axis == 1 ? 1.f : 0.f, axis == 2 ? 1.f : 0.f);
			if (high > 0.5f) open = open * -1.f;	// rock ahead: the open side is behind
			const Vec3 n = (out.vertex[q[1]] - out.vertex[q[0]]).cross(out.vertex[q[2]] - out.vertex[q[0]])
				+ (out.vertex[q[2]] - out.vertex[q[0]]).cross(out.vertex[q[3]] - out.vertex[q[0]]);
			if (n.dotProduct(open) >= 0.f)
			{
				out.index.push_back(q[0]); out.index.push_back(q[1]); out.index.push_back(q[2]);
				out.index.push_back(q[0]); out.index.push_back(q[2]); out.index.push_back(q[3]);
			}
			else
			{
				out.index.push_back(q[0]); out.index.push_back(q[2]); out.index.push_back(q[1]);
				out.index.push_back(q[0]); out.index.push_back(q[3]); out.index.push_back(q[2]);
			}
		};
		// Every lattice edge once: each of our cells brings the three that
		// leave its low corner. (The cell is the one of the four round such
		// an edge with the highest indices, so no edge comes up twice.)
		for (size_t i = 0; i < order.size(); i++)
		{
			const int32 x = std::get<0>(order[i]), y = std::get<1>(order[i]), z = std::get<2>(order[i]);
			edge(x, y, z, 0);
			edge(x, y, z, 1);
			edge(x, y, z, 2);
		}
		if (out.index.empty()) { out = CaveMeshData(); return; }

		// Normals: down the rock's slope, toward the open side the surface
		// is seen from.
		out.normal.resize(out.vertex.size());
		const f32 e = voxel * 0.5f;
		for (size_t i = 0; i < out.vertex.size(); i++)
		{
			const Vec3 &p = out.vertex[i];
			Vec3 g(rockAt(p - Vec3(e, 0.f, 0.f)) - rockAt(p + Vec3(e, 0.f, 0.f)),
				rockAt(p - Vec3(0.f, e, 0.f)) - rockAt(p + Vec3(0.f, e, 0.f)),
				rockAt(p - Vec3(0.f, 0.f, e)) - rockAt(p + Vec3(0.f, 0.f, e)));
			out.normal[i] = g.magnitude() > 1e-5f ? g.normalize() : Vec3(0.f, 1.f, 0.f);
		}
		ClipToHoles(out);
	}

	std::vector<uchar> CaveVolume::ToBlob() const
	{
		std::vector<uchar> out;
		if (chunks.empty()) return out;
		out.push_back('P'); out.push_back('C'); out.push_back('V'); out.push_back('1');
		uint32 voxelBits;
		std::memcpy(&voxelBits, &voxel, 4);
		Put32(out, voxelBits);
		Put32(out, (uint32)cells);
		auto write = [&out](const Field &field) {
			Put32(out, (uint32)field.size());
			for (Field::const_iterator it = field.begin(); it != field.end(); ++it)
			{
				Put32(out, (uint32)std::get<0>(it->first));
				Put32(out, (uint32)std::get<1>(it->first));
				Put32(out, (uint32)std::get<2>(it->first));
				// Runs of (length - 1, value): most of a chunk is one of two values.
				const std::vector<uchar> &d = it->second;
				for (size_t i = 0; i < d.size();)
				{
					size_t run = 1;
					while (i + run < d.size() && run < 256 && d[i + run] == d[i]) run++;
					out.push_back((uchar)(run - 1));
					out.push_back(d[i]);
					i += run;
				}
			}
		};
		write(chunks);
		// After the air, and only when there are any: the holes. A file
		// without this section is one written before caves had holes.
		if (!holes.empty())
		{
			out.push_back('H'); out.push_back('O'); out.push_back('L'); out.push_back('E');
			write(holes);
		}
		return out;
	}

	bool CaveVolume::FromBlob(const std::vector<uchar> &in)
	{
		chunks.clear();
		holes.clear();
		if (in.empty()) return true;
		if (in.size() < 16 || in[0] != 'P' || in[1] != 'C' || in[2] != 'V' || in[3] != '1') return false;
		size_t at = 4;
		const uint32 voxelBits = Take32(in, at);
		f32 fileVoxel;
		std::memcpy(&fileVoxel, &voxelBits, 4);
		const int32 fileCells = (int32)Take32(in, at);
		const uint32 count = Take32(in, at);
		// Made for another lattice: not this tile's caves.
		if (std::fabs(fileVoxel - voxel) > 1e-4f || fileCells != cells) return false;
		auto read = [&](Field &field, const uint32 n) -> bool {
			for (uint32 c = 0; c < n; c++)
			{
				if (at + 12 > in.size()) return false;
				const int32 cx = (int32)Take32(in, at), cy = (int32)Take32(in, at), cz = (int32)Take32(in, at);
				std::vector<uchar> d(kChunkBytes, 0);
				size_t filled = 0;
				while (filled < kChunkBytes)
				{
					if (at + 2 > in.size()) return false;
					const size_t run = (size_t)in[at] + 1;
					const uchar value = in[at + 1];
					at += 2;
					if (filled + run > kChunkBytes) return false;
					std::memset(&d[filled], value, run);
					filled += run;
				}
				field[Key(cx, cy, cz)].swap(d);
			}
			return true;
		};
		if (!read(chunks, count)) { chunks.clear(); return false; }
		if (at + 8 <= in.size() && in[at] == 'H' && in[at + 1] == 'O' && in[at + 2] == 'L' && in[at + 3] == 'E')
		{
			at += 4;
			const uint32 holeCount = Take32(in, at);
			if (!read(holes, holeCount)) { chunks.clear(); holes.clear(); return false; }
		}
		return true;
	}

	bool CaveVolume::Save(const std::string &path) const
	{
		const std::vector<uchar> blob = ToBlob();
		std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
		if (!blob.empty()) out.write((const char*)&blob[0], (std::streamsize)blob.size());
		return (bool)out;
	}

	bool CaveVolume::Load(const std::string &path)
	{
		std::ifstream in(path.c_str(), std::ios::binary);
		if (!in) return false;
		const std::vector<uchar> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return FromBlob(blob);
	}

	CaveMesh::CaveMesh(CaveMeshData &&mesh)
	{
		geometry->materialProperties.haveColor = true;
		geometry->materialProperties.Color = Vec4(1.f, 1.f, 1.f, 1.f);
		calculateTangentBitangent = true;
		if (mesh.vertex.empty() || mesh.index.empty()) return;
		const size_t n = mesh.vertex.size();
		geometry->tTexcoord.resize(n);
		geometry->tTangent.resize(n);
		geometry->tBitangent.resize(n);
		Vec3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
		for (size_t i = 0; i < n; i++)
		{
			const Vec3 &p = mesh.vertex[i];
			const Vec3 &nrm = mesh.normal[i];
			// Nothing here has a natural unwrap; a material for caves maps
			// by world position. These are for whatever insists on some.
			geometry->tTexcoord[i] = Vec2(p.x * 0.25f, p.z * 0.25f);
			Vec3 t = std::fabs(nrm.x) < 0.9f ? Vec3(1.f, 0.f, 0.f) - nrm * nrm.x : Vec3(0.f, 0.f, 1.f) - nrm * nrm.z;
			t = t.normalize();
			geometry->tTangent[i] = t;
			geometry->tBitangent[i] = nrm.cross(t).normalize() * -1.f;
			lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
			hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
		}
		geometry->tVertex = std::move(mesh.vertex);
		geometry->tNormal = std::move(mesh.normal);
		geometry->index = std::move(mesh.index);
		geometry->CreateBuffers(false);
		AttributeBuffer* attributes = static_cast<AttributeBuffer*>(geometry->Attributes.back());
		attributes->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3, &geometry->tTangent[0], geometry->tTangent.size());
		attributes->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3, &geometry->tBitangent[0], geometry->tBitangent.size());
		geometry->SendBuffers();
		minBounds = lo;
		maxBounds = hi;
		BoundingSphereCenter = (lo + hi) * 0.5f;
		BoundingSphereRadius = hi.distance(BoundingSphereCenter);
		Geometries.push_back(geometry);
	}

}
