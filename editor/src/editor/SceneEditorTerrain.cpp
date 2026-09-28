// The Scene View's terrain brushes: the SceneEditor half of TerrainTools -
// the cursor ray, the stroke's press/drag/release, undo, saving, the
// Terrain panel and the terrain_* agent commands.

#include "SceneEditor.h"
#include "EditorWorld.h"
#include "SceneCommands.h"
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>

#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <cmath>

namespace {
	FoliageComponent* FindFoliage(GameObject* go)
	{
		if (!go) return NULL;
		for (size_t c = 0; c < go->GetComponents().size(); c++)
			if (FoliageComponent* fc = dynamic_cast<FoliageComponent*>(go->GetComponents()[c].get())) return fc;
		return NULL;
	}

	const HeightfieldData* TileGround(GameObject* go)
	{
		if (!go) return NULL;
		for (size_t c = 0; c < go->GetComponents().size(); c++)
			if (RenderingComponent* rc = dynamic_cast<RenderingComponent*>(go->GetComponents()[c].get()))
				if (Heightfield* hf = dynamic_cast<Heightfield*>(rc->GetRenderable())) return hf->GetData().get();
		return NULL;
	}

	// Whether two specs grow different instances (as opposed to only
	// drawing the same ones differently - distances, shadows).
	bool GrowsDifferently(const FoliageLayerSpec &a, const FoliageLayerSpec &b)
	{
		return a.density != b.density || a.blockSize != b.blockSize || a.minScale != b.minScale || a.maxScale != b.maxScale
			|| a.tintLow != b.tintLow || a.tintHigh != b.tintHigh || a.maxSlopeDegrees != b.maxSlopeDegrees
			|| a.minHeight != b.minHeight || a.maxHeight != b.maxHeight || a.alignToGround != b.alignToGround
			|| a.sink != b.sink || a.seed != b.seed || a.densityMap != b.densityMap;
	}

	json SpecJson(const FoliageLayerSpec &s)
	{
		return {
			{ "name", s.name }, { "density", s.density }, { "blockSize", s.blockSize },
			{ "minScale", s.minScale }, { "maxScale", s.maxScale },
			{ "tintLow", { s.tintLow.x, s.tintLow.y, s.tintLow.z, s.tintLow.w } },
			{ "tintHigh", { s.tintHigh.x, s.tintHigh.y, s.tintHigh.z, s.tintHigh.w } },
			{ "maxSlope", s.maxSlopeDegrees }, { "minHeight", s.minHeight }, { "maxHeight", s.maxHeight },
			{ "alignToGround", s.alignToGround }, { "sink", s.sink }, { "seed", s.seed },
			{ "fullDistance", s.fullDistance }, { "fadeDistance", s.fadeDistance }, { "shadowDistance", s.shadowDistance },
			{ "lodDistance", s.lodDistance }, { "castShadows", s.castShadows }, { "densityMap", s.densityMap } };
	}

	// The keys set_foliage_layer takes - the scene file's own names.
	void PatchSpec(FoliageLayerSpec &s, const json &j)
	{
		auto vec4 = [&j](const char* k, Vec4 &v) {
			if (j.contains(k) && j[k].is_array() && j[k].size() >= 3)
				v = Vec4(j[k][0].get<f32>(), j[k][1].get<f32>(), j[k][2].get<f32>(), j[k].size() > 3 ? j[k][3].get<f32>() : 1.f);
		};
		s.name = j.value("layerName", s.name);	// "name" is the object
		s.density = std::max(0.f, j.value("density", s.density));
		s.blockSize = std::max(1.f, j.value("blockSize", s.blockSize));
		s.minScale = std::max(0.01f, j.value("minScale", s.minScale));
		s.maxScale = std::max(s.minScale, j.value("maxScale", s.maxScale));
		vec4("tintLow", s.tintLow);
		vec4("tintHigh", s.tintHigh);
		s.maxSlopeDegrees = j.value("maxSlope", s.maxSlopeDegrees);
		s.minHeight = j.value("minHeight", s.minHeight);
		s.maxHeight = j.value("maxHeight", s.maxHeight);
		s.alignToGround = std::min(1.f, std::max(0.f, j.value("alignToGround", s.alignToGround)));
		s.sink = j.value("sink", s.sink);
		s.seed = j.value("seed", s.seed);
		s.fullDistance = std::max(0.f, j.value("fullDistance", s.fullDistance));
		s.fadeDistance = std::max(s.fullDistance, j.value("fadeDistance", s.fadeDistance));
		s.shadowDistance = std::max(0.f, j.value("shadowDistance", s.shadowDistance));
		s.lodDistance = std::max(0.f, j.value("lodDistance", s.lodDistance));
		s.castShadows = j.value("castShadows", s.castShadows);
	}

	SceneObject* FindGameObjectNamed(SceneObjects* objects, const std::string &name)
	{
		for (std::map<uint32, SceneObject*>::const_iterator i = objects->GetList().begin(); i != objects->GetList().end(); ++i)
			if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->second->GetName() == name) return i->second;
		return NULL;
	}
}


