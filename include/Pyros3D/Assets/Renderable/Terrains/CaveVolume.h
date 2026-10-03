//============================================================================
// Name        : CaveVolume.h
// Author      : Duarte Peixinho
// Description : What a heightfield cannot be: ground with something under
//               it. A terrain tile's caves are a voxel field over the tile -
//               how much of each lattice point is air, 0 (rock) to 255 -
//               kept sparsely, in 16-point chunks that exist only where
//               something was dug. Everything undug is rock, so a tile with
//               no caves stores nothing.
//
//               What is drawn is the surface of the rock: rock is whatever
//               is under the terrain's ground and has not been dug. Deep
//               down that is the cave's wall. Where a cave comes through
//               the ground it is ALSO the ground around the opening: the
//               mesh runs from a ring of ground, over the rim, down the
//               wall, in one piece - so the rim cannot have a gap, however
//               many overlapping digs made it. The terrain's own surface
//               is cut away a little outside the rim
//               (HeightfieldData::holes, from OpeningAt) and simply ends
//               on top of that ring.
//
//               Meshed with surface nets: a vertex in every cell the
//               surface crosses, a quad across every lattice edge it cuts.
//               Brushes and noise write smooth ramps, so the walls are
//               smooth too. The mesh faces the open side.
//
//               The lattice is the tile's: `cells` cells of `voxel` metres
//               a side along x and z, points 0..cells on the tile, plus a
//               few layers beyond each side (kApron) so that the
//               cells on a border are meshed alike by both tiles and the
//               walls join without a seam. Unbounded along y. Positions
//               are local to the tile (its corner at the origin).
//============================================================================

