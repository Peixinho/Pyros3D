// The Scene View's terrain brushes: the SceneEditor half of TerrainTools -
// the cursor ray, the stroke's press/drag/release, undo, saving, the
// Terrain panel and the terrain_* agent commands.

#include "SceneEditor.h"
#include "EditorWorld.h"
#include "SceneCommands.h"
#include "MaterialCodegen.h"
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Rendering/Components/Foliage/Foliage.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>

#include <Pyros3D/Assets/Texture/PaintableImage.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <algorithm>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <fstream>
#include <set>
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

	// What the foliage and material commands edit inside an object's
	// snapshot: a Terrain object's tile template - itself a subtree, one
	// tile and the materials it uses - or else the snapshot.
	json &TileSubtree(json &tree)
	{
		if (tree.contains("root") && tree["root"].is_object() && tree["root"].contains("components") && tree["root"]["components"].is_array())
			for (auto &c : tree["root"]["components"])
				if (c.is_object() && c.value("type", std::string()) == "Terrain" && c.contains("tileTemplate") && c["tileTemplate"].is_object())
					return c["tileTemplate"];
		return tree;
	}

	TerrainComponent* TerrainOn(GameObject* go)
	{
		if (!go) return NULL;
		for (size_t c = 0; c < go->GetComponents().size(); c++)
			if (TerrainComponent* tc = dynamic_cast<TerrainComponent*>(go->GetComponents()[c].get())) return tc;
		return NULL;
	}

	// The template's foliage layers (NULL when it has none).
	json* TemplateFoliageLayers(json &tmpl)
	{
		if (!tmpl.contains("root") || !tmpl["root"].contains("components") || !tmpl["root"]["components"].is_array()) return NULL;
		for (auto &c : tmpl["root"]["components"])
			if (c.is_object() && c.value("type", std::string()) == "Foliage" && c.contains("layers") && c["layers"].is_array()) return &c["layers"];
		return NULL;
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
		case TerrainTools::Place: return "Place Objects";
		case TerrainTools::Hole: return "Cut Terrain Hole";
		case TerrainTools::Fill: return "Fill Terrain Hole";
		case TerrainTools::Dig: return "Dig Cave";
		case TerrainTools::Pack: return "Fill Cave";
		case TerrainTools::CaveSmooth: return "Smooth Cave";
		case TerrainTools::CaveLevel: return "Level Cave Floor";
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
	// Set here for the same reason: ResetScene() starts the brushes over,
	// and the world they must ask is whichever one is open now.
	SceneEditor* self = this;
	terrainTools->SetEditable([self](const GameObject* owner) { return !(self->editorWorld && self->editorWorld->IsFar(owner)); });
	return *terrainTools;
}

void SceneEditor::LiftViewAboveTerrain()
{
	if (worldViewLiftTries <= 0 || playMode || sceneIsTwoD || !CameraPivot) return;
	GameObject* viewCam = GetViewCameraGO();
	if (!viewCam || activeSceneCameraId != 0) return;
	worldViewLiftTries--;
	const Vec3 pivot = CameraPivot->GetWorldPosition();
	const Vec3 eye = viewCam->GetWorldPosition();
	f32 h;
	if (!TerrainEditor::HeightAt(scene, eye.x, eye.z, h)) return;
	worldViewLiftTries = 0;
	if (eye.y > h + 1.f) return;
	f32 hp = h;
	TerrainEditor::HeightAt(scene, pivot.x, pivot.z, hp);
	// The pivot onto its ground, and higher still if that leaves the eye
	// under the ground it stands over.
	const f32 lift = std::max(hp - pivot.y, h + 2.f - eye.y);
	CameraPivot->SetPosition(CameraPivot->GetPosition() + Vec3(0.f, lift, 0.f));
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
	if (terrainTools->Stroking() && terrainTools->tool == TerrainTools::Place && terrainTools->HoverValid())
	{
		std::string err;
		if (!PlaceDab(terrainTools->HoverPoint().x, terrainTools->HoverPoint().z, err) && !err.empty())
		{
			echo("ERROR: Place - " + err);
			EndTerrainStroke();
		}
	}
}

bool SceneEditor::BeginTerrainStroke()
{
	if (!terrainTools || !terrainTools->BeginStroke(scene)) return false;
	if (terrainTools->IsCaveTool()) EnsureCaveMaterials();
	placeLastValid = false;
	if (terrainTools->tool == TerrainTools::Place && !placeGroupOpen)
	{
		sceneUndo.BeginGroup("Place " + std::filesystem::path(terrainTools->placeAsset).stem().string());
		placeGroupOpen = true;
	}
	return true;
}