namespace {
	// One brush stroke. Holds every touched tile's state before and after,
	// which is the real cost - so it reports it.
	class TerrainStrokeCommand : public IUndoableCommand
	{
	public:
		TerrainStrokeCommand(SceneEditor* editor, TerrainTools* tools, SceneGraph* scene,
			std::vector<TerrainEditor::TileSnapshot> &before, std::vector<TerrainEditor::TileSnapshot> &after,
			const std::string &what)
			: editor(editor), tools(tools), scene(scene), what(what)
		{
			this->before.swap(before);
			this->after.swap(after);
		}
		void Undo() override { tools->Restore(scene, before); editor->MarkSceneDirty(); }
		void Redo() override { tools->Restore(scene, after); editor->MarkSceneDirty(); }
		std::string Description() const override { return what; }
		size_t MemoryCost() const override
		{
			size_t n = sizeof(*this);
			for (int pass = 0; pass < 2; pass++)
			{
				const std::vector<TerrainEditor::TileSnapshot> &v = pass ? after : before;
				for (size_t i = 0; i < v.size(); i++)
				{
					n += v[i].heights.size() * sizeof(f32) + v[i].splat.size();
					for (size_t l = 0; l < v[i].density.size(); l++) n += v[i].density[l].size();
				}
			}
			return n;
		}
	private:
		SceneEditor* editor;
		TerrainTools* tools;
		SceneGraph* scene;
		std::vector<TerrainEditor::TileSnapshot> before, after;
		std::string what;
	};

	const char* StrokeName(const TerrainTools::Tool t)
	{
		switch (t)
		{
		case TerrainTools::Raise: return "Raise Terrain";
		case TerrainTools::Lower: return "Lower Terrain";
		case TerrainTools::Smooth: return "Smooth Terrain";
		case TerrainTools::Flatten: return "Flatten Terrain";
		case TerrainTools::PaintTexture: return "Paint Terrain Texture";
		case TerrainTools::PaintFoliage: return "Paint Foliage";
		default: return "Terrain Stroke";
		}
	}
}

TerrainTools &SceneEditor::Terrain()
{
	if (!terrainTools) terrainTools.reset(new TerrainTools());
	// Where the scene's relative map paths resolve - asked for on every use,
	// since the project can change under a live SceneEditor.
	if (project && project->IsOpen()) terrainTools->SetAssetRoot(project->GetProjectPath());
	return *terrainTools;
}

bool SceneEditor::IsTerrainMode() const
{
	return terrainTools && terrainTools->active;
}

void SceneEditor::SetTerrainMode(const bool on)
{
	TerrainTools &t = Terrain();
	if (t.active == on) return;
	if (t.Stroking()) EndTerrainStroke();
	t.active = on;
	if (on)
	{
		// One viewport tool at a time.
		if (tilePaintMode) SetTilePaintMode(false);
		uiEditMode = false;
		gizmoDragging = false;
	}
}

bool SceneEditor::ViewportRay(Vec3 &origin, Vec3 &direction) const
{
	GameObject* viewCam = GetViewCameraGO();
	if (!viewCam || dim.x < 1.f || dim.y < 1.f) return false;
	const Matrix viewM = viewCam->GetWorldTransformation().Inverse();
	const Matrix projM = const_cast<Projection&>(isPerspective ? projection : projectionOrtho).GetProjectionMatrix();
	const Matrix inv = (projM * viewM).Inverse();
	const f32 x = viewportMouse.x / dim.x * 2.f - 1.f;
	const f32 y = 1.f - viewportMouse.y / dim.y * 2.f;
	// Two points on the ray - whichever depth convention the projection
	// uses, both lie on the line through the cursor.
	Vec4 a = inv * Vec4(x, y, -1.f, 1.f);
	Vec4 b = inv * Vec4(x, y, 1.f, 1.f);
	if (std::fabs(a.w) < 1e-12f || std::fabs(b.w) < 1e-12f) return false;
	const Vec3 pa(a.x / a.w, a.y / a.w, a.z / a.w), pb(b.x / b.w, b.y / b.w, b.z / b.w);
	direction = (pb - pa).normalize();
	// From the camera for a perspective view; from the near plane for an
	// orthographic one, where the "eye" is at infinity.
	origin = isPerspective ? viewCam->GetWorldPosition() : pa;
	return true;
}

void SceneEditor::UpdateTerrainBrush()
{
	if (!IsTerrainMode()) return;
	Terrain();
	if (playMode || sceneIsTwoD)
	{
		if (terrainTools->Stroking()) EndTerrainStroke();
		terrainTools->Update(scene, false, Vec3(), Vec3(), 0.f);
		return;
	}
	Vec3 o, d;
	const bool ok = viewportMouseValid && ViewportRay(o, d);
	terrainTools->Update(scene, ok, o, d, ImGui::GetIO().DeltaTime);
}

void SceneEditor::EndTerrainStroke()
{
	if (!terrainTools || !terrainTools->Stroking()) return;
	std::vector<TerrainEditor::TileSnapshot> before, after;
	if (!terrainTools->EndStroke(before, after)) return;
	sceneUndo.Push(std::unique_ptr<IUndoableCommand>(new TerrainStrokeCommand(this, terrainTools.get(), scene,
		before, after, StrokeName(terrainTools->tool))));
	MarkSceneDirty();
}

bool SceneEditor::SaveTerrain()
{
	if (!terrainTools) return true;
	if (project && project->IsOpen()) terrainTools->SetAssetRoot(project->GetProjectPath());
	if (terrainTools->Save()) return true;
	echo("ERROR: saving terrain - a heightmap, splat or density map could not be written");
	return false;
}

