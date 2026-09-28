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
	enum Tool { Raise, Lower, Smooth, Flatten, PaintTexture, PaintFoliage, Place, ToolCount };
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
	// A new scene: every tile known so far is gone. Settings stay.
	void ResetScene() { editor = p3d::TerrainEditor(); stroking = false; hoverValid = false; }

	// The cursor's ray, every frame the tool is on: finds the ground under
	// it and, during a stroke, applies the brush there for dt seconds.
	void Update(p3d::SceneGraph* scene, const bool rayValid, const Vec3 &origin, const Vec3 &direction, const float dt);
	bool HoverValid() const { return hoverValid; }
	const Vec3 &HoverPoint() const { return hover; }

	// Press: starts a stroke if the cursor is on terrain (false otherwise -
	// the click is then the viewport's as usual).
	bool BeginStroke(p3d::SceneGraph* scene);
	bool Stroking() const { return stroking; }
	// One dab at a world position, for dt seconds - what Update() does at
	// the cursor, and what a scripted stroke does along its points.
	bool ApplyAt(p3d::SceneGraph* scene, const float x, const float z, const float dt);
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
		Vec3 origin;
		// Import: heights from this square image (absolute path; 8 or
		// 16-bit, first channel) stretched over the whole terrain - image
		// row 0 along z = 0 - black at baseHeight, white heightRange above.
		// Steep ground starts out painted with the rock layer.
		std::string importPath;
		float baseHeight = 0.f;
	};
	static bool CreateTerrain(const CreateParams &params, const std::string &projectRoot, std::string &subtreeJson, std::string &error);

	// Adds a grass foliage layer to every terrain tile in a subtree's JSON
	// (SnapshotSubtree's form), writing the blade texture into the project
	// if it is not there yet. Returns how many tiles got it.
	static int AddGrassLayer(nlohmann::json &subtree, const std::string &projectRoot, std::string &error);

	nlohmann::json State(p3d::SceneGraph* scene) const;

private:
	bool Raycast(const std::vector<p3d::TerrainTile> &tiles, const Vec3 &origin, const Vec3 &direction, Vec3 &hit) const;

	p3d::TerrainEditor editor;
	bool hoverValid = false;
	Vec3 hover;
	bool stroking = false;
	float flattenTarget = 0.f;
};

#endif
