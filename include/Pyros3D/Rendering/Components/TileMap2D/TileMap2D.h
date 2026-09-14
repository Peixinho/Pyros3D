//============================================================================
// Name        : TileMap2D.h
// Description : A grid of cells from one tileset, drawn as a handful of
//               batched meshes instead of one GameObject per cell.
//
//               ONE TileMap2D IS ONE LAYER. Several map layers are several
//               of these under Layer2D parents, which is where draw order and
//               parallax already live (see Layer2D.h) - growing a layer stack
//               inside this component would reinvent both.
//
//               Storage is sparse 32x32 chunks, and a chunk is one geometry.
//               Both follow from the same fact: culling is per RenderingMesh,
//               i.e. per geometry (IRenderer::ShadowCasterVisible dispatches
//               on rmesh->CullingGeometry), so chunking is what lets a large
//               map submit only the part of itself that is on screen. Sparse
//               so a level that grows leftwards costs nothing and never needs
//               rebasing.
//
//               The component owns the grid; the geometry lives on a sibling
//               RenderingComponent, which Rebuild() replaces wholesale. That
//               mirrors how a 2D character works (ApplyCharacter2D) and keeps
//               this out of the renderer entirely.
//
//               See TILEMAP_PLAN.md.
//============================================================================

#ifndef TILEMAP2D_H
#define TILEMAP2D_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Assets/TileSet2D/TileSet2D.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Global.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class Renderable;
	class RenderingComponent;
	class IMaterial;
	class Texture;

	// One chunk's mesh, CPU-side. Split out from the geometry that uploads it
	// so the arithmetic - where a cell lands, which texels it samples, how big
	// the chunk is - can be checked without a render device. Everything in
	// here is in the owner's local space.
	struct PYROS3D_API TileChunkMesh2D {
		std::vector<Vec3> vertex;
		std::vector<Vec3> normal;
		std::vector<Vec2> texcoord;
		std::vector<__INDEX_C_TYPE__> index;

		// The chunk's FULL rect, not the filled part of it. Deliberately:
		// bounds that depend on which cells happen to be painted would change
		// on every edit, which means recomputing the component's bounds and
		// re-adopting the renderable for what should be an in-place buffer
		// refill. A chunk's extent is fixed by the grid, so this is stable.
		Vec3 minBounds;
		Vec3 maxBounds;

		// Cells actually emitted. Zero means "do not build a geometry for
		// this chunk at all" - PrimitiveGeometry::CreateBuffers() indexes
		// tVertex[0] and IGeometry::SendBuffers() indexes index[0], so an
		// empty mesh is not an empty draw, it is a crash.
		int32 tileCount = 0;
	};

	class PYROS3D_API TileMap2D : public IComponent {

	public:

		// Cells per chunk edge. 32 is a cull-granularity choice (about half a
		// screen at typical cell sizes), not an index-width one -
		// __INDEX_C_TYPE__ is uint32, so there is no 16-bit vertex ceiling.
		static const int32 CHUNK = 32;

		// Empty cell. Not 0, because 0 is a perfectly good tile index - the
		// first cell of the sheet. (Storage uses 0 as its empty marker and
		// holds index+1, which is why the two differ.)
		static const int32 EMPTY = -1;

		TileMap2D(const Vec2 &tileSize = Vec2(1.f, 1.f));
		virtual ~TileMap2D();

		// --- the tileset ---------------------------------------------------

		// The set is copied in. Its image size must already be known (see
		// TileSet2D::SetImageSize) or every cell's UVs come out as a zero
		// rect; LoadTileSet2D deliberately does not fill it in.
		void SetTileSet(const TileSet2D &set);
		const TileSet2D &GetTileSet() const { return tileset; }

		// Project-relative path of the .p3dt, kept for serialization only -
		// nothing here reads it back. Stored with its "assets/" prefix intact.
		void SetTileSetPath(const std::string &p) { tilesetPath = p; }
		const std::string &GetTileSetPath() const { return tilesetPath; }

		// --- geometry ------------------------------------------------------

		// World units one cell occupies. Independent of the cell's pixel size:
		// a 16px tile drawn at 1x1 world units is the normal case.
		void SetTileSize(const Vec2 &s);
		const Vec2 &GetTileSize() const { return tileSize; }

		// Whether the map's material takes 2D lighting. Off by default: most
		// map layers are backdrops, and Lighting2D on a full-screen layer is
		// paid per fragment.
		void SetLit(const bool l);
		bool IsLit() const { return lit; }

		// --- the grid ------------------------------------------------------

		// Tile coordinates are y-UP, like the world the map sits in: tile
		// (0,0) occupies the quadrant above and right of the owner's origin.
		// NOT the y-down convention tilesets index their own rows with - that
		// one is internal to the atlas and does not leak out here. Getting
		// this backwards mirrors a level vertically and nothing errors.
		int32 GetTile(const int32 x, const int32 y) const;
		// True if the cell actually changed.
		bool SetTile(const int32 x, const int32 y, const int32 index);
		// Inclusive rect, in either order. Returns how many cells changed.
		int32 Fill(const int32 x0, const int32 y0, const int32 x1, const int32 y1,
			const int32 index);
		void ClearTiles();

		// False when nothing is painted at all.
		bool GetTileBounds(int32 &minX, int32 &minY, int32 &maxX, int32 &maxY) const;
		int32 PaintedCount() const;
		// Chunk coordinates holding at least one cell, in a stable order.
		std::vector<std::pair<int32, int32> > NonEmptyChunks() const;

		// Centre of a cell, in the owner's local space.
		Vec2 TileToWorld(const int32 x, const int32 y) const;
		// Which cell a local-space point falls in. Exact inverse of the above
		// for any point inside the cell.
		void WorldToTile(const Vec2 &p, int32 &x, int32 &y) const;

		// --- collision ------------------------------------------------------

		// Solid cells (TileSet2D::IsSolid) merged into as few rectangles as
		// possible, as (centreX, centreY, halfW, halfH) in the owner's local
		// space - ready for Physics2D::SetCompoundBoxes.
		//
		// Greedy: each rectangle grows right as far as it can, then down as
		// far as every row still matches. Not optimal - finding the true
		// minimum rectangle cover is expensive - but a floor comes out as one
		// box and a platformer level as single digits, which is the whole
		// point. Pure, so the merge can be checked without a physics world.
		std::vector<Vec4> BuildColliderBoxes() const;

		// Pushes those onto the sibling Physics2D, if there is one. False
		// when there is nothing to push them to, which is not an error - a
		// decorative layer has no body.
		bool SyncColliders();

		// --- meshing -------------------------------------------------------

		// Builds one chunk's mesh. Pure: touches no GPU state and no sibling
		// component, so it is the seam the tests work at. False (and an
		// untouched `out`) when the chunk holds nothing.
		bool BuildChunkMesh(const int32 cx, const int32 cy, TileChunkMesh2D &out) const;

		// How the atlas path is turned into something loadable. Stored rather
		// than passed per call because Update() rebuilds too, and it has
		// nobody to ask. Only usable by callers whose resolver stays valid
		// for as long as the component lives - the editor's does.
		void SetPathResolver(const std::function<std::string(const std::string&)> &r) { resolve = r; }

		// An atlas path already resolved by the caller, which wins over the
		// resolver above.
		//
		// The scene loader needs this. SceneSerializer resolves against a
		// global asset root that DeserializeScene CLEARS when the load
		// finishes, so a resolver captured during load silently resolves
		// nothing by the time the deferred rebuild runs a frame later - the
		// atlas then loads as the default white texture and the whole map
		// draws as blank quads. Resolve while the root is still set, and hand
		// the answer over.
		//
		// Cleared by SetTileSet, since a new tileset means a new atlas: set
		// it after, not before.
		void SetResolvedAtlasPath(const std::string &absolute);
		const std::string &GetResolvedAtlasPath() const { return resolvedAtlas; }

		// Regenerates everything and hands it to the sibling
		// RenderingComponent. False if there is no sibling to hand it to -
		// this component draws nothing on its own.
		bool Rebuild();

		// Whether an edit is waiting for a rebuild.
		bool NeedsRebuild() const { return fullDirty || !dirtyChunks.empty(); }
		// ...and whether it is the expensive kind. A cell painted inside a
		// chunk that already exists refills that chunk's buffers; a cell that
		// brings a chunk into existence or empties the last one out of it
		// changes the geometry list, which means a new mesh set on the sibling
		// component. A paint tool that wants to know what a stroke will cost
		// asks these two.
		bool NeedsFullRebuild() const { return fullDirty; }
		int32 DirtyChunkCount() const { return (int32)dirtyChunks.size(); }

		virtual void Register(SceneGraph* Scene) {}
		virtual void Init() {}
		// Applies pending edits, once, at the end of the frame they were made.
		// A rect fill spanning nine chunks is nine chunk rebuilds here, not
		// one per cell.
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene) {}

		virtual uint32 GetComponentType() const { return ComponentType::TileMap2D; }

	private:

		struct Chunk {
			// CHUNK*CHUNK cells, row-major from the chunk's (0,0). 0 is empty,
			// anything else is tile index + 1.
			std::vector<uint16> cells;
			// How many are non-zero, so "did this chunk just appear or just
			// empty out" is answered without scanning it. That transition is
			// the one that changes the geometry LIST and so forces a full
			// rebuild rather than an in-place refill.
			int32 filled = 0;
		};

		static int64 Key(const int32 cx, const int32 cy)
		{
			return ((int64)cy << 32) | (int64)(uint32)cx;
		}
		// Floor division - tile -1 belongs to chunk -1, not chunk 0. Integer
		// division truncates towards zero, which would put the whole negative
		// column in chunk 0 and overwrite the positive one.
		static int32 ChunkOf(const int32 t)
		{
			return (t >= 0) ? (t / CHUNK) : -(((-t) + CHUNK - 1) / CHUNK);
		}
		static int32 InChunk(const int32 t)
		{
			const int32 m = t % CHUNK;
			return (m < 0) ? m + CHUNK : m;
		}

		const Chunk* FindChunk(const int32 cx, const int32 cy) const;
		RenderingComponent* FindSibling() const;
		// Loads the atlas and builds the material, if they are not current.
		bool EnsureMaterial();
		// Refills the geometries of dirty chunks without touching the list.
		void RebuildDirtyInPlace();

		TileSet2D tileset;
		std::string tilesetPath;
		Vec2 tileSize;
		bool lit;

		std::map<int64, Chunk> chunks;

		// Chunk keys in the order their geometries sit in the renderable, so
		// an in-place refill can find the geometry belonging to a chunk.
		std::vector<int64> chunkOrder;

		std::vector<int64> dirtyChunks;
		// The solid set may have changed. Tracked separately from the mesh
		// dirt because the two answer different questions: repainting grass
		// over grass redraws nothing and re-collides nothing, but repainting
		// grass over stone redraws one cell and can merge a whole floor
		// differently.
		bool collidersDirty;
		// The geometry list itself is stale - a chunk appeared or emptied, the
		// tileset changed, the cell size changed. Needs a full re-adopt.
		bool fullDirty;

		std::shared_ptr<Texture> atlas;
		std::shared_ptr<IMaterial> material;
		// What `material` was built for, so SetLit/SetTileSet know to rebuild.
		std::string materialFor;
		bool materialLit;

		std::function<std::string(const std::string&)> resolve;
		std::string resolvedAtlas;
	};

};

#endif /* TILEMAP2D_H */