void SceneEditor::ShowTerrainPanel()
{
	if (!IsTerrainMode() || sceneIsTwoD) return;
	TerrainTools &t = *terrainTools;
	ImGui::SetNextWindowSize(ImVec2(300, 420), ImGuiCond_FirstUseEver);
	bool open = true;
	if (!ImGui::Begin("Terrain", &open))
	{
		ImGui::End();
		if (!open) SetTerrainMode(false);
		return;
	}

	static const char* labels[TerrainTools::ToolCount] = { "Raise", "Lower", "Smooth", "Flatten", "Texture", "Foliage" };
	ImGui::TextDisabled("Sculpt");
	for (int i = 0; i < TerrainTools::ToolCount; i++)
	{
		// Two to a row, so a narrow panel does not clip the last one.
		if (i == TerrainTools::PaintTexture) ImGui::TextDisabled("Paint");
		else if (i % 2 == 1) ImGui::SameLine();
		if (i == TerrainTools::PaintFoliage) ImGui::SameLine();
		if (ImGui::RadioButton(labels[i], t.tool == i)) t.tool = (TerrainTools::Tool)i;
	}
	ImGui::Separator();
	ImGui::SliderFloat("Radius", &t.radius, 0.5f, 200.f, "%.1f m", ImGuiSliderFlags_Logarithmic);
	ImGui::SliderFloat("Strength", &t.strength, 0.01f, 1.f, "%.2f");
	ImGui::SliderFloat("Hardness", &t.hardness, 0.f, 1.f, "%.2f");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("0: fades from the centre to the edge. 1: a hard disc.");
	if (t.tool == TerrainTools::Flatten)
		ImGui::TextWrapped("Levels the ground to the height where the stroke starts.");
	if (t.tool == TerrainTools::PaintTexture)
	{
		static const char* layers[] = { "0  Grass", "1  Dirt", "2  Rock", "3  Sand" };
		t.layer = std::max(0, std::min(3, t.layer));
		ImGui::Combo("Layer", &t.layer, layers, 4);
		ImGui::TextWrapped("Paints the tile's splat map. Layer textures are the splat material's layer0..layer3 samplers.");
	}
	if (t.tool == TerrainTools::PaintFoliage)
	{
		ImGui::InputInt("Foliage layer", &t.layer);
		t.layer = std::max(0, t.layer);
		ImGui::SliderFloat("Density", &t.density, 0.f, 1.f, "%.2f");
		ImGui::TextWrapped("0 clears the layer, 1 grows it at its full density. Tiles need a Foliage component with that layer.");
	}

	ImGui::Separator();
	const nlohmann::json state = t.State(scene);
	ImGui::Text("%u tiles, %u with unsaved edits", state.value("tiles", 0u), state.value("unsaved", 0u));
	if (t.HoverValid())
		ImGui::Text("Cursor: %.1f, %.1f, %.1f", t.HoverPoint().x, t.HoverPoint().y, t.HoverPoint().z);
	else
		ImGui::TextDisabled("Cursor: not over terrain");
	ImGui::TextDisabled("Edits are written with the scene (Save).");

	static json pendingCreate;
	static bool pendingGrass = false;
	static std::string lastError;
	if (ImGui::CollapsingHeader("Grass Preset##terrain_foliage"))
	{
		ImGui::TextWrapped("Adds a grass layer to every tile of the selected terrain. Paint where it grows with the Foliage tool.");
		if (ImGui::Button("Add Grass to Selection")) pendingGrass = true;
	}
	if (ImGui::CollapsingHeader("Create Terrain##terrain_create"))
	{
		static TerrainTools::CreateParams params;
		static std::string name = "Terrain";
		static int samplesIndex = 1;
		static const char* sampleLabels[] = { "65", "129", "257", "513" };
		static const int sampleValues[] = { 65, 129, 257, 513 };
		ImGui::InputText("Name", &name);
		ImGui::InputInt("Tiles X", &params.tilesX);
		ImGui::InputInt("Tiles Z", &params.tilesZ);
		params.tilesX = std::max(1, std::min(32, params.tilesX));
		params.tilesZ = std::max(1, std::min(32, params.tilesZ));
		ImGui::InputFloat("Tile size (m)", &params.tileSize, 16.f, 64.f, "%.0f");
		ImGui::Combo("Samples", &samplesIndex, sampleLabels, 4);
		ImGui::InputFloat("Height range (m)", &params.heightRange, 10.f, 50.f, "%.0f");
		if (ImGui::Button("Create"))
		{
			params.name = name;
			params.samples = sampleValues[samplesIndex];
			json args;
			args["name"] = params.name;
			args["tilesX"] = params.tilesX;
			args["tilesZ"] = params.tilesZ;
			args["tileSize"] = params.tileSize;
			args["samples"] = params.samples;
			args["heightRange"] = params.heightRange;
			pendingCreate = args;
			lastError.clear();
		}
		if (!lastError.empty()) ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", lastError.c_str());
	}
	ImGui::End();
	if (!open) SetTerrainMode(false);
	// After End(): building objects inside another window's Begin/End pair
	// is how ImGui asserts.
	if (pendingGrass)
	{
		pendingGrass = false;
		json out;
		lastError.clear();
		AgentTerrain("terrain_add_grass", json::object(), out, lastError);
	}
	if (!pendingCreate.is_null())
	{
		json args, out;
		args.swap(pendingCreate);
		if (!AgentTerrain("terrain_create", args, out, lastError) && lastError.empty()) lastError = "failed";
	}
}

