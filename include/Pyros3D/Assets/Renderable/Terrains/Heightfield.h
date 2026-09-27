//============================================================================
// Name        : Heightfield.h
// Author      : Duarte Peixinho
// Description : Terrain as a square grid of heights - one tile per streamed
//               world cell, typically.
//
//               Local space runs from (0, *, 0) to (size, *, size): the
//               tile's GameObject sits at the tile's corner, so a cell root
//               at the cell origin needs no offset. Row r of the heights is
//               z = r * spacing, column c is x = c * spacing - the layout
//               Box3D's height field uses, cell diagonals included, so what
//               renders is what collides.
//
//               Loading is split like every streamed asset: HeightfieldData
//               and HeightfieldMesh are plain CPU data built on any thread;
//               only Heightfield (the Renderable) touches the device.
//============================================================================

#ifndef HEIGHTFIELD_H
#define HEIGHTFIELD_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	struct PYROS3D_API HeightfieldData
	{
		// Grid points per side, at least 2. A tile of N cells has N+1.
		uint32 samples = 0;
		// Metres covered per side.
		f32 size = 0.f;
		// samples * samples, row-major along z. Metres.
		std::vector<f32> heights;
		f32 minHeight = 0.f, maxHeight = 0.f;
		// What the heightmap CAN reach (heightOffset .. +heightScale), the
		// same for every tile loaded with the same settings - the physics
		// shape quantizes against it so neighbouring tiles line up.
		f32 rangeMin = 0.f, rangeMax = 0.f;

		f32 Spacing() const { return samples > 1 ? size / (f32)(samples - 1) : 0.f; }
		f32 At(const uint32 column, const uint32 row) const { return heights[(size_t)row * samples + column]; }

		// Height at local (x, z), following the same two triangles per cell
		// the mesh and the physics shape use. Clamped to the tile.
		f32 HeightAt(const f32 x, const f32 z) const;
		// Smooth normal from central differences of the grid.
		Vec3 NormalAt(const uint32 column, const uint32 row) const;

		// Heights = pixel (0..1) * heightScale + heightOffset, from the first
		// channel of anything stb reads; 16-bit PNGs keep full precision.
		// Any thread. False (logged) when unreadable or not square.
		static bool LoadFile(const std::string &path, const f32 size, const f32 heightScale, const f32 heightOffset, HeightfieldData &out);
		static bool LoadMemory(const uchar* data, const size_t length, const f32 size, const f32 heightScale, const f32 heightOffset, HeightfieldData &out);
	};

	// One level of detail's vertex data, ready for the device.
	struct PYROS3D_API HeightfieldMesh
	{
		std::vector<Vec3> vertex, normal, tangent, bitangent;
		std::vector<Vec2> texcoord;
		std::vector<uint32> index;

		// Every step-th grid point (a power of two that divides samples-1;
		// others are rounded down to one that does), plus a skirt: a strip
		// hanging skirtDepth metres below each edge, which hides the cracks
		// where two tiles at different steps meet. UVs run 0..1 across the
		// tile. Any thread.
		static void Build(const HeightfieldData &data, const uint32 step, const f32 skirtDepth, HeightfieldMesh &out);
	};

	// One detail level: every step-th grid point, used while the camera
	// is within distance metres of the tile (the last level's distance is
	// ignored - it covers everything beyond).
	struct PYROS3D_API HeightfieldLevel
	{
		uint32 step = 1;
		f32 distance = 0.f;
	};

	// What a tile needs before the device gets involved: its heights and
	// every level's vertex data. Any thread.
	struct PYROS3D_API PreparedHeightfield
	{
		std::shared_ptr<HeightfieldData> data;
		std::vector<HeightfieldMesh> meshes;	// parallel to the levels

		static bool Prepare(const std::string &heightmapPath, const f32 size, const f32 heightScale, const f32 heightOffset,
			const f32 skirt, const std::vector<HeightfieldLevel> &levels, PreparedHeightfield &out);
		// Identifies one set of Prepare() arguments, for AssetBundle.
		static std::string Key(const std::string &heightmapPath, const f32 size, const f32 heightScale, const f32 heightOffset,
			const f32 skirt, const std::vector<HeightfieldLevel> &levels);
	};

	class PYROS3D_API Heightfield : public Primitive
	{
	public:
		// Main thread: takes mesh's arrays and uploads them. data is kept
		// for queries (and a physics shape) and shared across LODs.
		Heightfield(HeightfieldMesh &&mesh, const std::shared_ptr<const HeightfieldData> &data, const uint32 step);

		const std::shared_ptr<const HeightfieldData> &GetData() const { return data; }
		uint32 GetStep() const { return step; }

		// What a scene file records, so the tile can be saved back.
		struct Source
		{
			std::string heightmap;
			f32 heightScale = 1.f, heightOffset = 0.f, skirt = 1.f;
			std::vector<HeightfieldLevel> levels;
		};
		Source source;

	private:
		std::shared_ptr<const HeightfieldData> data;
		uint32 step;
	};

}

#endif /* HEIGHTFIELD_H */
