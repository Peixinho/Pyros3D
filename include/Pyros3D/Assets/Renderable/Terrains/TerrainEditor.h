//============================================================================
// Name        : TerrainEditor.h
// Author      : Duarte Peixinho
// Description : Sculpting and painting a scene's terrain - what the editor's
//               brushes call, and what a game may call too (a crater, a
//               trampled path).
//
//               Every brush is a circle in WORLD space and lands on every
//               tile it overlaps. Neighbouring tiles duplicate their shared
//               edge (samples and pixels both), and each tile evaluates the
//               same world-space brush at the same world positions, so a
//               stroke across a border leaves the border seamless.
//
//               Cost is split the way a stroke wants it: during the stroke
//               call Sculpt()/PaintSplat()/PaintFoliage() every frame - they
//               edit the data and rebuild what shows (tile meshes, the splat
//               texture); when the stroke ends call FinishStroke() for what
//               is too slow per frame (collision, foliage regrowth).
//               Save() writes every edited tile's files.
//============================================================================

#ifndef TERRAINEDITOR_H
#define TERRAINEDITOR_H

#include <Pyros3D/Assets/Renderable/Terrains/Heightfield.h>
#include <Pyros3D/Other/Export.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace p3d {

	class SceneGraph;
	class GameObject;
	class RenderingComponent;
	class FoliageComponent;
	class IPhysicsComponent;
	class PaintableImage;

	// One terrain tile found in a scene.
	struct PYROS3D_API TerrainTile
	{
		GameObject* owner = NULL;
		RenderingComponent* rendering = NULL;
		std::vector<Heightfield*> levels;		// level 0 first
		IPhysicsComponent* collision = NULL;	// NULL when the tile has none
		FoliageComponent* foliage = NULL;		// NULL when the tile has none
		Vec3 origin;							// world position of its corner
		// A Terrain's stand-in for a tile that is not loaded: ground to
		// aim at and stand a query on, never edited.
		bool distant = false;

		HeightfieldData* Data() const { return levels.empty() ? NULL : levels[0]->EditData(); }
		f32 Size() const { const HeightfieldData* d = Data(); return d ? d->size : 0.f; }
		bool Contains(const f32 x, const f32 z) const
		{
			return x >= origin.x && z >= origin.z && x <= origin.x + Size() && z <= origin.z + Size();
		}
	};

	class PYROS3D_API TerrainEditor
	{
	public:
		enum SculptMode { Raise, Lower, Smooth, Flatten };

		// Where the scene's relative paths ("assets/terrain/0_0.png")
		// resolve - the project root. Needed to load a density map for
		// painting and to save anything.
		void SetAssetRoot(const std::string &root) { assetRoot = root; }

		// Tiles the brushes must leave alone: a streamed world's far
		// versions are terrain tiles too - baked stand-ins for cells that
		// are not loaded - and a stroke landing on one would edit the
		// stand-in, not the ground. False from `fn` keeps a tile out of
		// Sculpt / PaintSplat / PaintFoliage; it is still ground to
		// FindTiles and HeightAt. Unset, every tile is editable.
		typedef std::function<bool(const GameObject* owner)> TileFilter;
		void SetEditable(const TileFilter &fn) { editable = fn; }
		bool IsEditable(const GameObject* owner) const { return !editable || editable(owner); }

		// Every terrain tile in the scene (at any depth - streamed cells are
		// children of their cell's root).
		static std::vector<TerrainTile> FindTiles(SceneGraph* scene);

		// Ground height under a world position, from whichever tile holds
		// it. False when no tile does.
		static bool HeightAt(SceneGraph* scene, const f32 x, const f32 z, f32 &height);

		// The four splat layer weights (summing to about one) under a world
		// position, from the tile that holds it. False when no loaded tile
		// does or its splat map cannot be read. Reads the painted pixels,
		// so a stroke not yet saved counts.
		bool SplatAt(SceneGraph* scene, const f32 x, const f32 z, f32 weights[4]);

		// Heights within `radius` of (x, z): Raise/Lower by up to `amount`
		// metres at the centre, Smooth toward the neighbourhood average,
		// Flatten toward `target`. `hardness` as PaintableImage's. Rebuilds
		// the touched tiles' meshes. Returns how many tiles changed.
		uint32 Sculpt(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const f32 amount,
			const f32 hardness, const SculptMode mode, const f32 target = 0.f);

		// Moves splat channel `layer` (0..3) toward full within the brush,
		// the other three giving way. The splat image is the tile material's
		// colour map when it is RGBA and clamped - see SplatImage(). Returns
		// how many tiles changed.
		uint32 PaintSplat(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const uint32 layer,
			const f32 strength, const f32 hardness);

		// Moves foliage layer `layer`'s density toward `target` (0..1)
		// within the brush. The field regrows at FinishStroke().
		uint32 PaintFoliage(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const uint32 layer,
			const f32 target, const f32 strength, const f32 hardness);

		// Cuts (open = true) or fills holes: every grid cell whose centre
		// is within `radius` of (x, z). A hole has no triangles and no
		// collision - see HeightfieldData::holes. Returns how many tiles
		// changed.
		uint32 CutHoles(SceneGraph* scene, const f32 x, const f32 z, const f32 radius, const bool open);

		// Caves, under a Terrain object's tiles (see CaveVolume.h): a sphere
		// dug out of the rock (air = true) or packed back in, at a world
		// position. Where the sphere breaks the surface the ground opens
		// (or closes) with it - the tile's holes follow the caves inside
		// the brush. Returns how many tiles changed; 0 on tiles that are
		// not a Terrain's.
		uint32 Dig(SceneGraph* scene, const Vec3 &centre, const f32 radius, const bool air);
		// CutHoles() for a cave: an opening in its walls (or its ring of
		// ground) at a world position - no triangles, no collision there.
		// Returns how many tiles changed.
		uint32 CutCaveHoles(SceneGraph* scene, const Vec3 &centre, const f32 radius, const bool open);
		// One dab of a cave brush (CaveVolume::BrushMode: dig, fill,
		// smooth, level) - Dig() a little at a time, and the two that
		// tidy a cave up. `level` is the floor's world height for Level.
		uint32 CaveBrush(SceneGraph* scene, const int mode, const Vec3 &centre, const f32 radius, const f32 amount,
			const f32 hardness, const f32 level);
		// Noise tunnels (CaveVolume::Generate) under every loaded tile of
		// every Terrain within `radius` of `centre` on the plane. With
		// minDepth at or under zero they break the surface, and the ground
		// opens there. Returns how many tiles changed.
		uint32 GenerateCaves(SceneGraph* scene, const Vec3 &centre, const f32 radius, const uint32 seed, const f32 size,
			const f32 width, const f32 minDepth, const f32 maxDepth);

		// Every loaded tile that has caves: its openings and walls made
		// again from its voxels - for caves saved by an older build, whose
		// openings were cut differently. Returns how many tiles.
		uint32 ResyncCaves(SceneGraph* scene);

		// The stroke is over: rebuild collision and regrow foliage on every
		// tile it touched.
		void FinishStroke();

		// Writes every tile edited since the last Save(): heightmaps as
		// 16-bit PNG, splat and density maps as 8-bit, under the asset root.
		// A density
		// map a layer did not have yet is created next to its heightmap.
		// Returns false if any file failed.
		bool Save();

		// Whether `owner`'s tile has edits Save() has not written - a
		// streamed cell holding one must not be unloaded.
		bool HasUnsaved(const GameObject* owner) const;
		// Every tile with edits Save() has not written.
		std::vector<const GameObject*> UnsavedOwners() const;
		// `owner` is leaving the scene (its cell unloaded): drop what is
		// kept for it. Unsaved edits to it are lost.
		void Forget(const GameObject* owner);

		// Undo. A tile's editable state - heights, splat pixels, density
		// maps - keyed by its heightmap path, which outlives the GameObject
		// (a cell can unload and come back as a new one).
		struct TileSnapshot
		{
			std::string heightmap;
			std::vector<f32> heights;
			std::vector<uchar> holes;	// empty = none
			std::vector<uchar> caves;	// CaveVolume::ToBlob; empty = none
			std::vector<uchar> splat;
			std::vector<std::vector<uchar> > density;	// per foliage layer; empty = none loaded
		};
		// Between BeginStroke() and FinishStroke(), the first touch of each
		// tile records how it was; FinishStroke() records how it ended, and
		// TakeStrokeUndo() hands both over (false when nothing changed).
		void BeginStroke();
		bool TakeStrokeUndo(std::vector<TileSnapshot> &before, std::vector<TileSnapshot> &after);
		// Puts tiles back as snapshotted: meshes, collision, splat texture
		// and foliage all follow, and the tiles count as edited. Tiles not
		// in the scene now are skipped.
		void Restore(SceneGraph* scene, const std::vector<TileSnapshot> &snapshots);

		// Per tile, the splat map being painted and the density maps, loaded
		// on first touch.
		struct TileState
		{
			GameObject* owner = NULL;
			bool heightsDirty = false, collisionDirty = false, splatDirty = false, holesDirty = false;
			bool cavesDirty = false, caveCollisionDirty = false;
			std::vector<bool> foliageDirty;
			std::shared_ptr<PaintableImage> splat;
			std::string splatPath;
		};

	private:
		TileState &State(const TerrainTile &tile);
		std::vector<TerrainTile> EditableTiles(SceneGraph* scene) const;
		// After a tile's caves changed: its holes within `reach` of (x, z)
		// become what the caves say (air at the surface), levels rebuilt.
		void SyncHolesToCaves(const TerrainTile &tile, const f32 x, const f32 z, const f32 reach);
		void MarkCaveEdit(const TerrainTile &tile);
		TileFilter editable;
		std::shared_ptr<PaintableImage> SplatImage(const TerrainTile &tile, TileState &state);
		std::string Resolve(const std::string &path) const;
		TileSnapshot Capture(const TerrainTile &tile);
		void Touch(const TerrainTile &tile);	// records the before-snapshot once per stroke
		bool recording = false;
		std::vector<TileSnapshot> strokeBefore;
		std::vector<TerrainTile> strokeTouched;
		std::vector<TileSnapshot> lastBefore, lastAfter;
		std::vector<TileState> states;
		std::vector<TerrainTile> strokeTiles;
		SceneGraph* strokeScene = NULL;
		std::string assetRoot;
	};

}

#endif /* TERRAINEDITOR_H */