#ifndef CAVEVOLUME_H
#define CAVEVOLUME_H

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Other/Export.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace p3d {

	struct HeightfieldData;

	struct PYROS3D_API CaveMeshData
	{
		std::vector<Vec3> vertex, normal;
		std::vector<uint32> index;
	};

	class PYROS3D_API CaveVolume
	{
	public:
		CaveVolume(const f32 voxel, const int32 cells) : voxel(voxel), cells(cells) {}

		f32 Voxel() const { return voxel; }
		int32 Cells() const { return cells; }
		bool Empty() const { return chunks.empty(); }
		size_t ChunkCount() const { return chunks.size(); }

		// One lattice point's air, 0..255; anywhere never written is rock.
		uchar Get(const int32 x, const int32 y, const int32 z) const;
		void Set(const int32 x, const int32 y, const int32 z, const uchar air);
		// Air at a local position, 0..1, between lattice points.
		f32 AirAt(const Vec3 &local) const;

		// A sphere of air (dig) or rock (fill) at a local position, with a
		// soft rim two voxels wide so the wall it leaves is round. Points
		// off the tile's lattice (past its apron) are left alone. Returns
		// whether anything changed.
		bool Carve(const Vec3 &centre, const f32 radius, const bool air);

		// The brushes, a little at a time. `amount` is how much of the
		// brush's full effect this dab applies (0..1, more for all of it at
		// once): held down, a brush works at a rate, not in a single frame.
		// Strongest at the centre, fading to the rim by `hardness`.
		//   Dig    - toward air: the sphere a full Carve() would leave.
		//   Fill   - toward rock, likewise.
		//   Smooth - each point toward the mean of its neighbours: takes the
		//            ridges between overlapping digs off a wall.
		//   Level  - a floor at height `level` (local y): rock below it,
		//            air above, within the brush. What a cave needs before
		//            anyone can walk in it.
		// Returns whether anything changed.
		enum BrushMode { Dig, Fill, Smooth, Level };
		bool Brush(const BrushMode mode, const Vec3 &centre, const f32 radius, const f32 amount, const f32 hardness, const f32 level = 0.f);

		// Holes in the cave's own surface: where this second field passes
		// one half the walls are not drawn and do not collide - an opening
		// for whatever is put there instead (a door, a mine's timbering, a
		// tunnel modelled by hand), as HeightfieldData::holes are for the
		// ground. The rock is still rock to everything else. A sphere
		// opened (or closed again) at a local position, with the soft rim
		// that makes the cut's edge round. Whether anything changed.
		bool CutHole(const Vec3 &centre, const f32 radius, const bool open);
		// How much of a hole a local position is, 0..1.
		f32 HoleAt(const Vec3 &local) const;
		bool HasHoles() const { return !holes.empty(); }

		// Winding tunnels from 3D noise: air where two noise fields are
		// both near zero, which is a network of tubes. Only between
		// minDepth and maxDepth metres under `ground`, and within `radius`
		// of `centre` on the plane (local; radius <= 0 = the whole tile).
		// `origin` is the tile's corner in the terrain, so tunnels run on
		// across tiles. Returns whether anything changed.
		struct Noise
		{
			uint32 seed = 1;
			f32 size = 48.f;		// metres from one bend to the next
			f32 width = 0.09f;		// 0..0.5: how fat the tunnels are
			f32 minDepth = 6.f, maxDepth = 90.f;
		};
		bool Generate(const HeightfieldData &ground, const Vec3 &origin, const Vec3 &centre, const f32 radius, const Noise &noise);

		// The ground's height at a tile-local (x, z) - asked for points a
		// voxel or two past the tile as well, where the caller answers from
		// the neighbouring tile if it can.
		typedef std::function<f32(const f32 x, const f32 z)> GroundFn;

		// The rock's surface wherever digging changed it: cave walls, and
		// the ground itself for a cell or two around every opening (a few
		// centimetres under the terrain's, which lies on top of it).
		// Without `ground` everything undug is rock, ground or no ground.
		// lastX / lastZ: no tile follows on that side, so this one closes
		// the border itself. Any thread `ground` may be called from.
		void BuildMesh(const GroundFn &ground, const bool lastX, const bool lastZ, CaveMeshData &out) const;

		// How open the terrain's surface is at a point on it (tile-local),
		// 0..1: what HeightfieldData::holes holds there. Past one half the
		// terrain is not drawn - which happens a little outside the cave's
		// rim, over the ring of ground BuildMesh() makes.
		f32 OpeningAt(const Vec3 &surface) const;

		// How far under the terrain's surface the mesh's ground lies.
		static f32 GroundDrop() { return 0.06f; }

		// The whole field as bytes (run-length packed), and back. An empty
		// blob is an empty volume.
		std::vector<uchar> ToBlob() const;
		bool FromBlob(const std::vector<uchar> &blob);
		bool Save(const std::string &path) const;
		bool Load(const std::string &path);

		static const int32 kChunk = 16;
		// Lattice layers kept past each border of the tile: what a border
		// cell's vertex, its normal and the ring of ground round an opening
		// near the border all read from the neighbour's side.
		static const int32 kApron = 3;

	private:
		typedef std::tuple<int32, int32, int32> Key;
		static int32 Floor16(const int32 v) { return v >= 0 ? v / kChunk : -((-v + kChunk - 1) / kChunk); }
		void DropEmptyChunks();

		f32 voxel;
		int32 cells;
		typedef std::map<Key, std::vector<uchar> > Field;
		static uchar GetIn(const Field &field, const int32 x, const int32 y, const int32 z);
		static void SetIn(Field &field, const int32 x, const int32 y, const int32 z, const uchar value);
		static void DropEmpty(Field &field);
		static f32 SampleIn(const Field &field, const f32 voxel, const Vec3 &local);
		void ClipToHoles(CaveMeshData &mesh) const;
		Field holes;	// like chunks: how much of a hole each lattice point is
		std::map<Key, std::vector<uchar> > chunks;	// kChunk^3 each, x fastest, then z, then y
		uint32 dabs = 0;	// stirs the rounding of each dab's small changes
	};

	// The walls as something to draw.
	class PYROS3D_API CaveMesh : public Primitive
	{
	public:
		// Main thread: takes the arrays and uploads them.
		CaveMesh(CaveMeshData &&mesh);
	};

}

#endif /* CAVEVOLUME_H */