bool SceneEditor::AgentTerrain(const std::string& command, const json& a, json& out, std::string& errOut)
{
	TerrainTools &t = Terrain();
	const json args = a.is_object() ? a : json::object();

	if (command == "terrain_state")
	{
		out = t.State(scene);
		if (args.contains("x") && args.contains("z"))
		{
			f32 h;
			if (TerrainEditor::HeightAt(scene, args.value("x", 0.f), args.value("z", 0.f), h)) out["height"] = h;
			else out["height"] = nullptr;
		}
		return true;
	}
	// The panel's controls: {"on":true,"tool":"raise","radius":8,...}.
	if (command == "terrain_brush")
	{
		if (args.contains("tool"))
		{
			TerrainTools::Tool tool;
			if (!TerrainTools::ToolFromName(args.value("tool", std::string()), tool))
			{ errOut = "tool must be raise, lower, smooth, flatten, paint or foliage"; return false; }
			t.tool = tool;
		}
		t.radius = std::max(0.1f, args.value("radius", t.radius));
		t.strength = std::min(1.f, std::max(0.f, args.value("strength", t.strength)));
		t.hardness = std::min(1.f, std::max(0.f, args.value("hardness", t.hardness)));
		t.layer = std::max(0, args.value("layer", t.layer));
		t.density = std::min(1.f, std::max(0.f, args.value("density", t.density)));
		if (args.contains("on"))
		{
			if (args.value("on", false) && (sceneIsTwoD || playMode)) { errOut = "terrain brushes need a 3D scene outside play"; return false; }
			SetTerrainMode(args.value("on", false));
		}
		out = t.State(scene);
		return true;
	}
	// A stroke along world points [[x,z],...], dt seconds of brush at each -
	// the same begin / dab / finish the mouse runs, and one undo entry.
	if (command == "terrain_stroke")
	{
		if (playMode || sceneIsTwoD) { errOut = "terrain brushes need a 3D scene outside play"; return false; }
		if (!args.contains("points") || !args["points"].is_array() || args["points"].empty())
		{ errOut = "'points' ([[x,z],...]) is required"; return false; }
		if (t.Stroking()) EndTerrainStroke();
		const f32 dt = args.value("dt", 1.f / 60.f);
		const json &pts = args["points"];
		const f32 x0 = pts[0].at(0).get<f32>(), z0 = pts[0].at(1).get<f32>();
		f32 h0;
		if (!TerrainEditor::HeightAt(scene, x0, z0, h0)) { errOut = "the first point is not over terrain"; return false; }
		// Stand in for the cursor at the first point, so Flatten picks up
		// its height exactly as a click there would.
		const bool wasActive = t.active;
		t.active = true;
		t.Update(scene, true, Vec3(x0, h0 + 1000.f, z0), Vec3(0.f, -1.f, 0.f), 0.f);
		if (!t.BeginStroke(scene)) { t.active = wasActive; errOut = "could not start a stroke there"; return false; }
		uint32 dabs = 0;
		for (size_t i = 0; i < pts.size(); i++)
			if (t.ApplyAt(scene, pts[i].at(0).get<f32>(), pts[i].at(1).get<f32>(), dt)) dabs++;
		const size_t undoBefore = sceneUndo.UndoCount();
		EndTerrainStroke();
		t.active = wasActive;
		out = t.State(scene);
		out["dabs"] = dabs;
		out["undoEntry"] = sceneUndo.UndoCount() > undoBefore;
		return true;
	}
	if (command == "terrain_create")
	{
		if (playMode) { errOut = "stop play mode first"; return false; }
		if (sceneIsTwoD) { errOut = "terrain needs a 3D scene"; return false; }
		if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
		TerrainTools::CreateParams p;
		p.name = args.value("name", p.name);
		p.tilesX = args.value("tilesX", p.tilesX);
		p.tilesZ = args.value("tilesZ", p.tilesZ);
		p.tileSize = args.value("tileSize", p.tileSize);
		p.samples = args.value("samples", p.samples);
		p.heightRange = args.value("heightRange", p.heightRange);
		if (args.contains("origin") && args["origin"].is_array() && args["origin"].size() >= 3)
			p.origin = Vec3(args["origin"][0].get<f32>(), args["origin"][1].get<f32>(), args["origin"][2].get<f32>());
		std::string subtree;
		if (!TerrainTools::CreateTerrain(p, project->GetProjectPath(), subtree, errOut)) return false;
		if (editorWorld)
		{
			// A streamed world keeps its content in cells: each tile goes to
			// the cell under its centre, loaded or not (see MoveObjectToCell).
			// Tiles the size of a cell line up with them exactly.
			const json tree = json::parse(subtree);
			const json &tiles = tree["root"]["children"];
			uint32 placed = 0;
			for (size_t i = 0; i < tiles.size(); i++)
			{
				json one;
				one["root"] = tiles[i];
				one["root"]["position"] = { p.origin.x + tiles[i]["position"][0].get<f32>(), p.origin.y + tiles[i]["position"][1].get<f32>(),
					p.origin.z + tiles[i]["position"][2].get<f32>() };
				one["materials"] = tree["materials"];
				SceneObject* tile = RawInsertSubtree(one.dump(), 0, false, EditorCameraSettings(), true);
				if (!tile) { errOut = "a tile could not be built"; return false; }
				const Vec3 centre = ((GameObject*)tile->GetPTR())->GetWorldPosition() + Vec3(p.tileSize * 0.5f, 0.f, p.tileSize * 0.5f);
				int32 cx, cz;
				editorWorld->CellOf(centre, cx, cz);
				if (!MoveObjectToCell(tile->GetID(), cx, cz, errOut)) return false;
				placed++;
			}
			if (p.tileSize != editorWorld->CellSize())
				echo("WARNING: terrain tiles of " + std::to_string((int)p.tileSize) + " m in cells of " + std::to_string((int)editorWorld->CellSize())
					+ " m - a tile streams with the cell under its centre; tiles the size of a cell line up exactly");
			MarkSceneDirty();
			out = t.State(scene);
			out["tilesPlaced"] = placed;
			return true;
		}
		SceneObject* obj = RawInsertSubtree(subtree, 0, false, EditorCameraSettings(), true);
		if (!obj) { errOut = "the terrain's maps were written, but it could not be built"; return false; }
		PushAddCommand(obj);
		SelectSceneObject(obj);
		MarkSceneDirty();
		out = t.State(scene);
		out["name"] = obj->GetName();
		return true;
	}
	// A grass layer on every terrain tile under an object (the selection
	// when no name is given) - one undo entry. Foliage painting then has a
	// layer to paint.
	if (command == "terrain_add_grass")
	{
		if (playMode) { errOut = "stop play mode first"; return false; }
		if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
		SceneObject* target = ResolveTerrainTarget(args, errOut);
		if (!target) return false;
		const uint32 id = target->GetID();
		const std::string before = SnapshotSubtree(id);
		json tree;
		try { tree = json::parse(before); }
		catch (const std::exception &e) { errOut = e.what(); return false; }
		const int tiles = TerrainTools::AddGrassLayer(tree, project->GetProjectPath(), errOut);
		if (tiles <= 0) return false;
		const bool wasCamera = IsSceneCamera(id);
		std::unique_ptr<ReplaceGameObjectCommand> cmd(new ReplaceGameObjectCommand(this, target->GetParentID(), before, tree.dump(),
			wasCamera, wasCamera ? sceneCameras[id] : EditorCameraSettings(), target->Helper != nullptr, id, "Add Grass"));
		cmd->Redo();
		sceneUndo.Push(std::move(cmd));
		MarkSceneDirty();
		out = t.State(scene);
		out["tilesWithGrass"] = tiles;
		return true;
	}
	// A model scattered as a foliage layer on every tile under an object:
	// {"name"|"id", "mesh": "assets/models/x.p3dm", <layer keys>?}.
	if (command == "add_foliage_layer")
	{
		if (playMode) { errOut = "stop play mode first"; return false; }
		SceneObject* target = ResolveTerrainTarget(args, errOut);
		if (!target) return false;
		const std::string meshRel = args.value("mesh", std::string());
		if (meshRel.empty() || !project || !std::filesystem::exists(project->AbsolutePath(meshRel)))
		{ errOut = "'mesh' must name a model in the project (assets/models/...p3dm)"; return false; }
		FoliageLayerSpec spec;
		spec.name = std::filesystem::path(meshRel).stem().string();
		spec.density = 0.01f;
		spec.blockSize = 64.f;
		spec.maxSlopeDegrees = 25.f;
		spec.sink = 0.1f;
		spec.fullDistance = 250.f;
		spec.fadeDistance = 400.f;
		spec.shadowDistance = 60.f;
		PatchSpec(spec, args);
		json layer = SpecJson(spec);
		layer.erase("densityMap");
		layer["mesh"] = { { "kind", "model" }, { "mergeMeshes", true }, { "path", meshRel } };

		const uint32 id = target->GetID();
		const std::string before = SnapshotSubtree(id);
		json tree;
		try { tree = json::parse(before); }
		catch (const std::exception &e) { errOut = e.what(); return false; }
		int tiles = 0;
		std::function<void(json &)> visit = [&](json &node) {
			if (!node.is_object()) return;
			json &comps = node["components"];
			bool isTile = false;
			json* foliage = NULL;
			for (size_t c = 0; comps.is_array() && c < comps.size(); c++)
			{
				if (comps[c].value("type", std::string()) == "RenderingComponent" && comps[c].contains("renderable")
					&& comps[c]["renderable"].is_object() && comps[c]["renderable"].value("kind", std::string()) == "heightfield") isTile = true;
				if (comps[c].value("type", std::string()) == "Foliage") foliage = &comps[c];
			}
			if (isTile)
			{
				json l = layer;
				l["seed"] = spec.seed + (uint32)tiles * 7919u;
				if (foliage) (*foliage)["layers"].push_back(l);
				else comps.push_back({ { "type", "Foliage" }, { "layers", json::array({ l }) } });
				tiles++;
			}
			if (node.contains("children") && node["children"].is_array())
				for (size_t i = 0; i < node["children"].size(); i++) visit(node["children"][i]);
		};
		visit(tree["root"]);
		if (tiles == 0) { errOut = "no terrain tiles there"; return false; }
		const bool wasCamera = IsSceneCamera(id);
		std::unique_ptr<ReplaceGameObjectCommand> cmd(new ReplaceGameObjectCommand(this, target->GetParentID(), before, tree.dump(),
			wasCamera, wasCamera ? sceneCameras[id] : EditorCameraSettings(), target->Helper != nullptr, id, "Add Foliage Layer"));
		cmd->Redo();
		sceneUndo.Push(std::move(cmd));
		MarkSceneDirty();
		out["tiles"] = tiles;
		return true;
	}
	// A tile's foliage layers, as the Properties panel shows them.
	if (command == "get_foliage")
	{
		SceneObject* target = ResolveTerrainTarget(args, errOut);
		if (!target) return false;
		FoliageComponent* fc = FindFoliage((GameObject*)target->GetPTR());
		out["layers"] = json::array();
		if (fc)
			for (size_t i = 0; i < fc->GetLayers().size(); i++)
			{
				json l = SpecJson(fc->GetLayers()[i].spec);
				uint32 instances = 0;
				for (size_t b = 0; b < fc->GetLayers()[i].counts.size(); b++) instances += fc->GetLayers()[i].counts[b];
				l["instances"] = instances;
				l["blocks"] = (uint32)fc->GetLayers()[i].blocks.size();
				out["layers"].push_back(l);
			}
		return true;
	}
	// {"name"|"id", "layer": i, <any layer key; its name is "layerName">} - one undo entry.
	if (command == "set_foliage_layer")
	{
		SceneObject* target = ResolveTerrainTarget(args, errOut);
		if (!target) return false;
		FoliageComponent* fc = FindFoliage((GameObject*)target->GetPTR());
		const int layer = args.value("layer", 0);
		if (!fc || layer < 0 || (size_t)layer >= fc->GetLayers().size()) { errOut = "no foliage layer " + std::to_string(layer) + " there"; return false; }
		const FoliageLayerSpec was = fc->GetLayers()[layer].spec;
		FoliageLayerSpec now = was;
		PatchSpec(now, args);
		const uint32 id = target->GetID();
		ApplyFoliageSpec(id, (uint32)layer, now);
		sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
			[this, id, layer, was]() { ApplyFoliageSpec(id, (uint32)layer, was); },
			[this, id, layer, now]() { ApplyFoliageSpec(id, (uint32)layer, now); }, "Edit Foliage Layer"));
		return AgentTerrain("get_foliage", args, out, errOut);
	}
	if (command == "remove_foliage_layer")
	{
		if (playMode) { errOut = "stop play mode first"; return false; }
		SceneObject* target = ResolveTerrainTarget(args, errOut);
		if (!target) return false;
		const uint32 id = target->GetID();
		const std::string before = SnapshotSubtree(id);
		json tree;
		try { tree = json::parse(before); }
		catch (const std::exception &e) { errOut = e.what(); return false; }
		const int layer = args.value("layer", 0);
		bool removed = false;
		json &comps = tree["root"]["components"];
		for (size_t c = 0; comps.is_array() && c < comps.size() && !removed; c++)
		{
			if (comps[c].value("type", std::string()) != "Foliage") continue;
			json &layers = comps[c]["layers"];
			if (!layers.is_array() || layer < 0 || (size_t)layer >= layers.size()) break;
			layers.erase(layers.begin() + layer);
			if (layers.empty()) comps.erase(comps.begin() + c);
			removed = true;
		}
		if (!removed) { errOut = "no foliage layer " + std::to_string(layer) + " there"; return false; }
		const bool wasCamera = IsSceneCamera(id);
		std::unique_ptr<ReplaceGameObjectCommand> cmd(new ReplaceGameObjectCommand(this, target->GetParentID(), before, tree.dump(),
			wasCamera, wasCamera ? sceneCameras[id] : EditorCameraSettings(), target->Helper != nullptr, id, "Remove Foliage Layer"));
		cmd->Redo();
		sceneUndo.Push(std::move(cmd));
		MarkSceneDirty();
		out["ok"] = true;
		return true;
	}
	errOut = "unknown terrain command " + command;
	return false;
}