bool SceneEditor::PlaceDab(const f32 x, const f32 z, std::string& errOut)
{
	TerrainTools &t = Terrain();
	if (placeLastValid)
	{
		const f32 dx = x - placeLastX, dz = z - placeLastZ;
		if (dx * dx + dz * dz < t.placeSpacing * t.placeSpacing) return true;
	}
	if (t.placeAsset.empty()) { errOut = "no asset to place - pick a model or prefab"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
	f32 h;
	Vec3 n;
	if (!TerrainTools::GroundPoint(scene, x, z, h, n)) return true;	// off the terrain: skip, keep going
	placeLastValid = true;
	placeLastX = x;
	placeLastZ = z;

	auto rnd = [this]() { placeRandom = placeRandom * 1664525u + 1013904223u; return (f32)(placeRandom >> 8) / 16777216.f; };
	const f32 s = t.placeScaleMin + (t.placeScaleMax - t.placeScaleMin) * rnd();
	Vec3 rot(0.f, t.placeRandomYaw ? rnd() * 6.2831853f : 0.f, 0.f);	// radians, as the engine's Euler angles are
	if (t.placeAlign)
	{
		rot.x = std::atan2(n.z, n.y);
		rot.z = -std::atan2(n.x, n.y);
	}
	const Vec3 pos(x, h, z);
	const std::string abs = project->AbsolutePath(t.placeAsset);
	uint32 id = 0;
	if (ProjectManager::IsPrefabExtension(abs))
	{
		id = OpInstantiatePrefab(t.placeAsset, pos, errOut);
		if (!id) return false;
		// The add's snapshot has the prefab's own pose; this step, in the
		// same group, turns and scales it.
		if (!OpSetTransform(id, pos, rot, Vec3(s, s, s), errOut)) return false;
	}
	else if (ProjectManager::IsP3dm(abs))
	{
		SceneObject* obj = sceneObjects->CreateGameObject(std::filesystem::path(abs).stem().string());
		if (!obj) { errOut = "could not create an object"; return false; }
		GameObject* go = (GameObject*)obj->GetPTR();
		if (!sceneObjects->CreateRenderingModel(go, abs)) { sceneObjects->DestroySceneObject(obj->GetID()); errOut = "could not load " + t.placeAsset; return false; }
		go->SetPosition(pos);
		go->SetRotation(rot);
		go->SetScale(Vec3(s, s, s));
		PushAddCommand(obj);
		id = obj->GetID();
	}
	else { errOut = t.placeAsset + " is not a model (.p3dm) or a prefab"; return false; }

	// A streamed world keeps it in the cell it stands in.
	if (editorWorld)
	{
		int32 cx, cz;
		editorWorld->CellOf(pos, cx, cz);
		if (!MoveObjectToCell(id, cx, cz, errOut)) return false;
	}
	MarkSceneDirty();
	return true;
}

void SceneEditor::EndTerrainStroke()
{
	if (!terrainTools || !terrainTools->Stroking()) return;
	std::vector<TerrainEditor::TileSnapshot> before, after;
	const bool changed = terrainTools->EndStroke(before, after);
	if (placeGroupOpen)
	{
		placeGroupOpen = false;
		sceneUndo.EndGroup();
	}
	if (!changed) return;
	sceneUndo.Push(std::unique_ptr<IUndoableCommand>(new TerrainStrokeCommand(this, terrainTools.get(), scene,
		before, after, StrokeName(terrainTools->tool))));
	MarkSceneDirty();
}

bool SceneEditor::SaveTerrain()
{
	const std::vector<TerrainComponent*> terrains = SceneTerrains();
	// Which terrain tiles are about to be written: their part of the
	// overview follows them.
	std::map<TerrainComponent*, std::vector<std::pair<int32, int32> > > written;
	if (terrainTools)
	{
		const std::vector<const GameObject*> owners = terrainTools->UnsavedOwners();
		for (size_t i = 0; i < owners.size(); i++)
			for (size_t t = 0; t < terrains.size(); t++)
			{
				int32 x, z;
				if (terrains[t]->TileOf(owners[i], x, z)) written[terrains[t]].push_back(std::make_pair(x, z));
			}
		if (project && project->IsOpen()) terrainTools->SetAssetRoot(project->GetProjectPath());
		if (!terrainTools->Save())
		{
			echo("ERROR: saving terrain - a heightmap, splat or density map could not be written");
			return false;
		}
	}
	bool ok = true;
	for (size_t t = 0; t < terrains.size(); t++)
	{
		const uint32 id = sceneObjects->GetSceneObjectID(terrains[t]->GetOwner());
		const bool all = terrainOverviewStale.count(id) > 0 || !terrains[t]->HasOverview();
		std::map<TerrainComponent*, std::vector<std::pair<int32, int32> > >::const_iterator w = written.find(terrains[t]);
		if (!all && w == written.end()) continue;
		std::string err;
		if (!terrains[t]->BakeOverview(all ? NULL : &w->second, err))
		{
			echo("ERROR: baking the terrain overview - " + err);
			ok = false;
		}
		else terrainOverviewStale.erase(id);
	}
	return ok;
}

std::vector<TerrainComponent*> SceneEditor::SceneTerrains() const
{
	std::vector<TerrainComponent*> out;
	const std::vector<TerrainComponent*> &all = TerrainComponent::Instances();
	for (size_t i = 0; i < all.size(); i++)
		if (all[i]->GetOwner() && sceneObjects && sceneObjects->GetSceneObjectID(all[i]->GetOwner()) != 0) out.push_back(all[i]);
	return out;
}

void SceneEditor::UpdateTerrainObjects(const std::vector<Vec3>& foci)
{
	TerrainComponent::SetViewers(scene, foci);
	const std::vector<TerrainComponent*> terrains = SceneTerrains();
	SceneEditor* self = this;
	for (size_t i = 0; i < terrains.size(); i++)
	{
		// A scene never saved has no path for its objects to find the
		// project by.
		if (terrains[i]->GetAssetRoot().empty() && project && project->IsOpen())
			terrains[i]->SetAssetRoot(project->GetProjectPath());
		if (terrains[i]->HasHooks()) continue;
		// A tile with unsaved brush edits stays until it is saved, and one
		// leaving takes itself out of the brushes' bookkeeping.
		terrains[i]->SetUnloadVeto([self](GameObject* tile) {
			return !self->playMode && self->terrainTools && self->terrainTools->HasUnsaved(tile); });
		terrains[i]->SetOnTileUnloading([self](GameObject* tile) {
			if (self->terrainTools) self->terrainTools->Forget(tile); });
	}
}

void SceneEditor::EnsureCaveMaterials()
{
	if (!project || !project->IsOpen()) return;
	namespace fs = std::filesystem;
	const std::vector<TerrainComponent*> terrains = SceneTerrains();
	for (size_t i = 0; i < terrains.size(); i++)
	{
		if (!terrains[i]->GetSettings().caveMaterial.empty()) continue;
		// Cave walls have no unwrap, so the rock is laid on by world
		// position from three sides and blended by which way the wall
		// faces. Written once into the project, then the user's to change.
		const std::string shaderRel = "assets/terrain/cave_rock.glsl";
		const fs::path root(project->GetProjectPath());
		std::error_code ec;
		std::string rock = "assets/terrain/layers/rock.png";
		if (!fs::exists(root / rock, ec)) rock = TerrainLayerTexture(2);
		if (rock.empty() || !fs::exists(root / rock, ec)) continue;	// plain grey walls
		if (!fs::exists(root / shaderRel, ec))
		{
			const char* body =
				"vec3 bw = abs(p3d_N);\n"
				"bw = bw / max(bw.x + bw.y + bw.z, 0.0001);\n"
				"vec3 Albedo = texture_2D(rock, vWorldPos.zy / 5.0).rgb * bw.x\n"
				"    + texture_2D(rock, vWorldPos.xz / 5.0).rgb * bw.y\n"
				"    + texture_2D(rock, vWorldPos.xy / 5.0).rgb * bw.z;\n"
				"Albedo *= 0.8;\n"
				"float Metallic = 0.0;\n"
				"float Roughness = 0.95;\n";
			const MaterialCodegenResult gen = GenerateGLSLFromSimpleText(body, { "rock" });
			if (!gen.error.empty()) { echo("ERROR: cave shader - " + gen.error); continue; }
			fs::create_directories((root / shaderRel).parent_path(), ec);
			std::ofstream out((root / shaderRel).string().c_str(), std::ios::binary | std::ios::trunc);
			out << gen.glsl;
			if (!out) continue;
		}
		const json m = { { "id", 0 }, { "kind", "custom" }, { "shaderFile", shaderRel }, { "castingShadows", false },
			{ "samplers", json::array({ { { "name", "rock" }, { "texture", rock } } }) } };
		terrains[i]->SetCaveMaterial(m.dump());
		MarkSceneDirty();
	}
}

namespace {
	TerrainComponent* FindTerrain(GameObject* go)
	{
		if (!go) return NULL;
		for (size_t c = 0; c < go->GetComponents().size(); c++)
			if (TerrainComponent* tc = dynamic_cast<TerrainComponent*>(go->GetComponents()[c].get())) return tc;
		return NULL;
	}

	json TerrainInfo(TerrainComponent* tc, const std::string &name, const uint32 id)
	{
		const TerrainComponent::Settings &s = tc->GetSettings();
		const Vec3 at = tc->GetOwner() ? tc->GetOwner()->GetWorldPosition() : Vec3();
		return { { "name", name }, { "id", id }, { "directory", s.directory }, { "tilesX", s.tilesX }, { "tilesZ", s.tilesZ },
			{ "tileSize", s.tileSize }, { "loadRadius", s.loadRadius }, { "unloadRadius", s.unloadRadius },
			{ "viewDistance", s.viewDistance }, { "overviewSamples", s.overviewSamples }, { "centre", { at.x, at.y, at.z } },
			{ "corner", { at.x + tc->Corner().x, at.y, at.z + tc->Corner().z } },
			{ "loaded", tc->LoadedCount() }, { "loading", tc->LoadingCount() }, { "distant", tc->DistantCount() },
			{ "overview", tc->HasOverview() } };
	}
}

bool SceneEditor::ConvertTerrainToObjects(const json& args, json& out, std::string& errOut)
{
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
	if (scenePath.empty()) { errOut = "save the scene first"; return false; }
	if (terrainTools && terrainTools->Stroking()) EndTerrainStroke();
	// What is on disk must be what is on screen: the conversion reads and
	// rewrites the files.
	if (!AgentSave(errOut)) return false;
	const std::string path = scenePath;
	if (!TerrainTools::ConvertSceneTerrains(path, project->GetProjectPath(), args.value("force", false), out, errOut)) return false;
	if (!LoadSceneFromFile(path)) { errOut = "the scene was converted but could not be reloaded"; return false; }
	// The new terrains have maps but no overview yet.
	const std::vector<TerrainComponent*> terrains = SceneTerrains();
	for (size_t i = 0; i < terrains.size(); i++)
	{
		std::string err;
		if (!terrains[i]->HasOverview() && !terrains[i]->BakeOverview(NULL, err)) echo("ERROR: baking the terrain overview - " + err);
	}
	return true;
}

bool SceneEditor::AgentTerrainObject(const std::string& command, const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	if (command == "terrain_convert") return ConvertTerrainToObjects(args, out, errOut);
	if (command == "terrain_info")
	{
		out["terrains"] = json::array();
		const std::vector<TerrainComponent*> terrains = SceneTerrains();
		for (size_t i = 0; i < terrains.size(); i++)
		{
			const uint32 id = sceneObjects->GetSceneObjectID(terrains[i]->GetOwner());
			SceneObject* so = sceneObjects->GetSceneObject(id);
			out["terrains"].push_back(TerrainInfo(terrains[i], so ? so->GetName() : std::string(), id));
		}
		return true;
	}
	// Noise tunnels under the terrain around a point: {"centre": [x, y, z]
	// (default: the view's pivot), "radius", "seed", "size", "width",
	// "minDepth", "maxDepth"}. One undo entry.
	if (command == "terrain_generate_caves")
	{
		if (playMode || sceneIsTwoD) { errOut = "caves need a 3D scene outside play"; return false; }
		if (SceneTerrains().empty()) { errOut = "caves belong to a Terrain object: there is none in this scene"; return false; }
		TerrainTools &t = Terrain();
		if (t.Stroking()) EndTerrainStroke();
		Vec3 centre = CameraPivot ? CameraPivot->GetWorldPosition() : Vec3();
		if (args.contains("centre") && args["centre"].is_array() && args["centre"].size() >= 3)
			centre = Vec3(args["centre"][0].get<f32>(), args["centre"][1].get<f32>(), args["centre"][2].get<f32>());
		const f32 radius = std::min(std::max(args.value("radius", 150.f), 4.f), 4000.f);
		// The tiles it reaches must be in: it digs what is loaded.
		const std::vector<TerrainComponent*> terrains = SceneTerrains();
		std::vector<Vec3> foci;
		for (int k = 0; k < 9; k++) foci.push_back(centre + Vec3((k % 3 - 1) * radius, 0.f, (k / 3 - 1) * radius));
		for (size_t i = 0; i < terrains.size(); i++) terrains[i]->LoadAround(foci);
		EnsureCaveMaterials();
		t.BeginRecording();
		const uint32 tiles = t.GenerateCaves(scene, centre, radius, args.value("seed", 1u), std::max(4.f, args.value("size", 48.f)),
			std::min(std::max(args.value("width", 0.09f), 0.f), 0.5f), args.value("minDepth", 6.f), args.value("maxDepth", 90.f));
		const size_t undoBefore = sceneUndo.UndoCount();
		EndTerrainStroke();
		out["tiles"] = tiles;
		out["undoEntry"] = sceneUndo.UndoCount() > undoBefore;
		return true;
	}
	// Caves saved by an older build: openings and walls made again from
	// the voxels of every loaded tile. One undo entry.
	if (command == "terrain_resync_caves")
	{
		if (playMode || sceneIsTwoD) { errOut = "caves need a 3D scene outside play"; return false; }
		TerrainTools &t = Terrain();
		if (t.Stroking()) EndTerrainStroke();
		t.BeginRecording();
		out["tiles"] = t.ResyncCaves(scene);
		EndTerrainStroke();
		return true;
	}
	// The rest name one terrain: by id, by name, the selection, or the
	// scene's only one.
	TerrainComponent* tc = NULL;
	SceneObject* target = NULL;
	if (args.contains("id") || args.contains("name") || SelectedSceneObject)
	{
		std::string ignored;
		target = ResolveTerrainTarget(args, ignored);
		if (target) tc = FindTerrain((GameObject*)target->GetPTR());
	}
	if (!tc)
	{
		const std::vector<TerrainComponent*> terrains = SceneTerrains();
		if (terrains.size() == 1)
		{
			tc = terrains[0];
			target = sceneObjects->GetSceneObject(sceneObjects->GetSceneObjectID(tc->GetOwner()));
		}
	}
	if (!tc || !target) { errOut = "name a Terrain object ('name' or 'id'), or select one"; return false; }
	if (command == "set_terrain")
	{
		const TerrainComponent::Settings was = tc->GetSettings();
		const f32 load = std::max(0.f, args.value("loadRadius", was.loadRadius));
		const f32 unload = std::max(load, args.value("unloadRadius", std::max(was.unloadRadius, load * 1.25f)));
		const f32 view = std::max(0.f, args.value("viewDistance", was.viewDistance));
		const uint32 id = target->GetID();
		auto apply = [this, id](const f32 l, const f32 u, const f32 v) {
			SceneObject* so = sceneObjects->GetSceneObject(id);
			TerrainComponent* t = so ? FindTerrain((GameObject*)so->GetPTR()) : NULL;
			if (!t) return;
			t->SetRadii(l, u);
			t->SetViewDistance(v);
			MarkSceneDirty();
		};
		apply(load, unload, view);
		sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
			[apply, was]() { apply(was.loadRadius, was.unloadRadius, was.viewDistance); },
			[apply, load, unload, view]() { apply(load, unload, view); }, "Edit Terrain"));
		out = TerrainInfo(tc, target->GetName(), id);
		return true;
	}
	if (command == "terrain_bake_overview")
	{
		if (!SaveTerrain()) { errOut = "the terrain's maps could not be saved"; return false; }
		if (!tc->BakeOverview(NULL, errOut)) return false;
		out = TerrainInfo(tc, target->GetName(), target->GetID());
		return true;
	}
	errOut = "unknown terrain command " + command;
	return false;
}

