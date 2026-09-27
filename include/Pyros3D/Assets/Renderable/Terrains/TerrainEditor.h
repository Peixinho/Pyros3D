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

		// Every terrain tile in the scene (at any depth - streamed cells are
		// children of their cell's root).
		static std::vector<TerrainTile> FindTiles(SceneGraph* scene);

		// Ground height under a world position, from whichever tile holds
		// it. False when no tile does.
		static bool HeightAt(SceneGraph* scene, const f32 x, const f32 z, f32 &height);

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

		// The stroke is over: rebuild collision and regrow foliage on every
		// tile it touched.
		void FinishStroke();

		// Writes every tile edited since the last Save(): heightmaps as
		// 16-bit PNG, splat and density maps as 8-bit, under the asset root.
		// A density
		// map a layer did not have yet is created next to its heightmap.
		// Returns false if any file failed.
		bool Save();

		// Per tile, the splat map being painted and the density maps, loaded
		// on first touch.
		struct TileState
		{
			GameObject* owner = NULL;
			bool heightsDirty = false, collisionDirty = false, splatDirty = false;
			std::vector<bool> foliageDirty;
			std::shared_ptr<PaintableImage> splat;
			std::string splatPath;
		};

	private:
		TileState &State(const TerrainTile &tile);
		std::shared_ptr<PaintableImage> SplatImage(const TerrainTile &tile, TileState &state);
		std::string Resolve(const std::string &path) const;
		std::vector<TileState> states;
		std::vector<TerrainTile> strokeTiles;
		SceneGraph* strokeScene = NULL;
		std::string assetRoot;
	};

}

#endif /* TERRAINEDITOR_H */