void SceneEditor::ApplyFoliageSpec(uint32 goId, uint32 layer, const FoliageLayerSpec& spec)
{
	SceneObject* so = sceneObjects->GetSceneObject(goId);
	if (!so || so->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	GameObject* go = (GameObject*)so->GetPTR();
	FoliageComponent* fc = FindFoliage(go);
	if (!fc || layer >= fc->GetLayers().size()) return;
	FoliageComponent::Layer &l = fc->GetLayers()[layer];
	const bool regrow = GrowsDifferently(l.spec, spec);
	l.spec = spec;
	if (regrow)
		if (const HeightfieldData* ground = TileGround(go)) fc->Regrow(*ground, (int32)layer);
	MarkSceneDirty();
}

void SceneEditor::DrawFoliageProperties(GameObject* go, uint32 goId)
{
	FoliageComponent* fc = FindFoliage(go);
	const bool isTile = TileGround(go) != NULL;
	if (!fc && !isTile) return;
	if (!ImGui::CollapsingHeader("Foliage##props_foliage", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("foliage_props");

	// One undo entry per edit: the layer as it was when the widget was
	// grabbed, against how it is when let go. What grows is regrown then
	// too - regrowing a tile on every frame of a drag would stutter.
	static FoliageLayerSpec baseline;
	static int baselineLayer = -1;
	int removeLayer = -1;
	bool addGrass = false;
	const size_t count = fc ? fc->GetLayers().size() : 0;
	for (size_t i = 0; i < count; i++)
	{
		FoliageComponent::Layer &layer = fc->GetLayers()[i];
		FoliageLayerSpec &s = layer.spec;
		const FoliageLayerSpec before = s;
		ImGui::PushID((int)i);
		const std::string title = "Layer " + std::to_string(i) + (s.name.empty() ? std::string() : ": " + s.name) + "###layer";
		if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto track = [&]() {
				if (ImGui::IsItemActivated()) { baseline = before; baselineLayer = (int)i; }
				if (ImGui::IsItemDeactivatedAfterEdit() && baselineLayer == (int)i)
				{
					const FoliageLayerSpec was = baseline, now = s;
					// Put it back first: Apply decides whether to regrow by
					// comparing against what is live.
					s = was;
					ApplyFoliageSpec(goId, (uint32)i, now);
					sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
						[this, goId, i, was]() { ApplyFoliageSpec(goId, (uint32)i, was); },
						[this, goId, i, now]() { ApplyFoliageSpec(goId, (uint32)i, now); }, "Edit Foliage Layer"));
					baselineLayer = -1;
				}
			};
			auto immediate = [&](const bool changed) {
				if (!changed) return;
				const FoliageLayerSpec was = before, now = s;
				s = was;
				ApplyFoliageSpec(goId, (uint32)i, now);
				sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
					[this, goId, i, was]() { ApplyFoliageSpec(goId, (uint32)i, was); },
					[this, goId, i, now]() { ApplyFoliageSpec(goId, (uint32)i, now); }, "Edit Foliage Layer"));
			};
			ImGui::InputText("Name", &s.name); track();
			ImGui::TextDisabled("Growth (regrows on release)");
			ImGui::DragFloat("Density /m2", &s.density, 0.01f, 0.f, 50.f, "%.3f"); track();
			ImGui::DragFloat("Block size", &s.blockSize, 1.f, 4.f, 256.f, "%.0f m"); track();
			ImGui::DragFloat("Min scale", &s.minScale, 0.01f, 0.01f, 10.f); track();
			ImGui::DragFloat("Max scale", &s.maxScale, 0.01f, 0.01f, 10.f); track();
			ImGui::ColorEdit4("Tint low", &s.tintLow.x); track();
			ImGui::ColorEdit4("Tint high", &s.tintHigh.x); track();
			ImGui::DragFloat("Max slope", &s.maxSlopeDegrees, 0.5f, 0.f, 90.f, "%.0f deg"); track();
			ImGui::DragFloat("Min height", &s.minHeight, 0.5f, -1e9f, 1e9f, "%.1f m"); track();
			ImGui::DragFloat("Max height", &s.maxHeight, 0.5f, -1e9f, 1e9f, "%.1f m"); track();
			ImGui::SliderFloat("Align to ground", &s.alignToGround, 0.f, 1.f); track();
			ImGui::DragFloat("Sink", &s.sink, 0.01f, -10.f, 10.f, "%.2f m"); track();
			int seed = (int)s.seed;
			if (ImGui::InputInt("Seed", &seed)) s.seed = (uint32)std::max(0, seed);
			track();
			ImGui::TextDisabled("Drawing");
			ImGui::DragFloat("Full density to", &s.fullDistance, 1.f, 0.f, 5000.f, "%.0f m"); track();
			ImGui::DragFloat("Fade out by", &s.fadeDistance, 1.f, 0.f, 5000.f, "%.0f m"); track();
			ImGui::DragFloat("Shadows to", &s.shadowDistance, 1.f, 0.f, 5000.f, "%.0f m"); track();
			ImGui::DragFloat("Far mesh from", &s.lodDistance, 1.f, 0.f, 5000.f, "%.0f m"); track();
			bool shadows = s.castShadows;
			if (ImGui::Checkbox("Cast shadows", &shadows)) { s.castShadows = shadows; immediate(true); }
			s.fadeDistance = std::max(s.fadeDistance, s.fullDistance);
			ImGui::TextDisabled("Density map: %s", s.densityMap.empty() ? "(none - paint with the Foliage brush)" : s.densityMap.c_str());
			if (ImGui::SmallButton("Remove Layer")) removeLayer = (int)i;
			if (dynamic_cast<Model*>(layer.mesh.get()))
			{
				ImGui::SameLine();
				if (ImGui::SmallButton(layer.lodMesh ? "Rebake Impostor" : "Bake Impostor"))
					pendingImpostorBake = { { "id", goId }, { "layer", (int)i } };
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Renders the model onto a card that stands in for it\npast the far-mesh distance.");
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (isTile && ImGui::Button("Add Grass Layer")) addGrass = true;
	static std::string modelLayerPath;
	if (isTile)
	{
		ImGui::InputTextWithHint("##foliage_model", "assets/models/tree.p3dm", &modelLayerPath);
		ImGui::SameLine();
		if (ImGui::Button("Add Model Layer") && !modelLayerPath.empty())
			pendingFoliageOp = { { "cmd", "add_foliage_layer" }, { "id", goId }, { "mesh", modelLayerPath } };
	}
	ImGui::PopID();

	if (removeLayer >= 0)
		pendingFoliageOp = { { "cmd", "remove_foliage_layer" }, { "id", goId }, { "layer", removeLayer } };
	else if (addGrass)
		pendingFoliageOp = { { "cmd", "terrain_add_grass" }, { "id", goId } };
}

SceneObject* SceneEditor::ResolveTerrainTarget(const json& args, std::string& errOut)
{
	SceneObject* target = NULL;
	if (args.contains("id")) target = sceneObjects->GetSceneObject(args.value("id", 0u));
	else if (args.contains("name"))
	{
		target = FindGameObjectNamed(sceneObjects, args.value("name", std::string()));
		if (!target) { errOut = "object '" + args.value("name", std::string()) + "' not found"; return NULL; }
	}
	else target = SelectedSceneObject;
	if (!target || target->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "select the terrain (or a tile) first"; return NULL; }
	return target;
}

bool SceneEditor::BakeFoliageImpostor(const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
	SceneObject* target = ResolveTerrainTarget(args, errOut);
	if (!target) return false;
	const uint32 id = target->GetID();
	const int layerIndex = args.value("layer", 0);
	const std::string before = SnapshotSubtree(id);
	json tree;
	try { tree = json::parse(before); }
	catch (const std::exception &e) { errOut = e.what(); return false; }
	// Layer `layerIndex` of every tile under the target - a whole terrain
	// shares one bake - as long as it scatters the same model as the first.
	std::vector<json*> layers;
	std::function<void(json &)> visit = [&](json &node) {
		if (!node.is_object()) return;
		json &comps = node["components"];
		for (size_t c = 0; comps.is_array() && c < comps.size(); c++)
			if (comps[c].value("type", std::string()) == "Foliage" && comps[c]["layers"].is_array()
				&& layerIndex >= 0 && (size_t)layerIndex < comps[c]["layers"].size())
				layers.push_back(&comps[c]["layers"][layerIndex]);
		if (node.contains("children") && node["children"].is_array())
			for (size_t i = 0; i < node["children"].size(); i++) visit(node["children"][i]);
	};
	visit(tree["root"]);
	if (layers.empty()) { errOut = "no foliage layer " + std::to_string(layerIndex) + " there"; return false; }
	json* layer = layers[0];
	const json mesh = layer->value("mesh", json());
	if (!mesh.is_object() || mesh.value("kind", std::string()) != "model" || mesh.value("path", std::string()).empty())
	{ errOut = "the layer's mesh is not a model - only models have an impostor to bake"; return false; }
	const std::string modelRel = mesh.value("path", std::string());

	std::vector<unsigned char> rgba;
	uint32 w = 0, h = 0;
	f32 left, right, bottom, top;
	if (!RenderImpostorRGBA8(project->AbsolutePath(modelRel), rgba, w, h, left, right, bottom, top))
	{ errOut = "could not render " + modelRel; return false; }

	namespace fs = std::filesystem;
	const std::string texRel = "assets/terrain/impostors/" + fs::path(modelRel).stem().string() + "_impostor.png";
	std::error_code ec;
	fs::create_directories(fs::path(project->AbsolutePath(texRel)).parent_path(), ec);
	if (!PaintableImage::WritePNG(project->AbsolutePath(texRel), (int32)w, (int32)h, 4, rgba.data()))
	{ errOut = "could not write " + texRel; return false; }

	// Tinted layers draw with per-instance colours, and a shader reading
	// them needs the buffer - so the card's material asks for them exactly
	// when the layer has them.
	const FoliageLayerSpec spec = [&]() { FoliageLayerSpec s; PatchSpec(s, *layer); return s; }();
	const bool tinted = !(spec.tintLow == Vec4(1.f, 1.f, 1.f, 1.f) && spec.tintHigh == Vec4(1.f, 1.f, 1.f, 1.f));
	uint32 options = ShaderUsage::Texture | ShaderUsage::Diffuse | ShaderUsage::InstancedRendering | ShaderUsage::PBR | ShaderUsage::AlphaTest;
	if (tinted) options |= (1u << 25);

	if (!tree.contains("materials") || !tree["materials"].is_array()) tree["materials"] = json::array();
	json &materials = tree["materials"];
	uint32 matId = 0;
	for (size_t i = 0; i < materials.size(); i++) matId = std::max(matId, materials[i].value("id", 0u) + 1);
	json m;
	m["id"] = matId;
	m["kind"] = "generic";
	m["options"] = options;
	m["color"] = { 1, 1, 1, 1 };
	m["colorMap"] = texRel;
	m["clampMaps"] = true;
	m["alphaCutoff"] = 0.5;
	m["cullFace"] = 2;
	m["roughness"] = 0.9;
	m["castingShadows"] = false;
	materials.push_back(m);

	// Where the card takes over: the layer's own setting if it has one, else
	// half way to where it thins out.
	const f32 lodDistance = args.contains("distance") ? args.value("distance", 0.f)
		: (spec.lodDistance > 0.f ? spec.lodDistance : std::max(10.f, spec.fullDistance * 0.5f));
	int baked = 0;
	for (size_t i = 0; i < layers.size(); i++)
	{
		json &l = *layers[i];
		if (!l.contains("mesh") || l["mesh"].value("path", std::string()) != modelRel) continue;
		l["lodMesh"] = { { "kind", "primitive" }, { "shape", "Card" },
			{ "left", left }, { "right", right }, { "bottom", bottom }, { "top", top } };
		l["lodMaterial"] = matId;
		l["lodDistance"] = lodDistance;
		baked++;
	}

	const bool wasCamera = IsSceneCamera(id);
	std::unique_ptr<ReplaceGameObjectCommand> cmd(new ReplaceGameObjectCommand(this, target->GetParentID(), before, tree.dump(),
		wasCamera, wasCamera ? sceneCameras[id] : EditorCameraSettings(), target->Helper != nullptr, id, "Bake Impostor"));
	cmd->Redo();
	sceneUndo.Push(std::move(cmd));
	MarkSceneDirty();
	out["texture"] = texRel;
	out["size"] = { w, h };
	out["card"] = { left, right, bottom, top };
	out["lodDistance"] = lodDistance;
	out["tiles"] = baked;
	return true;
}

void SceneEditor::DrainPendingOps()
{
	if (!pendingFoliageOp.is_null())
	{
		json op, out;
		op.swap(pendingFoliageOp);
		std::string err;
		const std::string cmd = op.value("cmd", std::string());
		const bool network = cmd.find("network") != std::string::npos;
		const bool ok = cmd == "move_to_cell" ? AgentMoveToCell(op, out, err)
			: (network ? AgentNetwork(cmd, op, out, err) : AgentTerrain(cmd, op, out, err));
		if (!ok) echo("ERROR: " + err);
	}
}