void SceneEditor::DrawTerrainProperties(GameObject* go, uint32 goId)
{
	TerrainComponent* tc = FindTerrain(go);
	if (!tc) return;
	if (!ImGui::CollapsingHeader("Terrain##props_terrain", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::PushID("terrain_props");
	const TerrainComponent::Settings &s = tc->GetSettings();
	ImGui::Text("%d x %d tiles of %.0f m (%.0f x %.0f m)", s.tilesX, s.tilesZ, s.tileSize, s.tilesX * s.tileSize, s.tilesZ * s.tileSize);
	ImGui::TextDisabled("%s", s.directory.c_str());
	ImGui::Text("%u tile(s) loaded, %u loading, %u drawn from the overview", tc->LoadedCount(), tc->LoadingCount(), tc->DistantCount());
	if (!tc->HasOverview())
		ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "No overview yet: tiles past the load radius are not drawn.");

	// One undo entry per drag: sent when the widget is let go.
	static f32 load = 0.f, unload = 0.f, view = 0.f;
	static uint32 editing = 0;
	if (editing != goId || !ImGui::IsAnyItemActive()) { load = s.loadRadius; unload = s.unloadRadius; view = s.viewDistance; editing = goId; }
	bool commit = false;
	ImGui::DragFloat("Full tiles within (m)", &load, 8.f, 0.f, 100000.f, "%.0f");
	commit |= ImGui::IsItemDeactivatedAfterEdit();
	ImGui::DragFloat("Dropped past (m)", &unload, 8.f, 0.f, 100000.f, "%.0f");
	commit |= ImGui::IsItemDeactivatedAfterEdit();
	ImGui::DragFloat("Drawn out to (m)", &view, 16.f, 0.f, 1000000.f, view > 0.f ? "%.0f" : "everything");
	commit |= ImGui::IsItemDeactivatedAfterEdit();
	if (commit)
		pendingFoliageOp = { { "cmd", "set_terrain" }, { "id", goId }, { "loadRadius", load }, { "unloadRadius", std::max(load, unload) },
			{ "viewDistance", view } };

	ImGui::TextDisabled("To sculpt, paint or dig: select the Terrain component in the tree (or press B).\nIts brushes are in the Tools window.");
	if (ImGui::Button("Bake Overview")) pendingFoliageOp = { { "cmd", "terrain_bake_overview" }, { "id", goId } };
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Rebuilds the low-resolution ground drawn past the load radius\nfrom every tile's maps. Saving does this for edited tiles.");

	// Foliage: the layers every tile grows, from the template. An edit
	// rebuilds the terrain's tiles, so it is sent when the widget is let go.
	ImGui::Separator();
	ImGui::TextDisabled("Foliage (every tile)");
	static std::vector<FoliageLayerSpec> specs;
	static std::vector<bool> isModel;
	static uint32 specsFor = 0;
	static std::string specsFrom;
	if (specsFor != goId || (!ImGui::IsAnyItemActive() && specsFrom != s.tileTemplate))
	{
		specs.clear();
		isModel.clear();
		try
		{
			json tmpl = json::parse(s.tileTemplate);
			if (json* layers = TemplateFoliageLayers(tmpl))
				for (size_t i = 0; i < layers->size(); i++)
				{
					FoliageLayerSpec spec;
					PatchSpec(spec, (*layers)[i]);
					spec.name = (*layers)[i].value("name", std::string());
					specs.push_back(spec);
					isModel.push_back((*layers)[i].contains("mesh") && (*layers)[i]["mesh"].value("kind", std::string()) == "model");
				}
		}
		catch (const std::exception &) {}
		specsFor = goId;
		specsFrom = s.tileTemplate;
	}
	for (size_t i = 0; i < specs.size(); i++)
	{
		FoliageLayerSpec &fs = specs[i];
		ImGui::PushID((int)i);
		const std::string title = "Layer " + std::to_string(i) + (fs.name.empty() ? std::string() : ": " + fs.name) + "###layer";
		if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool send = false;
			ImGui::DragFloat("Density /m2", &fs.density, 0.01f, 0.f, 50.f, "%.3f"); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::DragFloat("Min scale", &fs.minScale, 0.01f, 0.01f, 10.f); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::DragFloat("Max scale", &fs.maxScale, 0.01f, 0.01f, 10.f); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::DragFloat("Max slope", &fs.maxSlopeDegrees, 0.5f, 0.f, 90.f, "%.0f deg"); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::DragFloat("Full density to", &fs.fullDistance, 1.f, 0.f, 5000.f, "%.0f m"); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::DragFloat("Fade out by", &fs.fadeDistance, 1.f, 0.f, 5000.f, "%.0f m"); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::DragFloat("Far mesh from", &fs.lodDistance, 1.f, 0.f, 5000.f, "%.0f m"); send |= ImGui::IsItemDeactivatedAfterEdit();
			if (send)
			{
				json op = SpecJson(fs);
				op.erase("densityMap");
				op.erase("name");
				op["cmd"] = "set_foliage_layer";
				op["id"] = goId;
				op["layer"] = (int)i;
				pendingFoliageOp = op;
			}
			if (ImGui::SmallButton("Remove Layer"))
				pendingFoliageOp = { { "cmd", "remove_foliage_layer" }, { "id", goId }, { "layer", (int)i } };
			if (isModel[i])
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("Bake Impostor")) pendingImpostorBake = { { "id", goId }, { "layer", (int)i } };
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (ImGui::Button("Add Grass Layer")) pendingFoliageOp = { { "cmd", "terrain_add_grass" }, { "id", goId } };
	static std::string terrainModelLayer;
	ImGui::InputTextWithHint("##terrain_foliage_model", "assets/models/tree.p3dm", &terrainModelLayer);
	ImGui::SameLine();
	if (ImGui::Button("Add Model Layer") && !terrainModelLayer.empty())
		pendingFoliageOp = { { "cmd", "add_foliage_layer" }, { "id", goId }, { "mesh", terrainModelLayer } };
	ImGui::PopID();
}

namespace {
	// What the New Terrain window asked for, run after its End().
	json g_pendingTerrainCreate, g_pendingTerrainImport;
	std::string g_terrainCreateError;
}

bool SceneEditor::IsTerrainSelection() const
{
	if (!SelectedSceneObject || sceneIsTwoD) return false;
	if (SelectedSceneObject->GetType() == SceneObjectTypes::TERRAIN_COMPONENT) return true;
	// A terrain saved before terrains were objects: its tile's mesh.
	if (SelectedSceneObject->GetType() == SceneObjectTypes::RENDERING_COMPONENT)
	{
		RenderingComponent* rc = (RenderingComponent*)SelectedSceneObject->GetPTR();
		return rc && dynamic_cast<Heightfield*>(rc->GetRenderable()) != NULL;
	}
	return false;
}

bool SceneEditor::IsTileMapSelection() const
{
	return SelectedSceneObject && SelectedSceneObject->GetType() == SceneObjectTypes::TILEMAP_COMPONENT;
}

void SceneEditor::SyncToolsToSelection()
{
	if (playMode) return;
	const uint32 now = SelectedSceneObject ? SelectedSceneObject->GetID() : 0;
	if (now == toolsSelection) return;
	const int was = toolsSelectionKind;
	const int kind = IsTerrainSelection() ? 1 : (IsTileMapSelection() ? 2 : 0);
	toolsSelection = now;
	toolsSelectionKind = kind;
	// The component's tool starts with its selection and ends with it: the
	// left button in the viewport is the brush for as long as the component
	// is what is selected, and never otherwise.
	if (kind == 1) SetTerrainMode(true);
	else if (was == 1) SetTerrainMode(false);
	if (kind == 2)
	{
		tilePaintTarget = SelectedSceneObject->GetParentID();
		uiEditMode = false;
		SetTilePaintMode(true);
	}
	else if (was == 2) SetTilePaintMode(false);
}

void SceneEditor::LeaveComponentTools()
{
	if (!SelectedSceneObject) return;
	if (!IsTerrainSelection() && !IsTileMapSelection()) { SetTerrainMode(false); SetTilePaintMode(false); return; }
	if (SceneObject* owner = sceneObjects->GetSceneObject(SelectedSceneObject->GetParentID())) SelectSceneObject(owner);
}

void SceneEditor::ToggleComponentTools()
{
	if (playMode) return;
	if (IsTerrainSelection() || IsTileMapSelection()) { LeaveComponentTools(); return; }
	const uint32 want = sceneIsTwoD ? SceneObjectTypes::TILEMAP_COMPONENT : SceneObjectTypes::TERRAIN_COMPONENT;
	// The one under the selected object if there is one, else the scene's first.
	SceneObject* found = NULL;
	const uint32 under = SelectedSceneObject ? SelectedSceneObject->GetID() : 0;
	for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); ++i)
	{
		if (!i->second || i->second->GetType() != want) continue;
		if (!found) found = i->second;
		if (under && i->second->GetParentID() == under) { found = i->second; break; }
	}
	if (found) SelectSceneObject(found);
	else if (!sceneIsTwoD) showNewTerrain = true;	// nothing to edit yet: offer to make one
}

