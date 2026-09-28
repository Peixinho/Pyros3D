// The Scene View's terrain brushes: the SceneEditor half of TerrainTools -
// the cursor ray, the stroke's press/drag/release, undo, saving, the
// Terrain panel and the terrain_* agent commands.

#include "SceneEditor.h"
#include "EditorWorld.h"
#include "SceneCommands.h"
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>

#include <algorithm>
#include <cmath>

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
		if (i == TerrainTools::PaintTexture) ImGui::TextDisabled("Paint");
		else if (i != 0) ImGui::SameLine();
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
		// A streamed world's content lives in its cells; a terrain made here
		// would land in the always-loaded scene file instead. Create it in
		// the plain scene and split that into cells.
		if (sceneWorld.enabled) { errOut = "the scene is a streamed world - create terrain before splitting it into cells"; return false; }
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
		SceneObject* target = NULL;
		const std::string name = args.value("name", std::string());
		if (!name.empty())
		{
			for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end() && !target; ++i)
				if (i->second && i->second->GetType() == SceneObjectTypes::GAMEOBJECT && i->second->GetName() == name) target = i->second;
			if (!target) { errOut = "object '" + name + "' not found"; return false; }
		}
		else target = SelectedSceneObject;
		if (!target || target->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "select the terrain (or a tile) first"; return false; }
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
	errOut = "unknown terrain command " + command;
	return false;
}
