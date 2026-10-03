//============================================================================
// Name        : TerrainTools.h
// Author      : Duarte Peixinho
// Description : The Scene View's terrain brushes. A fourth viewport tool
//               beside the gizmo: while it is on, the left button sculpts,
//               paints the splat map or paints foliage density on whatever
//               terrain tile is under the cursor, through the engine's
//               TerrainEditor - the same code a game calls.
//
//               A stroke runs from press to release: every frame it applies
//               the brush where the cursor meets the ground (rates are per
//               second, so a held brush builds up the same at any frame
//               rate), and the release finishes it - collision rebuilt,
//               foliage regrown - and hands back one undo entry.
//
//               What the brushes edit is not in the scene's JSON: heights,
//               splat and density maps are PNGs next to it, written by
//               Save() when the scene is saved.
//============================================================================

#ifndef TERRAINTOOLS_H
#define TERRAINTOOLS_H

#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace p3d { class SceneGraph; class GameObject; class DebugRenderer; }

class TerrainTools
{
public:
	enum Tool { Raise, Lower, Smooth, Flatten, PaintTexture, PaintFoliage, Place, Hole, Fill, Dig, Pack, CaveSmooth, CaveLevel, ToolCount };
	// The radius the cave tools really work with: a cave is made of voxels,
	// and a sphere much smaller than one and a half of them is nothing the
	// voxels can hold - the brush then touched the rock without ever
	// opening it.
	float CaveRadius() const;
	// The two that work in depth: a sphere at the cursor, not a disc on the ground.
	bool IsCaveTool() const { return tool == Dig || tool == Pack || tool == CaveSmooth || tool == CaveLevel; }
	static const char* ToolName(const Tool t);
	static bool ToolFromName(const std::string &name, Tool &out);

	bool active = false;
	Tool tool = Raise;
	float radius = 8.f;			// metres
	float strength = 0.5f;		// 0..1, scaled per tool to a per-second rate
	float hardness = 0.5f;		// 0 = soft edge, 1 = hard disc
	int layer = 0;				// splat channel or foliage layer
	float density = 1.f;		// what a foliage stroke paints toward
	// Place: one object from placeAsset (a .p3dm or .prefab, project-
	// relative) per placeSpacing metres of stroke, on the ground.
	std::string placeAsset;
	float placeSpacing = 8.f;
	float placeScaleMin = 0.8f, placeScaleMax = 1.2f;
	bool placeRandomYaw = true;
	bool placeAlign = false;		// tilt to the ground's slope

	void SetAssetRoot(const std::string &root) { editor.SetAssetRoot(root); }
	// Which tiles a stroke may change - see TerrainEditor::SetEditable. A
	// streamed world's far versions are ground to aim at, never to edit.
	void SetEditable(const p3d::TerrainEditor::TileFilter &fn) { editor.SetEditable(fn); }
	// A new scene: every tile known so far is gone. Settings stay.
	void ResetScene() { editor = p3d::TerrainEditor(); stroking = false; hoverValid = false; hoverEditable = false; }

	// The cursor's ray, every frame the tool is on: finds the ground under
	// it and, during a stroke, applies the brush there for dt seconds.
	void Update(p3d::SceneGraph* scene, const bool rayValid, const Vec3 &origin, const Vec3 &direction, const float dt);
	bool HoverValid() const { return hoverValid; }
	// False while the ground under the cursor is a far version: the cell is
	// not loaded (yet), and the brush does nothing there until it is.
	bool HoverEditable() const { return hoverValid && hoverEditable; }
	// Whether the cursor is on a cave's wall rather than on the ground:
	// the hole tools then cut the cave, not the terrain.
	bool HoverOnCave() const { return hoverValid && hoverOnCave; }
	bool IsHoleTool() const { return tool == Hole || tool == Fill; }
	const Vec3 &HoverPoint() const { return hover; }

	// Press: starts a stroke if the cursor is on terrain (false otherwise -
	// the click is then the viewport's as usual).
	bool BeginStroke(p3d::SceneGraph* scene);
	bool Stroking() const { return stroking; }
	// One dab at a world position, for dt seconds - what Update() does at
	// the cursor, and what a scripted stroke does along its points.
	bool ApplyAt(p3d::SceneGraph* scene, const float x, const float z, const float dt);
	// The cave tools' dab: a sphere at a world position.
	// dt seconds of it: the cave brushes work at a rate set by strength.
	bool ApplyAt3D(p3d::SceneGraph* scene, const Vec3 &centre, const float dt);
	// The height Level Floor levels to: where the stroke began, unless set.
	void SetLevel(const float y) { flattenTarget = y; }
	float Level() const { return flattenTarget; }
	// Noise tunnels around a point - one undo entry, taken like a stroke's
	// (BeginStroke / EndStroke around it).
	uint32_t GenerateCaves(p3d::SceneGraph* scene, const Vec3 &centre, const float radius, const uint32_t seed, const float size,
		const float width, const float minDepth, const float maxDepth)
	{ return editor.GenerateCaves(scene, centre, radius, seed, size, width, minDepth, maxDepth); }
	uint32_t ResyncCaves(p3d::SceneGraph* scene) { return editor.ResyncCaves(scene); }
	// Starts recording without a cursor (a scripted or generated edit).
	void BeginRecording() { stroking = true; editor.BeginStroke(); }
	// Release: finishes the stroke. True with the tiles' states before and
	// after when it changed anything - the undo entry.
	bool EndStroke(std::vector<p3d::TerrainEditor::TileSnapshot> &before, std::vector<p3d::TerrainEditor::TileSnapshot> &after);
	// Ground height and normal at a world position.
	static bool GroundPoint(p3d::SceneGraph* scene, const float x, const float z, float &height, Vec3 &normal);
	void Restore(p3d::SceneGraph* scene, const std::vector<p3d::TerrainEditor::TileSnapshot> &snapshots) { editor.Restore(scene, snapshots); }

