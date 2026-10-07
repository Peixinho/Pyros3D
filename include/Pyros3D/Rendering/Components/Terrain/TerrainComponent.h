//============================================================================
// Name        : TerrainComponent.h
// Author      : Duarte Peixinho
// Description : A terrain as one object. The GameObject carrying this
//               component IS the terrain: a grid of tilesX x tilesZ square
//               tiles whose maps live in one directory -
//               "<x>_<z>.png" (16-bit heights), "<x>_<z>_splat.png" and a
//               density map per painted foliage layer - and it brings its
//               own tiles in and out as children.
//
//               The object sits at the terrain's centre: tile (0, 0)'s
//               corner is half the terrain's extent back along x and z.
//               A terrain put at the origin surrounds it, and its gizmo is
//               in the middle of the ground rather than off one corner.
//
//               Every tile is made from one template: the subtree a tile
//               would be in a scene file (a heightfield RenderingComponent
//               with its material, a HeightField physics shape, a Foliage
//               component), with "{tile}" wherever a path names the tile.
//               Material, ground layers, detail levels and foliage are
//               therefore set once, on the terrain, not once per tile.
//
//               Tiles within loadRadius of a viewer are full tiles, loaded
//               through AssetStreamer like a streamed world's cells and
//               dropped past unloadRadius. Every other tile is drawn from
//               the overview: one low-resolution heightmap and one colour
//               image for the whole terrain ("overview.png",
//               "overview_color.png", baked by BakeOverview). A distant
//               tile is cut from that grid with the grid's normals, so it
//               shades across its borders - no seam between tiles - and
//               all of them share one material.
//
//               Viewers are per scene and set from outside
//               (SetViewers) because the scene has no idea which camera
//               is the player's - a server passes every client's.
//
//               The terrain does not pump AssetStreamer: an app with a
//               WorldStreamer already does, and one without must call
//               AssetStreamer::Instance().Pump() once a frame.
//
//               Main thread only, except where noted.
//============================================================================

#ifndef TERRAINCOMPONENT_H
#define TERRAINCOMPONENT_H

#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/Utils/Streaming/AssetStreamer.h>
#include <Pyros3D/Other/Export.h>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sol { class state; }

namespace p3d {

	class IPhysics;
	class IMaterial;
	class Heightfield;
	class CaveVolume;
	struct HeightfieldData;
	class RenderingComponent;
	struct LoadedSceneAssets;

	class PYROS3D_API TerrainComponent : public IComponent
	{
	public:
		struct Settings
		{
			// Where the tile maps and the overview live, relative to the
			// asset root ("assets/terrain/Terrain").
			std::string directory;
			int32 tilesX = 1, tilesZ = 1;
			f32 tileSize = 256.f;
			f32 loadRadius = 512.f, unloadRadius = 640.f;
			// Distant tiles are drawn out to here; 0 = the whole terrain.
			f32 viewDistance = 0.f;
			// Grid points per tile side in the overview (a power of two
			// plus one).
			uint32 overviewSamples = 33;
			// Caves: the side of a voxel of each tile's CaveVolume, in
			// metres (rounded so a whole number fit the tile), and the
			// material their walls are drawn with, as a scene file writes
			// one (JSON text; empty = plain rock grey).
			f32 caveVoxel = 2.f;
			std::string caveMaterial;
			// One tile as a subtree, JSON text: {"materials": [...],
			// "root": {...}}, "{tile}" standing for "<x>_<z>" in paths.
			std::string tileTemplate;
		};

		// assetRoot is the project root the settings' paths resolve
		// against; physics and lua are what tiles are built with, as a
		// scene load's are.
		TerrainComponent(const Settings &settings, const std::string &assetRoot, IPhysics* physics = NULL, sol::state* lua = NULL);
		virtual ~TerrainComponent();

		const Settings &GetSettings() const { return settings; }
		const std::string &GetAssetRoot() const { return assetRoot; }
		// A terrain made in a scene that has no file yet has no root to
		// inherit; whoever knows the project gives it one. Tiles and the
		// overview are fetched again from there.
		void SetAssetRoot(const std::string &root);
		void SetRadii(const f32 load, const f32 unload);
		void SetViewDistance(const f32 distance) { settings.viewDistance = std::max(0.f, distance); }
		// A new template: every loaded tile goes and is rebuilt from it -
		// unless reload is false, for a change the loaded tiles were given
		// some other way.
		void SetTileTemplate(const std::string &tileTemplate, const bool reload = true);
		// Every full tile out now, veto or not, and back in from the next
		// Update() - after the maps on disk changed under them.
		void ReloadTiles();

		// What the template's heightfield says: metres per unit of
		// heightmap, and the height of a zero.
		f32 HeightScale() const { return heightScale; }
		f32 HeightOffset() const { return heightOffset; }