void SceneEditor::DrawTerrainTools()
{
	if (sceneIsTwoD) return;
	TerrainTools &t = Terrain();
	// The ring is grey over ground that is still a far version; say why.
	if (t.HoverValid() && !t.HoverEditable())
		ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "Loading the cell under the brush...");

	static const char* labels[TerrainTools::ToolCount] = { "Raise", "Lower", "Smooth", "Flatten", "Texture", "Foliage", "Place", "Cut Hole", "Fill Hole", "Dig Cave", "Fill Cave", "Smooth Cave", "Level Floor" };
	ImGui::TextDisabled("Sculpt");
	for (int i = 0; i < TerrainTools::ToolCount; i++)
	{
		// Two to a row, so a narrow panel does not clip the last one.
		if (i == TerrainTools::PaintTexture) ImGui::TextDisabled("Paint");
		else if (i == TerrainTools::Hole) ImGui::TextDisabled("Holes");
		else if (i == TerrainTools::Dig) ImGui::TextDisabled("Caves");
		else if (i < TerrainTools::PaintTexture && i % 2 == 1) ImGui::SameLine();
		if (i == TerrainTools::PaintFoliage || i == TerrainTools::Place || i == TerrainTools::Fill || i == TerrainTools::Pack
			|| i == TerrainTools::CaveLevel) ImGui::SameLine();
		if (ImGui::RadioButton(labels[i], t.tool == i)) t.tool = (TerrainTools::Tool)i;
	}
	ImGui::Separator();
	ImGui::SliderFloat("Radius", &t.radius, 0.5f, 200.f, "%.1f m", ImGuiSliderFlags_Logarithmic);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl + scroll wheel over the viewport changes it too.");
	ImGui::SliderFloat("Strength", &t.strength, 0.01f, 1.f, "%.2f");
	ImGui::SliderFloat("Hardness", &t.hardness, 0.f, 1.f, "%.2f");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("0: fades from the centre to the edge. 1: a hard disc.");
	if (t.tool == TerrainTools::Flatten)
		ImGui::TextWrapped("Levels the ground to the height where the stroke starts.");
	static json pendingCaves;
	if (t.IsCaveTool())
	{
		if (t.CaveRadius() > t.radius)
			ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "Working at %.1f m: caves here are made of %.1f m voxels,\nand a smaller brush would not open the rock.",
				t.CaveRadius(), t.CaveRadius() / 1.5f);
		if (t.tool == TerrainTools::Dig || t.tool == TerrainTools::Pack)
			ImGui::TextWrapped("Works where the cursor points - on the ground, or on a cave's wall - for as long as it is held: "
				"the wall gives way at a rate set by Strength, so holding it bores a tunnel along the view. "
				"Where it breaks the surface the ground opens.");
		else if (t.tool == TerrainTools::CaveSmooth)
			ImGui::TextWrapped("Rounds off what the cursor points at: the ridges between overlapping digs, a rough wall or floor.");
		else
			ImGui::TextWrapped("Makes a flat floor at the height where the stroke starts: rock is filled in below it and dug away above, "
				"within the brush. Drag along a tunnel to give it a floor to walk on; the radius is also the headroom.");
		if (ImGui::TreeNodeEx("Generate tunnels", ImGuiTreeNodeFlags_DefaultOpen))
		{
			static int seed = 1;
			static float area = 150.f, size = 48.f, width = 0.09f, minDepth = 6.f, maxDepth = 90.f;
			ImGui::InputInt("Seed", &seed);
			ImGui::DragFloat("Area radius (m)", &area, 2.f, 10.f, 2000.f, "%.0f");
			ImGui::DragFloat("Bend size (m)", &size, 1.f, 8.f, 400.f, "%.0f");
			ImGui::SliderFloat("Tunnel width", &width, 0.03f, 0.3f, "%.2f");
			ImGui::DragFloat("Shallowest (m)", &minDepth, 0.5f, -10.f, 500.f, "%.0f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Metres under the ground where tunnels start. 0 or less lets them break the surface.");
			ImGui::DragFloat("Deepest (m)", &maxDepth, 1.f, 1.f, 2000.f, "%.0f");
			maxDepth = std::max(maxDepth, minDepth + 4.f);
			if (ImGui::Button("Generate Around the View"))
				pendingCaves = { { "seed", std::max(0, seed) }, { "radius", area }, { "size", size }, { "width", width },
					{ "minDepth", minDepth }, { "maxDepth", maxDepth } };
			ImGui::TreePop();
		}
	}
	if (!pendingCaves.is_null())
	{
		json op, ignored;
		op.swap(pendingCaves);
		op["cmd"] = "terrain_generate_caves";
		pendingFoliageOp = op;
	}
	if (t.tool == TerrainTools::Hole || t.tool == TerrainTools::Fill)
		ImGui::TextWrapped("A hole has no surface and no collision: cut one where a mesh of your own takes over, then place the "
			"mesh in it. On the ground it opens the terrain; pointed at a cave's wall or floor it opens the cave "
			"(a door, a shaft, a hand-made tunnel). Only the radius matters.");
	if (t.tool == TerrainTools::PaintTexture)
	{
		static const char* layers[] = { "0  Grass", "1  Dirt", "2  Rock", "3  Sand" };
		t.layer = std::max(0, std::min(3, t.layer));
		ImGui::Combo("Layer", &t.layer, layers, 4);
		ImGui::TextWrapped("Paints the tile's splat map. Layer textures are the splat material's layer0..layer3 samplers.");
	}
	if (t.tool == TerrainTools::Place)
	{
		ImGui::InputTextWithHint("Asset", "assets/models/tree.p3dm or .prefab", &t.placeAsset);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL")) t.placeAsset = std::string((const char*)payload->Data);
			ImGui::EndDragDropTarget();
		}
		ImGui::SliderFloat("Spacing", &t.placeSpacing, 0.5f, 100.f, "%.1f m", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloatRange2("Scale", &t.placeScaleMin, &t.placeScaleMax, 0.01f, 0.01f, 20.f, "%.2f", "%.2f");
		ImGui::Checkbox("Random yaw", &t.placeRandomYaw);
		ImGui::SameLine();
		ImGui::Checkbox("Align to slope", &t.placeAlign);
		ImGui::TextWrapped("Click places one; a drag places one per spacing. One undo step per stroke.");
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

	if (ImGui::CollapsingHeader("Layers##terrain_layers"))
	{
		static const char* layerNames[4] = { "0", "1", "2", "3" };
		static std::string layerPath[4];
		static bool layerLoaded = false;
		if (!layerLoaded || ImGui::IsWindowAppearing())
		{
			for (int k = 0; k < 4; k++) layerPath[k] = TerrainLayerTexture(k);
			layerLoaded = true;
		}
		ImGui::TextWrapped("Every splat tile's ground textures. Drop a texture from Assets on a slot.");
		for (int k = 0; k < 4; k++)
		{
			ImGui::PushID(k);
			ImGui::SetNextItemWidth(-60.f);
			ImGui::InputText(layerNames[k], &layerPath[k]);
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL"))
					layerPath[k] = std::string((const char*)payload->Data);
				ImGui::EndDragDropTarget();
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Apply"))
			{
				std::string err;
				if (!SetTerrainLayerTexture(k, layerPath[k], true, err)) echo("ERROR: terrain layer - " + err);
				layerPath[k] = TerrainLayerTexture(k);
			}
			ImGui::PopID();
		}
	}
	// A scene whose terrain is still a tile object per cell (or per child):
	// offer to make it one Terrain object.
	{
		bool legacy = false;
		const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
		for (size_t i = 0; i < tiles.size() && !legacy; i++)
		{
			GameObject* parent = tiles[i].owner ? tiles[i].owner->GetParent() : NULL;
			legacy = !(parent && TerrainOn(parent));
		}
		if (legacy && ImGui::CollapsingHeader("Terrain Object##terrain_convert", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextWrapped("This scene keeps each terrain tile as an object of its own. Converting makes the terrain one object "
				"that streams its tiles itself. It saves the scene and rewrites its files; the maps are not touched.");
			if (ImGui::Button("Convert to Terrain Object")) pendingFoliageOp = { { "cmd", "terrain_convert" } };
		}
	}
	if (ImGui::CollapsingHeader("Grass Preset##terrain_foliage"))
	{
		ImGui::TextWrapped("Adds a grass layer to every tile of this terrain. Paint where it grows with the Foliage tool.");
		// Runs between frames: it rebuilds the object this window is showing.
		if (ImGui::Button("Add Grass") && SelectedSceneObject)
			pendingFoliageOp = { { "cmd", "terrain_add_grass" }, { "id", SelectedSceneObject->GetParentID() } };
	}
}

void SceneEditor::ShowNewTerrainWindow()
{
	json &pendingCreate = g_pendingTerrainCreate;
	json &pendingImport = g_pendingTerrainImport;
	std::string &lastError = g_terrainCreateError;
	if (showNewTerrain && !sceneIsTwoD)
	{
		ImGui::SetNextWindowSize(ImVec2(340, 460), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("New Terrain", &showNewTerrain))
		{
			ImGui::TextWrapped("Makes a Terrain object. Select its Terrain component in the tree to sculpt and paint it.");
	if (ImGui::CollapsingHeader("Import Heightmap##terrain_import"))
	{
		static std::string heightmap;
		static float worldSize = 1024.f, tileSize = 256.f, range = 300.f, base = 0.f;
		static int samplesIndex = 2;
		static const char* sampleLabels[] = { "65", "129", "257", "513" };
		static const int sampleValues[] = { 65, 129, 257, 513 };
		ImGui::InputTextWithHint("Image", "assets/terrain/height.png", &heightmap);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_REL")) heightmap = std::string((const char*)payload->Data);
			ImGui::EndDragDropTarget();
		}
		ImGui::InputFloat("World size (m)", &worldSize, 64.f, 256.f, "%.0f");
		ImGui::InputFloat("Tile size (m)", &tileSize, 16.f, 64.f, "%.0f");
		ImGui::InputFloat("Height range (m)", &range, 10.f, 50.f, "%.0f");
		ImGui::InputFloat("Base height (m)", &base, 1.f, 10.f, "%.1f");
		ImGui::Combo("Samples per tile", &samplesIndex, sampleLabels, 4);
		const int across = std::max(1, (int)std::lround(worldSize / std::max(1.f, tileSize)));
		ImGui::TextDisabled("%d x %d tiles. A square 8 or 16-bit image; black = base.", across, across);
		if (ImGui::Button("Import"))
			pendingImport = { { "name", "Terrain" }, { "heightmap", heightmap }, { "worldSize", worldSize }, { "tileSize", tileSize },
				{ "heightRange", range }, { "baseHeight", base }, { "samples", sampleValues[samplesIndex] } };
	}
	if (ImGui::CollapsingHeader("Create Terrain##terrain_create", ImGuiTreeNodeFlags_DefaultOpen))
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
		// How the ground starts out.
		static int generatorIndex = 1;
		static const char* generatorLabels[] = { "Flat", "Hills (Perlin noise)", "Mountains (ridged noise)" };
		static const char* generatorNames[] = { "flat", "perlin", "ridged" };
		static int seed = 1;
		ImGui::Combo("Ground", &generatorIndex, generatorLabels, 3);
		if (generatorIndex != 0)
		{
			ImGui::InputInt("Seed", &seed);
			ImGui::SameLine();
			if (ImGui::SmallButton("Random")) seed = (int)(ImGui::GetTime() * 1000.0) % 100000 + 1;
			ImGui::DragFloat("Feature size (m)", &params.featureSize, 5.f, 10.f, 20000.f, "%.0f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far it is from one hill to the next.");
			ImGui::SliderInt("Detail (octaves)", &params.octaves, 1, 8);
			ImGui::SliderFloat("Roughness", &params.roughness, 0.2f, 0.8f, "%.2f");
			ImGui::SliderFloat("Height used", &params.amount, 0.05f, 0.9f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much of the height range the hills span.");
		}
		ImGui::TextDisabled("%.0f x %.0f m, centred on the origin.", params.tilesX * params.tileSize, params.tilesZ * params.tileSize);
		if (ImGui::Button("Create"))
		{
			params.name = name;
			params.samples = sampleValues[samplesIndex];
			json args;
			args["generator"] = generatorNames[generatorIndex];
			args["seed"] = std::max(0, seed);
			args["featureSize"] = params.featureSize;
			args["octaves"] = params.octaves;
			args["roughness"] = params.roughness;
			args["amount"] = params.amount;
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
		}
		ImGui::End();
	}
	// After End(): building objects inside another window's Begin/End pair
	// is how ImGui asserts.
	json args, out;
	if (!pendingImport.is_null()) args.swap(pendingImport);
	else if (!pendingCreate.is_null()) args.swap(pendingCreate);
	if (args.is_null()) return;
	lastError.clear();
	if (!AgentTerrain("terrain_create", args, out, lastError)) { if (lastError.empty()) lastError = "failed"; return; }
	// Made: straight to editing it.
	showNewTerrain = false;
	if (SelectedSceneObject)
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); ++i)
			if (i->second && i->second->GetType() == SceneObjectTypes::TERRAIN_COMPONENT && i->second->GetParentID() == SelectedSceneObject->GetID())
			{ SelectSceneObject(i->second); break; }
}