	// Writes every edited tile's maps. False if any file failed.
	bool Save() { return editor.Save(); }
	// Tiles under `root` with edits Save() has not written.
	bool HasUnsaved(const p3d::GameObject* root) const;
	uint32_t UnsavedCount(p3d::SceneGraph* scene) const;
	// The tiles Save() is about to write.
	std::vector<const p3d::GameObject*> UnsavedOwners() const { return editor.UnsavedOwners(); }
	// `root` is leaving the scene.
	void Forget(p3d::GameObject* root);

	// The brush outline, draped over the ground.
	void DrawOverlay(p3d::DebugRenderer* debug, p3d::SceneGraph* scene) const;

	// New terrain: tilesX x tilesZ tiles of tileSize metres, each a blank
	// 16-bit heightmap (flat at height 0, able to go heightRange/4 below it
	// and 3/4 above), a splat map all on layer 0, and a splat material over
	// four ground textures. Files go under assets/terrain/<name>/ in the
	// project; the scene subtree to add comes back as JSON text.
	struct CreateParams
	{
		std::string name = "Terrain";
		int tilesX = 1, tilesZ = 1;
		float tileSize = 128.f;
		int samples = 129;
		float heightRange = 200.f;
		Vec3 origin;	// where the terrain's centre goes
		// Import: heights from this square image (absolute path; 8 or
		// 16-bit, first channel) stretched over the whole terrain - image
		// row 0 along z = 0 - black at baseHeight, white heightRange above.
		// Steep ground starts out painted with the rock layer.
		std::string importPath;
		float baseHeight = 0.f;
		// Generated ground, when nothing is imported: "flat", "perlin"
		// (rolling hills: fractal Perlin noise) or "ridged" (mountains:
		// the same noise folded into sharp ridges). Features are about
		// featureSize metres across; each of `octaves` layers halves that
		// and scales its height by roughness; `amount` is how much of
		// heightRange the result spans. The same seed makes the same
		// ground. Steep ground starts out painted with the rock layer.
		std::string generator = "flat";
		uint32_t seed = 1;
		float featureSize = 600.f;
		int octaves = 5;
		float roughness = 0.5f;
		float amount = 0.6f;
	};
	static bool CreateTerrain(const CreateParams &params, const std::string &projectRoot, std::string &subtreeJson, std::string &error);

	// A scene saved before terrains were objects keeps each tile as an
	// object of its own - under a root, or one per cell of a streamed
	// world. This rewrites the files on disk: every such set of tiles (one
	// per maps directory) becomes a Terrain object in the scene file, made
	// from the first tile's settings, and the tile objects - with their
	// cells' terrain far versions - are removed. The maps are not touched.
	// `report` says what it did ("terrains": [{name, tilesX, tilesZ, ...}]).
	// Tiles that differ from the first (another material, other foliage)
	// stop it unless `force`. The caller reloads the scene.
	static bool ConvertSceneTerrains(const std::string &scenePath, const std::string &projectRoot, const bool force,
		nlohmann::json &report, std::string &error);

	// Adds a grass foliage layer to every terrain tile in a subtree's JSON
	// (SnapshotSubtree's form), writing the blade texture into the project
	// if it is not there yet. Returns how many tiles got it.
	static int AddGrassLayer(nlohmann::json &subtree, const std::string &projectRoot, std::string &error);

	nlohmann::json State(p3d::SceneGraph* scene) const;

private:
	bool Raycast(const std::vector<p3d::TerrainTile> &tiles, const Vec3 &origin, const Vec3 &direction, Vec3 &hit) const;

	p3d::TerrainEditor editor;
	bool hoverValid = false;
	bool hoverEditable = false;
	bool hoverOnCave = false;
	Vec3 hover;
	bool stroking = false;
	float flattenTarget = 0.f;
	// Cave dabs remesh a tile: applied thirty times a second, not every frame.
	float caveDt = 0.f;
};

#endif