		// Where each scene's viewers are this frame. An empty list keeps
		// what is loaded and loads nothing new.
		static void SetViewers(const SceneGraph* scene, const std::vector<Vec3> &viewers);
		// Every live terrain, in any scene.
		static const std::vector<TerrainComponent*> &Instances();
		// Whether `object` is a terrain's distant tile - ground to stand a
		// ray on, never to edit.
		static bool IsDistantTile(const GameObject* object);

		// Blocks until every tile wanted around `foci` is in - a loading
		// screen, or a tool about to edit the ground there.
		void LoadAround(const std::vector<Vec3> &foci);

		// The tile whose square holds a world position; false off the grid.
		bool TileAt(const Vec3 &world, int32 &x, int32 &z) const;
		// Which tile a loaded tile object is.
		bool TileOf(const GameObject* tile, int32 &x, int32 &z) const;
		// The loaded tile's object, NULL unless loaded.
		GameObject* GetTile(const int32 x, const int32 z) const;
		uint32 LoadedCount() const;
		uint32 LoadingCount() const;
		uint32 DistantCount() const;

		// Tile (0, 0)'s corner, relative to the object: the terrain is
		// centred on it.
		Vec3 Corner() const { return Vec3(-0.5f * settings.tilesX * settings.tileSize, 0.f, -0.5f * settings.tilesZ * settings.tileSize); }
		// A tile's corner in the world.
		Vec3 TileOrigin(const int32 x, const int32 z) const;

		// "<x>_<z>", and the tile's heightmap relative to the asset root.
		static std::string TileStem(const int32 x, const int32 z);
		std::string HeightmapPath(const int32 x, const int32 z) const;

		// Asked before a tile leaves: true keeps it. The editor keeps a
		// tile with unsaved brush edits until it is saved.
		typedef std::function<bool(GameObject* tile)> UnloadVeto;
		void SetUnloadVeto(const UnloadVeto &veto) { unloadVeto = veto; }
		typedef std::function<void(GameObject* tile)> TileCallback;
		void SetOnTileLoaded(const TileCallback &cb) { onLoaded = cb; }
		void SetOnTileUnloading(const TileCallback &cb) { onUnloading = cb; }
		bool HasHooks() const { return (bool)unloadVeto; }

		// ---- caves ----------------------------------------------------
		// Each tile may carry a CaveVolume ("<x>_<z>_caves.bin" next to
		// its heightmap), loaded with the tile and drawn as a child of it,
		// with collision. See CaveVolume.h.
		//
		// The tile's volume, NULL when it has none (create makes one) or
		// the tile is not loaded.
		CaveVolume* TileCaves(const int32 x, const int32 z, const bool create = false);
		// A sphere of air or rock at a world position, in one tile's
		// volume. Whether it changed anything.
		bool CarveTile(const int32 x, const int32 z, const Vec3 &world, const f32 radius, const bool air);
		// A hole opened (or closed) in the tile's cave walls
		// (CaveVolume::CutHole) at a world position. Nothing to cut where
		// the tile has no caves.
		bool CutCaveHole(const int32 x, const int32 z, const Vec3 &world, const f32 radius, const bool open);
		// One dab of a cave brush (CaveVolume::Brush, mode as its
		// BrushMode) at a world position, `level` a world height.
		bool BrushTile(const int32 x, const int32 z, const int mode, const Vec3 &world, const f32 radius, const f32 amount,
			const f32 hardness, const f32 level);
		// Makes the tile's cave walls again from its volume; collision too
		// when asked (not on every dab of a stroke - at its end).
		void RebuildCave(const int32 x, const int32 z, const bool collision);
		// How much air a world position is in, 0..1 - 0 anywhere no cave
		// was dug or no tile is loaded.
		f32 AirAt(const Vec3 &world) const;
		// A tile's volume as bytes, and put back (rebuilds the walls).
		std::vector<uchar> CaveBlob(const int32 x, const int32 z) const;
		void SetCaveBlob(const int32 x, const int32 z, const std::vector<uchar> &blob);
		// Writes the tile's volume (removes the file when it is empty).
		bool SaveCaves(const int32 x, const int32 z);
		void SetCaveMaterial(const std::string &materialJson);
		// How open the terrain's surface is at a world point on it, 0..1
		// (CaveVolume::OpeningAt): what a tile's holes take from its caves.
		f32 OpeningAt(const Vec3 &surface) const;
		// The ground's height at a world position, from whichever loaded
		// tile holds it; `fallback` off them all.
		f32 GroundHeight(const f32 x, const f32 z, const f32 fallback) const;
		// The loaded tile's heights, NULL unless loaded.
		const HeightfieldData* TileGround(const int32 x, const int32 z) const;
		f32 CaveVoxel() const { return caveVoxel; }