bool SceneEditor::AgentTerrain(const std::string& command, const json& a, json& out, std::string& errOut)
{
	TerrainTools &t = Terrain();
	const json args = a.is_object() ? a : json::object();

	if (command == "terrain_state")
	{
		out = t.State(scene);
		// {"grid":{"x0":..,"z0":..,"step":..,"nx":..,"nz":..}}: heights in one
		// call, row-major over z then x (null where there is no terrain).
		// One call per sample is ~12 ms over the socket, which makes any
		// map of the ground - a shoreline mask, a slope chart - unusable.
		if (args.contains("grid") && args["grid"].is_object())
		{
			const json &g = args["grid"];
			const f32 x0 = g.value("x0", 0.f), z0 = g.value("z0", 0.f), step = std::max(0.01f, g.value("step", 1.f));
			const int nx = std::min(2048, std::max(1, g.value("nx", 1))), nz = std::min(2048, std::max(1, g.value("nz", 1)));
			json rows = json::array();
			for (int iz = 0; iz < nz; iz++)
			{
				json row = json::array();
				for (int ix = 0; ix < nx; ix++)
				{
					f32 h;
					if (TerrainEditor::HeightAt(scene, x0 + ix * step, z0 + iz * step, h)) row.push_back(h);
					else row.push_back(nullptr);
				}
				rows.push_back(row);
			}
			out["grid"] = rows;
		}
		if (args.contains("x") && args.contains("z"))
		{
			f32 h;
			if (TerrainEditor::HeightAt(scene, args.value("x", 0.f), args.value("z", 0.f), h)) out["height"] = h;
			else out["height"] = nullptr;
			// With "y" too: how much of that point is cave air (0..1), and
			// whether the ground above it is open.
			if (args.contains("y"))
			{
				const Vec3 at(args.value("x", 0.f), args.value("y", 0.f), args.value("z", 0.f));
				f32 air = 0.f;
				const std::vector<TerrainComponent*> terrains = SceneTerrains();
				for (size_t i = 0; i < terrains.size(); i++) air = std::max(air, terrains[i]->AirAt(at));
				out["air"] = air;
			}
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
			{ errOut = "tool must be raise, lower, smooth, flatten, paint, foliage, place, hole, fill, dig, pack, cavesmooth or cavelevel"; return false; }
			t.tool = tool;
		}
		t.radius = std::max(0.1f, args.value("radius", t.radius));
		t.strength = std::min(1.f, std::max(0.f, args.value("strength", t.strength)));
		t.hardness = std::min(1.f, std::max(0.f, args.value("hardness", t.hardness)));
		t.layer = std::max(0, args.value("layer", t.layer));
		t.density = std::min(1.f, std::max(0.f, args.value("density", t.density)));
		t.placeAsset = args.value("asset", t.placeAsset);
		t.placeSpacing = std::max(0.1f, args.value("spacing", t.placeSpacing));
		t.placeScaleMin = std::max(0.01f, args.value("scaleMin", t.placeScaleMin));
		t.placeScaleMax = std::max(t.placeScaleMin, args.value("scaleMax", t.placeScaleMax));
		t.placeRandomYaw = args.value("randomYaw", t.placeRandomYaw);
		t.placeAlign = args.value("alignToSlope", t.placeAlign);
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
		const f32 x0 = pts[0].at(0).get<f32>(), z0 = pts[0].at(pts[0].size() >= 3 ? 2 : 1).get<f32>();
		f32 h0;
		// A streamed world: the cells under the stroke come in now, not a
		// few frames from now - until they do the ground there is a far
		// version, which no brush touches.
		if (editorWorld)
		{
			std::vector<Vec3> foci;
			bool missing = false;
			for (size_t i = 0; i < pts.size(); i++)
			{
				foci.push_back(Vec3(pts[i].at(0).get<f32>(), 0.f, pts[i].at(pts[i].size() >= 3 ? 2 : 1).get<f32>()));
				int32 cx, cz;
				editorWorld->CellOf(foci.back(), cx, cz);
				if (editorWorld->HasCell(cx, cz) && !editorWorld->CellRoot(cx, cz)) missing = true;
			}
			if (missing) editorWorld->LoadAround(foci);
		}
		{
			std::vector<Vec3> foci;
			for (size_t i = 0; i < pts.size(); i++) foci.push_back(Vec3(pts[i].at(0).get<f32>(), 0.f, pts[i].at(pts[i].size() >= 3 ? 2 : 1).get<f32>()));
			const std::vector<TerrainComponent*> terrains = SceneTerrains();
			for (size_t k = 0; k < terrains.size(); k++)
			{
				// Every tile the brush reaches, not only the one under its
				// centre: a dab near a border edits both sides of it.
				bool missing = false;
				for (size_t i = 0; i < foci.size() && !missing; i++)
					for (int corner = 0; corner < 5 && !missing; corner++)
					{
						const Vec3 at = foci[i] + Vec3(corner == 1 ? t.radius : (corner == 2 ? -t.radius : 0.f), 0.f,
							corner == 3 ? t.radius : (corner == 4 ? -t.radius : 0.f));
						int32 tx, tz;
						missing = terrains[k]->TileAt(at, tx, tz) && !terrains[k]->GetTile(tx, tz);
					}
				if (missing) terrains[k]->LoadAround(foci);
			}
		}
		if (!TerrainEditor::HeightAt(scene, x0, z0, h0)) { errOut = "the first point is not over terrain"; return false; }
		// Stand in for the cursor at the first point, so Flatten picks up
		// its height exactly as a click there would.
		const bool wasActive = t.active;
		t.active = true;
		t.Update(scene, true, Vec3(x0, h0 + 1000.f, z0), Vec3(0.f, -1.f, 0.f), 0.f);
		if (!BeginTerrainStroke()) { t.active = wasActive; errOut = "could not start a stroke there"; return false; }
		uint32 dabs = 0;
		for (size_t i = 0; i < pts.size(); i++)
		{
			const f32 px = pts[i].at(0).get<f32>(), pz = pts[i].at(pts[i].size() >= 3 ? 2 : 1).get<f32>();
			if (t.tool == TerrainTools::Place)
			{
				if (!PlaceDab(px, pz, errOut)) { EndTerrainStroke(); t.active = wasActive; return false; }
				dabs++;
			}
			// A cave tool takes [x, y, z]; with two numbers it works at the
			// surface.
			else if ((t.IsCaveTool() || t.IsHoleTool()) && pts[i].size() >= 3)
			{
				// The floor tool levels to the first point's height, or to
				// "level" when given.
				if (i == 0 && t.tool == TerrainTools::CaveLevel) t.SetLevel(args.value("level", pts[0].at(1).get<f32>()));
				const Vec3 at(px, t.tool == TerrainTools::CaveLevel ? t.Level() : pts[i].at(1).get<f32>(), pts[i].at(2).get<f32>());
				if (t.ApplyAt3D(scene, at, dt)) dabs++;
			}
			else if (t.ApplyAt(scene, px, pz, dt)) dabs++;
		}
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
		// Generated ground: {"generator": "flat"|"perlin"|"ridged", "seed",
		// "featureSize" (m), "octaves", "roughness" 0..1, "amount" 0..0.9,
		// "baseHeight"}.
		p.generator = args.value("generator", p.generator);
		p.seed = args.value("seed", p.seed);
		p.featureSize = args.value("featureSize", p.featureSize);
		p.octaves = args.value("octaves", p.octaves);
		p.roughness = args.value("roughness", p.roughness);
		p.amount = args.value("amount", p.amount);
		p.baseHeight = args.value("baseHeight", p.baseHeight);
		// Import: {"heightmap": project-relative or absolute path, "worldSize":
		// metres the image covers, "baseHeight"}; tiles = worldSize / tileSize.
		if (args.contains("heightmap"))
		{
			const std::string hm = args.value("heightmap", std::string());
			p.importPath = std::filesystem::path(hm).is_absolute() ? hm : project->AbsolutePath(hm);
			p.baseHeight = args.value("baseHeight", 0.f);
			if (args.contains("worldSize"))
			{
				const int tiles = std::max(1, (int)std::lround(args.value("worldSize", 0.f) / std::max(1.f, p.tileSize)));
				p.tilesX = p.tilesZ = tiles;
			}
		}
		if (args.contains("origin") && args["origin"].is_array() && args["origin"].size() >= 3)
			p.origin = Vec3(args["origin"][0].get<f32>(), args["origin"][1].get<f32>(), args["origin"][2].get<f32>());
		std::string subtree;
		if (!TerrainTools::CreateTerrain(p, project->GetProjectPath(), subtree, errOut)) return false;
		SceneObject* obj = RawInsertSubtree(subtree, 0, false, EditorCameraSettings(), true);
		if (!obj) { errOut = "the terrain's maps were written, but it could not be built"; return false; }
		// The terrain is one object in the scene file, streamed world or
		// not: it brings its own tiles in. What is drawn past them comes
		// from the overview, baked now from the maps just written.
		if (TerrainComponent* tc = FindTerrain((GameObject*)obj->GetPTR()))
		{
			if (tc->GetAssetRoot().empty()) tc->SetAssetRoot(project->GetProjectPath());
			std::string bakeErr;
			if (!tc->BakeOverview(NULL, bakeErr)) echo("ERROR: baking the terrain overview - " + bakeErr);
		}
		PushAddCommand(obj);
		SelectSceneObject(obj);
		MarkSceneDirty();
		out = t.State(scene);
		out["name"] = obj->GetName();
		return true;
	}
	// {"layer":0-3,"texture":"assets/..."} - a ground layer on every splat
	// tile. Without "texture": reads all four.
	if (command == "terrain_layer")
	{
		if (args.contains("texture"))
		{
			if (!SetTerrainLayerTexture(args.value("layer", 0), args.value("texture", std::string()), true, errOut)) return false;
		}
		out["layers"] = json::array();
		for (int k = 0; k < 4; k++) out["layers"].push_back(TerrainLayerTexture(k));
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
		const int tiles = TerrainTools::AddGrassLayer(TileSubtree(tree), project->GetProjectPath(), errOut);
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
		visit(TileSubtree(tree)["root"]);
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
		// A Terrain object: the layers every tile grows, from its template;
		// instances are those of the tiles loaded now.
		if (TerrainComponent* tc = TerrainOn((GameObject*)target->GetPTR()))
		{
			json tmpl;
			try { tmpl = json::parse(tc->GetSettings().tileTemplate); }
			catch (const std::exception &e) { errOut = e.what(); return false; }
			if (json* layers = TemplateFoliageLayers(tmpl))
				for (size_t i = 0; i < layers->size(); i++)
				{
					FoliageLayerSpec spec;
					PatchSpec(spec, (*layers)[i]);
					spec.name = (*layers)[i].value("name", std::string());
					json l = SpecJson(spec);
					l.erase("densityMap");
					uint32 instances = 0, blocks = 0;
					const std::vector<std::shared_ptr<GameObject> > &kids = target ? ((GameObject*)target->GetPTR())->GetChildren() : std::vector<std::shared_ptr<GameObject> >();
					for (size_t k = 0; k < kids.size(); k++)
						if (FoliageComponent* tf = FindFoliage(kids[k].get()))
							if (i < tf->GetLayers().size())
							{
								for (size_t b = 0; b < tf->GetLayers()[i].counts.size(); b++) instances += tf->GetLayers()[i].counts[b];
								blocks += (uint32)tf->GetLayers()[i].blocks.size();
							}
					l["instances"] = instances;
					l["blocks"] = blocks;
					out["layers"].push_back(l);
				}
			return true;
		}
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
		// A Terrain object: the layer is in its template, and every tile
		// is made again from it - one undo entry, like adding a layer.
		if (TerrainOn((GameObject*)target->GetPTR()))
		{
			if (playMode) { errOut = "stop play mode first"; return false; }
			const uint32 tid = target->GetID();
			const std::string before = SnapshotSubtree(tid);
			json tree;
			try { tree = json::parse(before); }
			catch (const std::exception &e) { errOut = e.what(); return false; }
			json* layers = TemplateFoliageLayers(TileSubtree(tree));
			const int index = args.value("layer", 0);
			if (!layers || index < 0 || (size_t)index >= layers->size()) { errOut = "no foliage layer " + std::to_string(index) + " there"; return false; }
			json &lj = (*layers)[index];
			FoliageLayerSpec spec;
			PatchSpec(spec, lj);
			spec.name = lj.value("name", std::string());
			PatchSpec(spec, args);
			const json now = SpecJson(spec);
			for (json::const_iterator it = now.begin(); it != now.end(); ++it)
				if (it.key() != "densityMap") lj[it.key()] = it.value();
			const bool wasCamera = IsSceneCamera(tid);
			std::unique_ptr<ReplaceGameObjectCommand> cmd(new ReplaceGameObjectCommand(this, target->GetParentID(), before, tree.dump(),
				wasCamera, wasCamera ? sceneCameras[tid] : EditorCameraSettings(), target->Helper != nullptr, tid, "Edit Foliage Layer"));
			cmd->Redo();
			sceneUndo.Push(std::move(cmd));
			MarkSceneDirty();
			return AgentTerrain("get_foliage", args, out, errOut);
		}
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
		json &comps = TileSubtree(tree)["root"]["components"];
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

bool SceneEditor::BakeModelImpostor(const json& a, json& out, std::string& errOut)
{
	const json args = a.is_object() ? a : json::object();
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
	const std::string modelRel = args.value("model", std::string());
	if (modelRel.empty()) { errOut = "bake_model_impostor needs {\"model\": \"assets/...p3dm\"}"; return false; }
	std::vector<unsigned char> rgba;
	uint32 w = 0, h = 0;
	f32 left, right, bottom, top;
	if (!RenderImpostorRGBA8(project->AbsolutePath(modelRel), rgba, w, h, left, right, bottom, top))
	{ errOut = "could not render " + modelRel; return false; }
	namespace fs = std::filesystem;
	const std::string texRel = args.value("out", (fs::path(modelRel).parent_path() / (fs::path(modelRel).stem().string() + "_impostor.png")).generic_string());
	std::error_code ec;
	fs::create_directories(fs::path(project->AbsolutePath(texRel)).parent_path(), ec);
	if (!PaintableImage::WritePNG(project->AbsolutePath(texRel), (int32)w, (int32)h, 4, rgba.data()))
	{ errOut = "could not write " + texRel; return false; }
	Texture::ForgetShared(project->AbsolutePath(texRel));
	out["texture"] = texRel;
	out["width"] = w; out["height"] = h;
	out["left"] = left; out["right"] = right; out["bottom"] = bottom; out["top"] = top;
	return true;
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
	visit(TileSubtree(tree)["root"]);
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

	json &pool = TileSubtree(tree);
	if (!pool.contains("materials") || !pool["materials"].is_array()) pool["materials"] = json::array();
	json &materials = pool["materials"];
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
		const bool terrainObject = cmd == "set_terrain" || cmd == "terrain_bake_overview" || cmd == "terrain_convert"
			|| cmd == "terrain_generate_caves" || cmd == "terrain_resync_caves";
		const bool ok = cmd == "move_to_cell" ? AgentMoveToCell(op, out, err)
			: (network ? AgentNetwork(cmd, op, out, err)
				: (terrainObject ? AgentTerrainObject(cmd, op, out, err) : AgentTerrain(cmd, op, out, err)));
		if (!ok) echo("ERROR: " + err);
	}
}

namespace {
	// Loaded splat tiles' materials, with the index of sampler "layer<k>".
	void ForEachSplatLayer(SceneGraph* scene, const int layer, const std::function<void(CustomShaderMaterial*, size_t)> &fn)
	{
		const std::string name = "layer" + std::to_string(layer);
		const std::vector<TerrainTile> tiles = TerrainEditor::FindTiles(scene);
		std::set<CustomShaderMaterial*> seen;
		for (size_t t = 0; t < tiles.size(); t++)
		{
			if (!tiles[t].rendering) continue;
			const std::vector<RenderingMesh*> meshes = tiles[t].rendering->GetMeshes();
			for (size_t m = 0; m < meshes.size(); m++)
			{
				CustomShaderMaterial* cm = meshes[m] ? dynamic_cast<CustomShaderMaterial*>(meshes[m]->Material.get()) : NULL;
				if (!cm || !seen.insert(cm).second) continue;
				const std::vector<std::string> &names = cm->GetSamplerNames();
				for (size_t i = 0; i < names.size() && i < cm->textures.size(); i++)
					if (names[i] == name) fn(cm, i);
			}
		}
	}
}

std::string SceneEditor::TerrainLayerTexture(const int layer)
{
	std::string found;
	ForEachSplatLayer(scene, layer, [&found, this](CustomShaderMaterial* cm, size_t i) {
		if (found.empty() && cm->textures[i]) found = project ? project->RelativePath(cm->textures[i]->GetFilename()) : cm->textures[i]->GetFilename();
	});
	// No tile loaded to read it from: a Terrain object's template says.
	if (found.empty())
	{
		const std::string name = "layer" + std::to_string(layer);
		const std::vector<TerrainComponent*> terrains = SceneTerrains();
		for (size_t t = 0; t < terrains.size() && found.empty(); t++)
		{
			json tmpl;
			try { tmpl = json::parse(terrains[t]->GetSettings().tileTemplate); }
			catch (const std::exception &) { continue; }
			if (tmpl.contains("materials") && tmpl["materials"].is_array())
				for (const auto &m : tmpl["materials"])
					if (m.contains("samplers") && m["samplers"].is_array())
						for (const auto &smp : m["samplers"])
							if (found.empty() && smp.value("name", std::string()) == name) found = smp.value("texture", std::string());
		}
	}
	return found;
}

bool SceneEditor::SetTerrainLayerTexture(const int layer, const std::string& textureRel, const bool record, std::string& errOut)
{
	if (layer < 0 || layer > 3) { errOut = "layer must be 0..3"; return false; }
	if (playMode) { errOut = "stop play mode first"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
	const std::string abs = project->AbsolutePath(textureRel);
	if (textureRel.empty() || !std::filesystem::exists(abs)) { errOut = "no such texture: " + textureRel; return false; }
	const std::string before = TerrainLayerTexture(layer);

	// Loaded tiles, live. The same unit, a different texture: the sampler
	// keeps its index, so nothing else about the material changes.
	std::shared_ptr<Texture> tex = Texture::LoadShared(abs, TextureType::Texture, true, false);
	if (!tex) { errOut = "could not load " + textureRel; return false; }
	int changed = 0;
	ForEachSplatLayer(scene, layer, [&tex, &changed](CustomShaderMaterial* cm, size_t i) { cm->textures[i] = tex; changed++; });

	// Unloaded cells: their files, as JSON - no GPU work for cells nobody
	// is looking at. The material's sampler entry names the texture.
	int files = 0;
	if (editorWorld)
	{
		const std::string name = "layer" + std::to_string(layer);
		const std::vector<std::pair<int32_t, int32_t> > cells = editorWorld->Cells();
		for (size_t c = 0; c < cells.size(); c++)
		{
			if (editorWorld->CellRoot(cells[c].first, cells[c].second)) continue;	// done live above
			if (editorWorld->IsLoading(cells[c].first, cells[c].second)) { errOut = "a cell is loading - try again"; return false; }
			const std::string path = editorWorld->Streamer().CellPath(cells[c].first, cells[c].second);
			std::ifstream in(path.c_str(), std::ios::binary);
			if (!in) continue;
			json tree;
			try { in >> tree; }
			catch (const std::exception &) { continue; }
			in.close();
			bool touched = false;
			if (tree.contains("materials") && tree["materials"].is_array())
				for (auto &m : tree["materials"])
					if (m.contains("samplers") && m["samplers"].is_array())
						for (auto &smp : m["samplers"])
							if (smp.value("name", std::string()) == name) { smp["texture"] = textureRel; touched = true; }
			if (!touched) continue;
			std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
			out << tree.dump();
			if (!out) { errOut = "could not write " + path; return false; }
			files++;
		}
	}
	// Terrain objects: their template names the texture every tile to come
	// will use. The tiles loaded now were retextured live, above; the
	// overview is coloured from the layers and is baked again on save.
	{
		const std::string name = "layer" + std::to_string(layer);
		const std::vector<TerrainComponent*> terrains = SceneTerrains();
		for (size_t t = 0; t < terrains.size(); t++)
		{
			json tmpl;
			try { tmpl = json::parse(terrains[t]->GetSettings().tileTemplate); }
			catch (const std::exception &) { continue; }
			bool touched = false;
			if (tmpl.contains("materials") && tmpl["materials"].is_array())
				for (auto &m : tmpl["materials"])
					if (m.contains("samplers") && m["samplers"].is_array())
						for (auto &smp : m["samplers"])
							if (smp.value("name", std::string()) == name) { smp["texture"] = textureRel; touched = true; }
			if (!touched) continue;
			terrains[t]->SetTileTemplate(tmpl.dump(), false);
			terrainOverviewStale.insert(sceneObjects->GetSceneObjectID(terrains[t]->GetOwner()));
			files++;
		}
	}
	if (changed == 0 && files == 0) { errOut = "no splat terrain tiles use layer" + std::to_string(layer); return false; }
	if (record && !before.empty() && before != textureRel)
	{
		sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
			[this, layer, before]() { std::string e; SetTerrainLayerTexture(layer, before, false, e); },
			[this, layer, textureRel]() { std::string e; SetTerrainLayerTexture(layer, textureRel, false, e); },
			"Set Terrain Layer " + std::to_string(layer)));
	}
	MarkSceneDirty();
	// Far versions are coloured from the layers' averages.
	if (editorWorld && sceneWorld.farRadius > 0.f)
	{
		std::string e;
		if (BakeFarCells(true, farResolution, e) < 0) echo("ERROR: baking far versions - " + e);
	}
	return true;
}
