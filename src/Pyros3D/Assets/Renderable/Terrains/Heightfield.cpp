//============================================================================
// Name        : Heightfield.cpp
// Author      : Duarte Peixinho
// Description : See Heightfield.h.
//============================================================================

#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Core/File/File.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Ext/stb/stb_image.h>

#include <algorithm>
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

	Vec3 HeightfieldData::NormalAt(const uint32 column, const uint32 row) const
	{
		const uint32 c0 = column > 0 ? column - 1 : column, c1 = std::min(column + 1, samples - 1);
		const uint32 r0 = row > 0 ? row - 1 : row, r1 = std::min(row + 1, samples - 1);
		const f32 s = Spacing();
		const f32 dx = (At(c1, row) - At(c0, row)) / (s * (f32)(c1 - c0));
		const f32 dz = (At(column, r1) - At(column, r0)) / (s * (f32)(r1 - r0));
		return Vec3(-dx, 1.f, -dz).normalize();
	}

	void HeightfieldMesh::Build(const HeightfieldData &data, uint32 step, const f32 skirtDepth, HeightfieldMesh &out)
	{
		out = HeightfieldMesh();
		if (data.samples < 2) return;
		const uint32 cells = data.samples - 1;
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
				out.texcoord.push_back(Vec2((f32)gc / cells, (f32)gr / cells));
			}

		out.index.reserve((size_t)(n - 1) * (n - 1) * 6 + (size_t)4 * (n - 1) * 6);
		for (uint32 r = 0; r + 1 < n; r++)
			for (uint32 c = 0; c + 1 < n; c++)
			{
				const uint32 i11 = r * n + c, i12 = i11 + 1, i21 = i11 + n, i22 = i21 + 1;
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
					const uint32 bot0 = base + k, bot1 = base + k + 1;
					out.index.push_back(top0); out.index.push_back(top1); out.index.push_back(bot0);
					out.index.push_back(bot0); out.index.push_back(top1); out.index.push_back(bot1);
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
		const f32 skirt, const std::vector<HeightfieldLevel> &levels, PreparedHeightfield &out)
	{
		out.data = std::make_shared<HeightfieldData>();
		if (!HeightfieldData::LoadFile(heightmapPath, size, heightScale, heightOffset, *out.data))
		{
			out.data.reset();
			return false;
		}
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

	Heightfield::Heightfield(HeightfieldMesh &&mesh, const std::shared_ptr<const HeightfieldData> &data, const uint32 step)
		: data(data), step(step)
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
		geometry->materialProperties.haveColor = true;
		geometry->materialProperties.Color = Vec4(1.f, 1.f, 1.f, 1.f);
		Geometries.push_back(geometry);
		calculateTangentBitangent = true;

		minBounds = Vec3(0.f, data ? data->minHeight : 0.f, 0.f);
		maxBounds = Vec3(data ? data->size : 0.f, data ? data->maxHeight : 0.f, data ? data->size : 0.f);
		BoundingSphereCenter = (minBounds + maxBounds) * 0.5f;
		BoundingSphereRadius = maxBounds.distance(BoundingSphereCenter);
	}

}