		// (Re)writes the overview from the tile maps on disk: every tile,
		// or just `only` (the rest kept from the existing files). The
		// distant tiles follow. False with `error` when a file could not
		// be read or written.
		bool BakeOverview(const std::vector<std::pair<int32, int32> >* only, std::string &error);
		bool HasOverview() const { return overviewLoaded; }

		// One tile's subtree JSON, from the template. Any thread.
		static std::string BuildTileJson(const Settings &settings, const std::string &tileTemplate, const std::string &assetRoot,
			const int32 x, const int32 z);

		virtual void Register(SceneGraph* Scene);
		virtual void Unregister(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual uint32 GetComponentType() const { return ComponentType::Terrain; }

	private:
		typedef std::pair<int32, int32> TileKey;

		struct Tile
		{
			enum State { Unloaded, Loading, Loaded };
			State state = Unloaded;
			AssetStreamer::Ticket ticket = 0;
			std::shared_ptr<GameObject> root;
			std::shared_ptr<LoadedSceneAssets> assets;
			// What was dug under it, and the walls drawn from that.
			std::shared_ptr<CaveVolume> caves;
			std::shared_ptr<GameObject> caveObject;
			// The stand-in drawn while the tile is not loaded.
			std::shared_ptr<GameObject> distant;
			uint32 distantVersion = 0;	// the overview it was cut from
			bool distantShown = false;
			// A far block is drawn in its place (see FarBlock).
			bool merged = false;
		};

		// Far off, distant tiles are not drawn one by one: kFarBlockTiles x
		// kFarBlockTiles of them are one mesh, a block, at a quarter of the
		// detail. A terrain of a thousand tiles seen from the middle is then
		// some tens of draws and not several hundred.
		struct FarBlock
		{
			std::shared_ptr<GameObject> object;
			uint32 version = 0;		// the overview it was cut from
			bool beyond = false;	// far enough from every focus to be drawn as one
			bool shown = false;
			bool leaving = false;	// near again: drawn until its tiles have their own stand-ins
		};
		std::vector<FarBlock> farBlocks;
		int32 farBlocksX = 0, farBlocksZ = 0;
		FarBlock* BlockOf(const int32 x, const int32 z);
		void BuildFarBlock(const int32 bx, const int32 bz);
		void UpdateFarBlocks(const std::vector<Vec3> &foci, const Vec3 &origin);

		Tile &At(const int32 x, const int32 z) { return tiles[(size_t)z * settings.tilesX + x]; }
		const Tile &At(const int32 x, const int32 z) const { return tiles[(size_t)z * settings.tilesX + x]; }
		f32 DistanceToTile(const Vec3 &local, const int32 x, const int32 z) const;
		void Stream(const std::vector<Vec3> &foci);
		void RequestLoad(const int32 x, const int32 z, const f32 distance);
		void Finish(const int32 x, const int32 z, const std::shared_ptr<void> &prepared);
		void Unload(const int32 x, const int32 z);
		void ReadTemplate();
		void CollectGraveyard(const bool all);

		// Overview.
		std::string OverviewPath(const bool colour) const;
		bool LoadOverview();
		void EnsureDistantMaterial();
		void BuildDistant(const int32 x, const int32 z);
		void DropDistant(Tile &tile);
		void SyncDistant(Tile &tile);
		void UpdateDistant(const std::vector<Vec3> &foci);

		Settings settings;
		std::shared_ptr<const std::string> templateText;
		std::string assetRoot;
		IPhysics* physics;
		sol::state* lua;
		SceneGraph* scene = NULL;
		f32 heightScale = 1.f, heightOffset = 0.f, tileSkirt = 2.f;
		std::vector<Tile> tiles;
		UnloadVeto unloadVeto;
		TileCallback onLoaded, onUnloading;

		struct Grave
		{
			std::shared_ptr<LoadedSceneAssets> assets;
			std::shared_ptr<GameObject> object;
			uint32 updatesLeft = 0;
		};
		std::deque<Grave> graveyard;

		// The overview grid, in metres: side x side points, row-major along
		// z, covering the whole terrain (padded to a square).
		bool overviewTried = false, overviewLoaded = false;
		uint32 overviewSide = 0;
		std::vector<f32> overviewHeights;
		uint32 overviewVersion = 1;
		std::shared_ptr<IMaterial> distantMaterial;
		std::shared_ptr<IMaterial> caveMaterial;
		f32 caveVoxel = 2.f;
		int32 caveCells = 128;
		std::string CavePath(const int32 x, const int32 z) const;
		uint32 distantCursor = 0;
	};

}

#endif /* TERRAINCOMPONENT_H */
