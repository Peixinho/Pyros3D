//============================================================================
// Name        : Heightfield.cpp
// Author      : Duarte Peixinho
// Description : See Heightfield.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Core/File/File.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/stb/stb_image.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace p3d {

	bool HeightfieldData::LoadMemory(const uchar* bytes, const size_t length, const f32 size, const f32 heightScale, const f32 heightOffset, HeightfieldData &out)
	{
		int w = 0, h = 0, channels = 0;
		std::vector<f32> unit;	// 0..1 per pixel
		// 16-bit in every case: stb widens an 8-bit image itself, and a
		// 16-bit heightmap keeps the precision that is its reason to exist.
		stbi_us* px = stbi_load_16_from_memory(bytes, (int)length, &w, &h, &channels, 1);
		if (px)
		{
			unit.resize((size_t)w * h);
			for (size_t i = 0; i < unit.size(); i++) unit[i] = px[i] / 65535.f;
			stbi_image_free(px);
		}
		if (unit.empty())
		{
			echo("ERROR: Heightfield - could not decode the heightmap");
			return false;
		}
		if (w != h || w < 2)
		{
			echo("ERROR: Heightfield - a heightmap must be square and at least 2x2 (got "
				+ std::to_string(w) + "x" + std::to_string(h) + ")");
			return false;
		}

		out.samples = (uint32)w;
		out.size = size;
		out.rangeMin = std::min(heightOffset, heightOffset + heightScale);
		out.rangeMax = std::max(heightOffset, heightOffset + heightScale);
		out.heights.resize(unit.size());
		out.minHeight = 1e30f;
		out.maxHeight = -1e30f;
		for (size_t i = 0; i < unit.size(); i++)
		{
			const f32 y = unit[i] * heightScale + heightOffset;
			out.heights[i] = y;
			out.minHeight = std::min(out.minHeight, y);
			out.maxHeight = std::max(out.maxHeight, y);
		}
		return true;
	}

	bool HeightfieldData::LoadFile(const std::string &path, const f32 size, const f32 heightScale, const f32 heightOffset, HeightfieldData &out)
	{
		File file;
		if (!file.Open(path)) return false;	// File logs it
		const bool ok = file.Size() > 0 && LoadMemory(&file.GetData()[0], file.Size(), size, heightScale, heightOffset, out);
		file.Close();
		if (!ok) echo("ERROR: Heightfield - failed to load " + path);
		return ok;
	}

	f32 HeightfieldData::HeightAt(const f32 x, const f32 z) const
	{
		if (samples < 2) return 0.f;
		const f32 s = Spacing();
		const f32 last = (f32)(samples - 1);
		const f32 fx = std::min(std::max(x / s, 0.f), last);
		const f32 fz = std::min(std::max(z / s, 0.f), last);
		const uint32 c = std::min((uint32)fx, samples - 2);
		const uint32 r = std::min((uint32)fz, samples - 2);
		const f32 u = fx - (f32)c, v = fz - (f32)r;
		const f32 h11 = At(c, r), h12 = At(c + 1, r), h21 = At(c, r + 1), h22 = At(c + 1, r + 1);
		// Split along the 12-21 diagonal, as Box3D does: the triangle
		// (11, 21, 12) holds u + v <= 1, the other (22, 12, 21) the rest.
		if (u + v <= 1.f)
			return h11 + (h12 - h11) * u + (h21 - h11) * v;
		return h22 + (h21 - h22) * (1.f - u) + (h12 - h22) * (1.f - v);
	}

	bool HeightfieldData::IsHoleAt(const f32 x, const f32 z) const
	{
		if (holes.empty() || samples < 2) return false;
		const f32 s = Spacing();
		const f32 fx = std::min(std::max(x / s, 0.f), (f32)(samples - 1)), fz = std::min(std::max(z / s, 0.f), (f32)(samples - 1));
		const uint32 c = std::min((uint32)fx, samples - 2), r = std::min((uint32)fz, samples - 2);
		const f32 u = fx - (f32)c, v = fz - (f32)r;
		const f32 top = HoleAt(c, r) + (HoleAt(c + 1, r) - HoleAt(c, r)) * u;
		const f32 bottom = HoleAt(c, r + 1) + (HoleAt(c + 1, r + 1) - HoleAt(c, r + 1)) * u;
		return top + (bottom - top) * v > 0.5f;
	}

	bool HeightfieldData::LoadHoles(const std::string &path)
	{
		if (samples < 2) return false;
		PaintableImage img;
		if (!img.Load(path, 1) || img.width < 1 || img.height < 1) return false;
		holes.assign((size_t)samples * samples, 0);
		bool any = false;
		for (uint32 r = 0; r < samples; r++)
			for (uint32 c = 0; c < samples; c++)
			{
				const f32 v = img.Sample((f32)c / (samples - 1), (f32)r / (samples - 1), 0);
				const uchar q = (uchar)std::lround(std::min(std::max(v, 0.f), 1.f) * 255.f);
				holes[(size_t)r * samples + c] = q;
				any = any || q > 127;
			}
		if (!any) holes.clear();
		return true;
	}

	Vec3 HeightfieldData::NormalAt(const uint32 column, const uint32 row) const
	{
		if (normals.size() == heights.size() && !normals.empty()) return normals[(size_t)row * samples + column];
		const uint32 c0 = column > 0 ? column - 1 : column, c1 = std::min(column + 1, samples - 1);
		const uint32 r0 = row > 0 ? row - 1 : row, r1 = std::min(row + 1, samples - 1);
		const f32 s = Spacing();
		const f32 dx = (At(c1, row) - At(c0, row)) / (s * (f32)(c1 - c0));
		const f32 dz = (At(column, r1) - At(column, r0)) / (s * (f32)(r1 - r0));
		return Vec3(-dx, 1.f, -dz).normalize();
	}

	namespace { std::atomic<bool> g_headless(false); }
	void HeightfieldMesh::SetHeadless(const bool on) { g_headless = on; }
	bool HeightfieldMesh::IsHeadless() { return g_headless; }

	void HeightfieldMesh::Build(const HeightfieldData &data, uint32 step, const f32 skirtDepth, HeightfieldMesh &out)
	{
		out = HeightfieldMesh();
		if (data.samples < 2) return;
		const uint32 cells = data.samples - 1;
		if (g_headless) step = cells;	// one quad: see SetHeadless
		// The largest power of two <= step that divides the grid, so the
		// last row and column land exactly on the tile's edge.
		uint32 st = 1;
		while (st * 2 <= std::max(step, 1u) && cells % (st * 2) == 0) st *= 2;
		const uint32 n = cells / st + 1;	// points per side at this step
		const f32 spacing = data.Spacing();

		const size_t gridCount = (size_t)n * n;
		const size_t skirtCount = (size_t)4 * n;
		out.vertex.reserve(gridCount + skirtCount);
		out.normal.reserve(gridCount + skirtCount);
		out.texcoord.reserve(gridCount + skirtCount);

		for (uint32 r = 0; r < n; r++)
			for (uint32 c = 0; c < n; c++)
			{
				const uint32 gc = c * st, gr = r * st;
				out.vertex.push_back(Vec3(gc * spacing, data.At(gc, gr), gr * spacing));
				out.normal.push_back(data.NormalAt(gc, gr));
				out.texcoord.push_back(Vec2(data.uvOffset.x + data.uvScale.x * (f32)gc / cells,
					data.uvOffset.y + data.uvScale.y * (f32)gr / cells));
			}

		out.index.reserve((size_t)(n - 1) * (n - 1) * 6 + (size_t)4 * (n - 1) * 6);
		// How much of a hole each point of this level is. A coarser level
		// stands for several points with one, and takes the LEAST open of
		// them: its cut is rougher, and it must err toward leaving ground
		// in - under which there is rock - never toward opening wider than
		// what lies beneath covers.
		const bool holed = !data.holes.empty() && !g_headless;
		std::vector<f32> hole;
		if (holed)
		{
			hole.resize(gridCount);
			const int32 reach = (int32)st / 2;
			for (uint32 r = 0; r < n; r++)
				for (uint32 c = 0; c < n; c++)
				{
					f32 least = 1.f;
					for (int32 dz = -reach; dz <= reach; dz++)
						for (int32 dx = -reach; dx <= reach; dx++)
						{
							const int32 gc = std::min(std::max((int32)(c * st) + dx, 0), (int32)data.samples - 1);
							const int32 gr = std::min(std::max((int32)(r * st) + dz, 0), (int32)data.samples - 1);
							least = std::min(least, data.HoleAt((uint32)gc, (uint32)gr));
						}
					hole[(size_t)r * n + c] = least;
				}
		}
		for (uint32 r = 0; r + 1 < n; r++)
			for (uint32 c = 0; c + 1 < n; c++)
			{
				const uint32 i11 = r * n + c, i12 = i11 + 1, i21 = i11 + n, i22 = i21 + 1;
				if (holed)
				{
					// The quad's corners in winding order, and which are hole.
					const uint32 corner[4] = { i11, i21, i22, i12 };
					const f32 h[4] = { hole[i11], hole[i21], hole[i22], hole[i12] };
					const uint32 open = (h[0] > 0.5f) + (h[1] > 0.5f) + (h[2] > 0.5f) + (h[3] > 0.5f);
					if (open == 4) continue;
					if (open > 0)
					{
						// The rim crosses this quad. What is left of it is a
						// polygon: its ground corners, and on each side where
						// ground turns to hole the point where the field
						// passes one half - marching squares, so the rim runs
						// between grid points instead of along them.
						uint32 poly[8];
						uint32 count = 0;
						for (uint32 k = 0; k < 4; k++)
						{
							const uint32 a = k, b = (k + 1) % 4;
							if (h[a] <= 0.5f) poly[count++] = corner[a];
							if ((h[a] > 0.5f) != (h[b] > 0.5f))
							{
								// Always from the lower-numbered grid point, so
								// the two quads sharing this side put the rim's
								// point in the very same place.
								uint32 va = corner[a], vb = corner[b];
								f32 ha = h[a], hb = h[b];
								if (va > vb) { std::swap(va, vb); std::swap(ha, hb); }
								const f32 t = (0.5f - ha) / (hb - ha);
								out.vertex.push_back(out.vertex[va] + (out.vertex[vb] - out.vertex[va]) * t);
								out.normal.push_back((out.normal[va] + (out.normal[vb] - out.normal[va]) * t).normalize());
								out.texcoord.push_back(out.texcoord[va] + (out.texcoord[vb] - out.texcoord[va]) * t);
								poly[count++] = (uint32)out.vertex.size() - 1;
							}
						}
						for (uint32 k = 1; k + 1 < count; k++)
						{
							out.index.push_back(poly[0]); out.index.push_back(poly[k]); out.index.push_back(poly[k + 1]);
						}
						continue;
					}
				}
				out.index.push_back(i11); out.index.push_back(i21); out.index.push_back(i12);
				out.index.push_back(i22); out.index.push_back(i12); out.index.push_back(i21);
			}

		// Skirts: each edge's points again, dropped by skirtDepth, with the
		// edge's normals so the strip shades like the ground above it.
		if (skirtDepth > 0.f)
		{
			// Edge point k of side e, walking so every skirt quad faces out.
			struct Edge { static uint32 Grid(const uint32 e, const uint32 k, const uint32 n)
			{
				switch (e)
				{
				case 0: return k;							// z = 0, +x
				case 1: return k * n + (n - 1);				// x = max, +z
				case 2: return (n - 1) * n + (n - 1 - k);	// z = max, -x
				default: return (n - 1 - k) * n;			// x = 0, -z
				}
			} };
			for (uint32 e = 0; e < 4; e++)
			{
				const uint32 base = (uint32)out.vertex.size();
				for (uint32 k = 0; k < n; k++)
				{
					const uint32 g = Edge::Grid(e, k, n);
					out.vertex.push_back(out.vertex[g] - Vec3(0.f, skirtDepth, 0.f));
					out.normal.push_back(out.normal[g]);
					out.texcoord.push_back(out.texcoord[g]);
				}
				for (uint32 k = 0; k + 1 < n; k++)
				{
					const uint32 top0 = Edge::Grid(e, k, n), top1 = Edge::Grid(e, k + 1, n);
					uint32 t0 = top0, t1 = top1, b0 = base + k, b1 = base + k + 1;
					if (holed)
					{
						// An opening on the border: no skirt under it, and
						// where the rim crosses this stretch of the edge the
						// skirt stops at the rim, not at the next grid point -
						// or it hangs into the opening like a curtain.
						const bool open0 = hole[top0] > 0.5f, open1 = hole[top1] > 0.5f;
						if (open0 && open1) continue;
						if (open0 != open1)
						{
							// From the lower-numbered point, as the surface does.
							uint32 va = top0, vb = top1;
							if (va > vb) std::swap(va, vb);
							const f32 t = (0.5f - hole[va]) / (hole[vb] - hole[va]);
							const uint32 mid = (uint32)out.vertex.size();
							out.vertex.push_back(out.vertex[va] + (out.vertex[vb] - out.vertex[va]) * t);
							out.normal.push_back((out.normal[va] + (out.normal[vb] - out.normal[va]) * t).normalize());
							out.texcoord.push_back(out.texcoord[va] + (out.texcoord[vb] - out.texcoord[va]) * t);
							out.vertex.push_back(out.vertex[mid] - Vec3(0.f, skirtDepth, 0.f));
							out.normal.push_back(out.normal[mid]);
							out.texcoord.push_back(out.texcoord[mid]);
							if (open0) { t0 = mid; b0 = mid + 1; }
							else { t1 = mid; b1 = mid + 1; }
						}
					}
					out.index.push_back(t0); out.index.push_back(t1); out.index.push_back(b0);
					out.index.push_back(b0); out.index.push_back(t1); out.index.push_back(b1);
				}
			}
		}

		// Tangent along +x (where u grows), bitangent along +z (where v
		// grows), both bent onto the surface by the normal.
		out.tangent.resize(out.vertex.size());
		out.bitangent.resize(out.vertex.size());
		for (size_t i = 0; i < out.vertex.size(); i++)
		{
			const Vec3 &nrm = out.normal[i];
			Vec3 t = Vec3(1.f, 0.f, 0.f) - nrm * nrm.x;
			t = t.normalize();
			out.tangent[i] = t;
			out.bitangent[i] = nrm.cross(t).normalize() * -1.f;
		}
	}

	bool PreparedHeightfield::Prepare(const std::string &heightmapPath, const f32 size, const f32 heightScale, const f32 heightOffset,
		const f32 skirt, const std::vector<HeightfieldLevel> &levels, PreparedHeightfield &out, const std::string &holesPath)
	{
		out.data = std::make_shared<HeightfieldData>();
		if (!HeightfieldData::LoadFile(heightmapPath, size, heightScale, heightOffset, *out.data))
		{
			out.data.reset();
			return false;
		}
		if (!holesPath.empty()) out.data->LoadHoles(holesPath);
		out.meshes.resize(std::max<size_t>(levels.size(), 1));
		for (size_t i = 0; i < out.meshes.size(); i++)
			HeightfieldMesh::Build(*out.data, levels.empty() ? 1 : levels[i].step, skirt, out.meshes[i]);
		return true;
	}

	std::string PreparedHeightfield::Key(const std::string &heightmapPath, const f32 size, const f32 heightScale, const f32 heightOffset,
		const f32 skirt, const std::vector<HeightfieldLevel> &levels)
	{
		std::string k = "heightfield|" + heightmapPath + "|" + std::to_string(size) + "|" + std::to_string(heightScale)
			+ "|" + std::to_string(heightOffset) + "|" + std::to_string(skirt);
		for (size_t i = 0; i < levels.size(); i++) k += "|" + std::to_string(levels[i].step);
		return k;
	}

	void Heightfield::Rebuild()
	{
		if (!data) return;
		HeightfieldData &d = *EditData();
		d.minHeight = 1e30f; d.maxHeight = -1e30f;
		for (size_t i = 0; i < d.heights.size(); i++)
		{
			d.minHeight = std::min(d.minHeight, d.heights[i]);
			d.maxHeight = std::max(d.maxHeight, d.heights[i]);
		}
		HeightfieldMesh mesh;
		HeightfieldMesh::Build(d, step, source.skirt, mesh);
		// Same vertex count as before, but the attribute buffers interleave
		// at SendBuffers() time, so they are rebuilt rather than patched.
		// SendBuffers bumps buffersRevision, which is how the renderer
		// learns the handles changed.
		geometry->Dispose();
		Upload(std::move(mesh));
	}

	void Heightfield::Upload(HeightfieldMesh &&mesh)
	{
		geometry->tVertex = std::move(mesh.vertex);
		geometry->tNormal = std::move(mesh.normal);
		geometry->tTexcoord = std::move(mesh.texcoord);
		geometry->tTangent = std::move(mesh.tangent);
		geometry->tBitangent = std::move(mesh.bitangent);
		geometry->index = std::move(mesh.index);
		if (geometry->tVertex.empty()) return;

		// Primitive::Build() would recompute tangents from the triangles
		// and smooth nothing; the grid already has better of both.
		geometry->CreateBuffers(false);
		AttributeBuffer* attributes = static_cast<AttributeBuffer*>(geometry->Attributes.back());
		// Always present: a normal-mapped Generic material's pipeline reads
		// aTangent, and without it the draw is dropped on Vulkan and Metal.
		attributes->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3, &geometry->tTangent[0], geometry->tTangent.size());
		attributes->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3, &geometry->tBitangent[0], geometry->tBitangent.size());
		geometry->SendBuffers();
		minBounds = Vec3(0.f, data ? data->minHeight : 0.f, 0.f);
		maxBounds = Vec3(data ? data->size : 0.f, data ? data->maxHeight : 0.f, data ? data->size : 0.f);
		BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
		BoundingSphereRadius = maxBounds.distance(BoundingSphereCenter);
	}

	Heightfield::Heightfield(HeightfieldMesh &&mesh, const std::shared_ptr<const HeightfieldData> &data, const uint32 step)
		: data(data), step(step)
	{
		geometry->materialProperties.haveColor = true;
		geometry->materialProperties.Color = Vec4(1.f, 1.f, 1.f, 1.f);
		calculateTangentBitangent = true;
		if (mesh.vertex.empty()) return;
		Upload(std::move(mesh));
		Geometries.push_back(geometry);
	}

}
