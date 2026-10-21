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
#include <cctype>
#include <cstdio>
#include <iterator>

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
			|| a.sink != b.sink || a.seed != b.seed || a.densityMap != b.densityMap
			|| a.patchMap != b.patchMap || a.patchSize != b.patchSize || a.patchFrom != b.patchFrom || a.patchTo != b.patchTo
			|| a.patchKeep != b.patchKeep || a.patchTint != b.patchTint;
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
			{ "lodDistance", s.lodDistance }, { "castShadows", s.castShadows }, { "densityMap", s.densityMap },
			{ "thinFrom", s.thinFrom }, { "thinTo", s.thinTo }, { "thinDensity", s.thinDensity },
			{ "patchMap", s.patchMap }, { "patchSize", s.patchSize }, { "patchFrom", s.patchFrom }, { "patchTo", s.patchTo },
			{ "patchKeep", s.patchKeep }, { "patchTint", s.patchTint } };
	}

	// The open project's folder: what a layer's patch map is named from.
	std::string g_specAssetRoot;

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
		s.thinFrom = std::max(0.f, j.value("thinFrom", s.thinFrom));
		s.thinTo = std::max(s.thinFrom, j.value("thinTo", s.thinTo));
		s.thinDensity = std::min(1.f, std::max(0.05f, j.value("thinDensity", s.thinDensity)));
		if (j.contains("patchMap") && j["patchMap"].is_string() && j["patchMap"].get<std::string>() != s.patchMap)
		{
			s.patchMap = j["patchMap"].get<std::string>();
			s.patchMapPath = (s.patchMap.empty() || g_specAssetRoot.empty()) ? std::string() : g_specAssetRoot + "/" + s.patchMap;
		}
		s.patchSize = std::max(0.f, j.value("patchSize", s.patchSize));
		s.patchFrom = j.value("patchFrom", s.patchFrom);
		s.patchTo = std::max(s.patchFrom, j.value("patchTo", s.patchTo));
		s.patchKeep = std::min(1.f, std::max(0.f, j.value("patchKeep", s.patchKeep)));
		s.patchTint = std::min(1.f, std::max(0.f, j.value("patchTint", s.patchTint)));
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
		case TerrainTools::Erode: return "Erode Terrain";
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
	if (project && project->IsOpen()) { terrainTools->SetAssetRoot(project->GetProjectPath()); g_specAssetRoot = project->GetProjectPath(); }
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

bool SceneEditor::Populate(const f32 cx, const f32 cz, const f32 radius, const uint32 count, const f32 spacing, const uint32 seed,
	const f32 sea, const nlohmann::json &rules, uint32 &planted, std::string &errOut)
{
	TerrainTools &t = Terrain();
	planted = 0;
	uint32 dice = seed * 2654435761u + 99u;
	auto rnd = [&dice]() { dice ^= dice << 13; dice ^= dice >> 17; dice ^= dice << 5; return (f32)(dice & 0xFFFFFF) / 16777216.f; };
	const std::string keptAsset = t.placeAsset;
	const f32 keptSpacing = t.placeSpacing;
	t.placeSpacing = 0.f;
	sceneUndo.BeginGroup("Populate");
	std::vector<Vec2> at;
	bool ok = true;
	for (uint32 i = 0; i < count && ok; i++)
	{
		const f32 a = rnd() * 6.2831853f, r = std::sqrt(rnd()) * radius;
		const f32 x = cx + std::cos(a) * r, z = cz + std::sin(a) * r;
		const f32 lot = rnd();
		f32 h; Vec3 n;
		if (!TerrainTools::GroundPoint(scene, x, z, h, n)) continue;
		const f32 slope = std::acos(std::min(1.f, std::max(0.f, n.y))) * 57.29578f;
		bool clear = true;
		for (size_t k = 0; k < at.size() && clear; k++) clear = (at[k].x - x) * (at[k].x - x) + (at[k].y - z) * (at[k].y - z) >= spacing * spacing;
		if (!clear) continue;
		// which rules fit here, and one of them by its weight
		f32 total = 0.f;
		std::vector<std::pair<f32, std::string> > fit;
		for (const auto &rule : rules)
		{
			if (!rule.is_object() || !rule.contains("asset")) continue;
			const f32 above = h - (sea > -1e8f ? sea : 0.f);
			if (above < rule.value("minHeight", -1e9f) || above > rule.value("maxHeight", 1e9f) || slope > rule.value("maxSlope", 40.f)) continue;
			if (rule.value("edge", false) != (r > radius * 0.8f) && rule.value("edge", false)) continue;
			const f32 w = rule.value("weight", 1.f);
			if (w <= 0.f) continue;
			total += w; fit.push_back(std::make_pair(w, rule["asset"].get<std::string>()));
		}
		if (fit.empty()) continue;
		f32 pick = lot * total;
		size_t which = 0;
		while (which + 1 < fit.size() && pick > fit[which].first) { pick -= fit[which].first; which++; }
		t.placeAsset = fit[which].second;
		placeLastValid = false;
		const size_t before = sceneObjects->GetList().size();
		if (!PlaceDab(x, z, errOut)) { ok = false; break; }
		if (sceneObjects->GetList().size() > before) { planted++; at.push_back(Vec2(x, z)); }
	}
	sceneUndo.EndGroup();
	t.placeAsset = keptAsset; t.placeSpacing = keptSpacing;
	placeLastValid = false;
	return ok;
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

nlohmann::json SceneEditor::DepthBakeSettings(const std::string &texture) const
{
	std::map<std::string, PendingDepthBake>::const_iterator p = pendingDepthBakes.find(texture);
	if (p != pendingDepthBakes.end()) return p->second.settings;
	if (!project || !project->IsOpen() || texture.empty()) return nlohmann::json::object();
	std::ifstream side(project->AbsolutePath(texture) + ".bake.json");
	nlohmann::json j = nlohmann::json::parse(side, nullptr, false);
	return j.is_object() ? j : nlohmann::json::object();
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
		if (project && project->IsOpen()) { terrainTools->SetAssetRoot(project->GetProjectPath()); g_specAssetRoot = project->GetProjectPath(); }
		if (!terrainTools->Save())
		{
			echo("ERROR: saving terrain - a heightmap, splat or density map could not be written");
			return false;
		}
	}
	bool ok = true;
	for (std::map<std::string, PendingDepthBake>::iterator d = pendingDepthBakes.begin(); d != pendingDepthBakes.end(); ++d)
	{
		if (!project || !project->IsOpen()) break;
		const std::string file = project->AbsolutePath(d->first);
		if (!PaintableImage::WritePNG(file, (int32)d->second.res, (int32)d->second.res, 4, &d->second.pixels[0]))
		{
			echo("ERROR: saving the depth picture " + d->first);
			ok = false;
			continue;
		}
		std::ofstream side(file + ".bake.json");
		side << d->second.settings.dump(1, '\t') << "\n";
	}
	if (ok) pendingDepthBakes.clear();
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
			ImGui::DragFloat("Thin out from", &fs.thinFrom, 1.f, 0.f, 5000.f, "%.0f m"); send |= ImGui::IsItemDeactivatedAfterEdit();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Far off, fewer plants are drawn and each one left is drawn bigger,\nso the ground stays covered. What a field costs is how many plants it is.");
			ImGui::DragFloat("Thinned by", &fs.thinTo, 1.f, 0.f, 5000.f, "%.0f m"); send |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::SliderFloat("Plants kept", &fs.thinDensity, 0.1f, 1.f, "%.2f"); send |= ImGui::IsItemDeactivatedAfterEdit();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("1: no thinning.");
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

namespace {
	// Small ripples, as a tangent-space normal map that tiles: what the Water template reads as "waves".
	bool WriteWaterNormals(const std::string &path, const uint32 seed)
	{
		const int32 n = 512;
		auto lattice = [seed](int32 x, int32 y, const int32 period) -> f32 {
			x = ((x % period) + period) % period;
			y = ((y % period) + period) % period;
			uint32 h = (uint32)x * 374761393u + (uint32)y * 668265263u + seed * 2246822519u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return (f32)((h ^ (h >> 16)) & 0xffff) / 65535.f;
		};
		auto noise = [&](const f32 u, const f32 v, const int32 period) -> f32 {
			const f32 x = u * period, y = v * period;
			const int32 x0 = (int32)std::floor(x), y0 = (int32)std::floor(y);
			f32 fx = x - x0, fy = y - y0;
			fx = fx * fx * (3.f - 2.f * fx);
			fy = fy * fy * (3.f - 2.f * fy);
			const f32 a = lattice(x0, y0, period), b = lattice(x0 + 1, y0, period);
			const f32 c = lattice(x0, y0 + 1, period), d = lattice(x0 + 1, y0 + 1, period);
			return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
		};
		std::vector<f32> h((size_t)n * n);
		for (int32 y = 0; y < n; y++)
			for (int32 x = 0; x < n; x++)
			{
				const f32 u = (f32)x / n, v = (f32)y / n;
				h[(size_t)y * n + x] = noise(u, v, 12) * 0.5f + noise(u, v, 24) * 0.28f + noise(u, v, 48) * 0.15f + noise(u, v, 96) * 0.07f;
			}
		// (slopes scaled so the ripples lean as far as a photographed water's do: about 0.16 either way)
		std::vector<f32> gx(h.size()), gy(h.size());
		f64 sum2 = 0.0;
		for (int32 y = 0; y < n; y++)
			for (int32 x = 0; x < n; x++)
			{
				const size_t i = (size_t)y * n + x;
				gx[i] = h[(size_t)y * n + (x + 1) % n] - h[(size_t)y * n + (x + n - 1) % n];
				gy[i] = h[(size_t)((y + 1) % n) * n + x] - h[(size_t)((y + n - 1) % n) * n + x];
				sum2 += gx[i] * gx[i] + gy[i] * gy[i];
			}
		const f32 k = 0.16f / (f32)std::max(1e-9, std::sqrt(sum2 / (2.0 * h.size())));
		std::vector<uchar> px((size_t)n * n * 4);
		for (size_t i = 0; i < h.size(); i++)
		{
			Vec3 nn(-gx[i] * k, -gy[i] * k, 1.f);
			nn = nn.normalize();
			px[i * 4 + 0] = (uchar)std::lround((nn.x * 0.5f + 0.5f) * 255.f);
			px[i * 4 + 1] = (uchar)std::lround((nn.y * 0.5f + 0.5f) * 255.f);
			px[i * 4 + 2] = (uchar)std::lround((nn.z * 0.5f + 0.5f) * 255.f);
			px[i * 4 + 3] = 255;
		}
		return PaintableImage::WritePNG(path, n, n, 4, &px[0]);
	}
}

// The river being laid out: its points, and whether clicks in the view add to them.
static std::vector<Vec2> gRiverPoints;
static bool gRiverPick = false;

bool SceneEditor::TerrainRiverClick()
{
	if (!gRiverPick || !terrainTools || !terrainTools->HoverValid()) return false;
	gRiverPoints.push_back(Vec2(terrainTools->HoverPoint().x, terrainTools->HoverPoint().z));
	return true;
}

static bool gOpenRiverSection = false;

void SceneEditor::DrawTerrainRiverOverlay()
{
	if (gRiverPoints.empty() || !debugRenderer || !scene) return;
	const Vec4 blue(0.25f, 0.65f, 1.f, 1.f);
	Vec3 prev;
	for (size_t i = 0; i < gRiverPoints.size(); i++)
	{
		f32 h = 0.f; Vec3 n;
		TerrainTools::GroundPoint(scene, gRiverPoints[i].x, gRiverPoints[i].y, h, n);
		const Vec3 at(gRiverPoints[i].x, h + 0.2f, gRiverPoints[i].y);
		// a ring on the ground and a post, to be seen from afar
		Vec3 last;
		for (int k = 0; k <= 20; k++)
		{
			const f32 a = (f32)k / 20.f * 6.2831853f;
			const Vec3 p(at.x + cosf(a) * 1.6f, at.y, at.z + sinf(a) * 1.6f);
			if (k > 0) debugRenderer->drawLine(last, p, blue);
			last = p;
		}
		debugRenderer->drawLine(at, at + Vec3(0.f, 5.f, 0.f), blue);
		if (i > 0)
		{
			// the course between two points, laid on the ground
			Vec3 a = prev;
			for (int k = 1; k <= 12; k++)
			{
				const f32 f = (f32)k / 12.f;
				Vec3 b(prev.x + (at.x - prev.x) * f, 0.f, prev.z + (at.z - prev.z) * f);
				f32 bh = 0.f; Vec3 bn;
				TerrainTools::GroundPoint(scene, b.x, b.z, bh, bn);
				b.y = bh + 0.2f;
				debugRenderer->drawLine(a, b, blue);
				a = b;
			}
		}
		prev = at;
	}
}

void SceneEditor::DrawTerrainTools()
{
	if (sceneIsTwoD) return;
	TerrainTools &t = Terrain();
	// The ring is grey over ground that is still a far version; say why.
	if (t.HoverValid() && !t.HoverEditable())
		ImGui::TextColored(ImVec4(1.f, 0.8f, 0.3f, 1.f), "Loading the cell under the brush...");

	static const char* labels[TerrainTools::ToolCount] = { "Raise", "Lower", "Smooth", "Flatten", "Texture", "Foliage", "Place", "Cut Hole", "Fill Hole", "Dig Cave", "Fill Cave", "Smooth Cave", "Level Floor", "Erode (rain)" };
	ImGui::TextDisabled("Sculpt");
	for (int i = 0; i < TerrainTools::ToolCount; i++)
	{
		// Two to a row, so a narrow panel does not clip the last one.
		if (i == TerrainTools::PaintTexture) ImGui::TextDisabled("Paint");
		else if (i == TerrainTools::Hole) ImGui::TextDisabled("Holes");
		else if (i == TerrainTools::Dig) ImGui::TextDisabled("Caves");
		else if (i == TerrainTools::Erode) ImGui::TextDisabled("Weather");
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
		static const char* layers[] = { "0  Grass", "1  Dirt", "2  Rock", "3  Sand", "4  None (the shader's own: stones)" };
		t.layer = std::max(0, std::min(4, t.layer));
		// (one button a layer, in its own colour: what is being painted shows at a glance)
		{
			static const char* names[] = { "Grass", "Dirt", "Rock", "Sand", "None" };
			static const ImVec4 tint[] = { ImVec4(0.30f, 0.46f, 0.20f, 1.f), ImVec4(0.42f, 0.32f, 0.22f, 1.f), ImVec4(0.45f, 0.45f, 0.47f, 1.f), ImVec4(0.78f, 0.70f, 0.52f, 1.f), ImVec4(0.25f, 0.25f, 0.27f, 1.f) };
			const float swatch = std::max(30.f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 4.f) / 5.f);
			for (int i = 0; i < 5; i++)
			{
				if (i) ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, tint[i]);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(tint[i].x + 0.08f, tint[i].y + 0.08f, tint[i].z + 0.08f, 1.f));
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, t.layer == i ? 2.f : 0.f);
				if (ImGui::Button(names[i], ImVec2(swatch, 34.f))) t.layer = i;
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(2);
			}
		}
		ImGui::Combo("Layer", &t.layer, layers, 5);
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
		// The palette: the project's prefabs and models, to pick from. A click takes one; with
		// Shift it joins (or leaves) the ones already taken - several are mixed by the brush's
		// Populate. The list is read when the tool is opened and on Refresh.
		{
			static std::vector<std::string> palette;
			static std::string paletteOf;
			static char paletteFilter[64] = "";
			const std::string root = project && project->IsOpen() ? project->AbsolutePath("") : std::string();
			bool refresh = ImGui::SmallButton("Refresh palette");
			if (refresh || paletteOf != root)
			{
				palette.clear(); paletteOf = root;
				if (!root.empty())
				{
					std::vector<ProjectAssetEntry> found;
					project->ListAssets("assets", found, true);
					for (size_t i = 0; i < found.size(); i++)
					{
						if (found[i].isDirectory) continue;
						const std::string &rel = found[i].relativePath;
						const size_t dot = rel.rfind('.');
						const std::string ext = dot == std::string::npos ? std::string() : rel.substr(dot);
						if (ext == ".prefab" || ext == ".p3dm") palette.push_back(rel);
					}
					std::sort(palette.begin(), palette.end());
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(140.f);
			ImGui::InputTextWithHint("##palette_filter", "filter (oak, rock...)", paletteFilter, sizeof(paletteFilter));
			std::set<std::string> taken;
			{
				std::string one;
				for (size_t c = 0; c <= t.placeAsset.size(); c++)
				{
					if (c == t.placeAsset.size() || t.placeAsset[c] == ',')
					{
						while (!one.empty() && one.front() == ' ') one.erase(one.begin());
						while (!one.empty() && one.back() == ' ') one.pop_back();
						if (!one.empty()) taken.insert(one);
						one.clear();
					}
					else one += t.placeAsset[c];
				}
			}
			std::string needle = paletteFilter;
			std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
			std::vector<const std::string*> shown;
			for (size_t i = 0; i < palette.size(); i++)
			{
				std::string low = palette[i];
				std::transform(low.begin(), low.end(), low.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
				if (needle.empty() || low.find(needle) != std::string::npos) shown.push_back(&palette[i]);
			}
			bool changed = false;
			ImGui::SameLine();
			if (ImGui::SmallButton("All shown")) { for (size_t i = 0; i < shown.size(); i++) taken.insert(*shown[i]); changed = true; }
			ImGui::SameLine();
			if (ImGui::SmallButton("None")) { taken.clear(); changed = true; }
			if (ImGui::BeginChild("##place_palette", ImVec2(0.f, 150.f), true))
			{
				for (size_t i = 0; i < shown.size(); i++)
				{
					const std::string &rel = *shown[i];
					const bool on = taken.count(rel) != 0;
					if (ImGui::Selectable(std::filesystem::path(rel).stem().string().c_str(), on))
					{
						if (ImGui::GetIO().KeyShift) { if (on) taken.erase(rel); else taken.insert(rel); }
						else { taken.clear(); taken.insert(rel); }
						changed = true;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", rel.c_str());
				}
				if (shown.empty()) ImGui::TextDisabled(palette.empty() ? "No prefabs or models in this project." : "Nothing matches.");
			}
			ImGui::EndChild();
			if (changed)
			{
				t.placeAsset.clear();
				for (std::set<std::string>::const_iterator it = taken.begin(); it != taken.end(); ++it) { if (!t.placeAsset.empty()) t.placeAsset += ", "; t.placeAsset += *it; }
			}
			ImGui::TextDisabled("%d taken of %d. Shift-click to take several; the brush picks among them.", (int)taken.size(), (int)palette.size());
		}
		ImGui::SliderFloat("Spacing", &t.placeSpacing, 0.5f, 100.f, "%.1f m", ImGuiSliderFlags_Logarithmic);
		ImGui::DragFloatRange2("Scale", &t.placeScaleMin, &t.placeScaleMax, 0.01f, 0.01f, 20.f, "%.2f", "%.2f");
		ImGui::Checkbox("Random yaw", &t.placeRandomYaw);
		ImGui::SameLine();
		ImGui::Checkbox("Align to slope", &t.placeAlign);
		ImGui::TextWrapped("Click places one; a drag places one per spacing. One undo step per stroke.");
		// Populate: many at once, over the brush's disc where the cursor last was - the asset
		// above, or several (separated by commas) mixed.
		static int populateCount = 60;
		ImGui::SliderInt("Populate count", &populateCount, 5, 2000, "%d", ImGuiSliderFlags_Logarithmic);
		if (ImGui::Button("Populate") && t.HoverValid())
		{
			nlohmann::json rules = nlohmann::json::array();
			std::string list = t.placeAsset, one;
			for (size_t c = 0; c <= list.size(); c++)
			{
				if (c == list.size() || list[c] == ',')
				{
					while (!one.empty() && one.front() == ' ') one.erase(one.begin());
					while (!one.empty() && one.back() == ' ') one.pop_back();
					if (!one.empty()) rules.push_back({ { "asset", one } });
					one.clear();
				}
				else one += list[c];
			}
			uint32 planted = 0; std::string err;
			static uint32 populateSeed = 1;
			if (!rules.empty() && !Populate(t.HoverPoint().x, t.HoverPoint().z, t.radius, (uint32)populateCount, std::max(0.5f, t.placeSpacing), populateSeed++, -1e9f, rules, planted, err) && !err.empty())
				echo("ERROR: Populate - " + err);
		}
		ImGui::SameLine(); ImGui::TextDisabled("over the brush, where the cursor last was");
	}
	// River: points from source to mouth, picked where the cursor last was on the ground;
	// Build cuts the bed along them (terrain_river) and makes the water's depth picture again.
	if (gOpenRiverSection) { ImGui::SetNextItemOpen(true); gOpenRiverSection = false; }
	if (ImGui::CollapsingHeader("River"))
	{
		std::vector<Vec2> &riverPoints = gRiverPoints;
		static float riverWidth = 10.f, riverDepth = 2.4f, riverBank = 9.f, riverWater = 0.f, riverStones = 0.f;
		static bool riverToLevel = false, riverClear = true;
		static char riverDepthTexture[256] = "";
		ImGui::SliderFloat("Width (m)", &riverWidth, 1.f, 60.f, "%.1f");
		ImGui::SliderFloat("Depth (m)", &riverDepth, 0.2f, 12.f, "%.1f");
		ImGui::SliderFloat("Bank width (m)", &riverBank, 0.f, 40.f, "%.1f");
		ImGui::SliderFloat("Bare banks (m)", &riverStones, 0.f, 12.f, "%.1f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clears the ground's four layers and what grows, this far past the water's edge:\nfor a terrain material that lays stones of its own where no layer is painted.");
		ImGui::Checkbox("Remove trees and rocks in the channel", &riverClear);
		ImGui::Checkbox("Down to a water level", &riverToLevel);
		if (riverToLevel) ImGui::InputFloat("Water level", &riverWater);
		ImGui::InputTextWithHint("Depth texture", "assets/textures/sea_depth.png (optional)", riverDepthTexture, sizeof(riverDepthTexture));
		ImGui::Checkbox("Click in the view to add points", &gRiverPick);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("From the source to the mouth. While this is on, a click on the ground\nis a point of the river and not a stroke of the brush.");
		if (ImGui::Button("Add point at cursor") && t.HoverValid()) riverPoints.push_back(Vec2(t.HoverPoint().x, t.HoverPoint().z));
		ImGui::SameLine();
		if (ImGui::Button("Remove last") && !riverPoints.empty()) riverPoints.pop_back();
		ImGui::SameLine();
		ImGui::Text("%d point(s)", (int)riverPoints.size());
		if (ImGui::Button("Build river") && riverPoints.size() >= 2)
		{
			nlohmann::json a;
			a["points"] = nlohmann::json::array();
			for (size_t i = 0; i < riverPoints.size(); i++) a["points"].push_back({ riverPoints[i].x, riverPoints[i].y });
			a["width"] = riverWidth; a["depth"] = riverDepth; a["bank"] = riverBank; a["stones"] = riverStones; a["clear"] = riverClear;
			if (riverToLevel) { a["level"] = riverWater - riverDepth; a["waterLevel"] = riverWater; }
			if (riverDepthTexture[0]) a["depthTexture"] = std::string(riverDepthTexture);
			nlohmann::json out; std::string err;
			if (!AgentTerrain("terrain_river", a, out, err)) echo("ERROR: River - " + err);
			else { riverPoints.clear(); gRiverPick = false; }
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { riverPoints.clear(); gRiverPick = false; }
		// The water's depth picture, made again from the ground as it now is (after sculpting a
		// shore, say): what a water shader reads for its shallows, its foam and where it ends.
		ImGui::Separator();
		ImGui::TextDisabled("Water depth picture");
		static float bakeLevel = 0.f, bakeRange = 12.f, bakeAbove = 0.f, bakeSize = 1024.f;
		static bool bakeSqrt = false, bakeFlow = false;
		// (a texture baked before brings the settings it was baked with)
		static std::string bakeSettingsOf;
		static bool bakeSettingsFound = false;
		if (bakeSettingsOf != riverDepthTexture)
		{
			bakeSettingsOf = riverDepthTexture;
			const nlohmann::json k = DepthBakeSettings(bakeSettingsOf);
			bakeSettingsFound = !k.empty();
			bakeLevel = k.value("level", bakeLevel); bakeRange = k.value("range", bakeRange); bakeAbove = k.value("above", bakeAbove);
			bakeSize = k.value("size", bakeSize); bakeFlow = k.value("flow", bakeFlow);
			if (k.contains("encoding")) bakeSqrt = k.value("encoding", std::string()) == "sqrt";
		}
		if (!bakeSettingsOf.empty() && !bakeSettingsFound)
			ImGui::TextDisabled("Not baked before: a river's Build bakes it linear, over 1024 m, at level 0.");
		ImGui::InputFloat("Water level##bake", &bakeLevel);
		ImGui::InputFloat("World size (m)##bake", &bakeSize);
		ImGui::SliderFloat("Deepest stored (m)", &bakeRange, 1.f, 60.f, "%.1f");
		ImGui::SliderFloat("Ground above water kept (m)", &bakeAbove, 0.f, 4.f, "%.2f");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stores the ground a little above the water too, so a shader can\nlet the water's edge run up the sand. 0: the picture ends at the waterline.");
		ImGui::Checkbox("Finer in the shallows (square root)", &bakeSqrt);
		ImGui::SameLine();
		ImGui::Checkbox("River flow", &bakeFlow);
		if (ImGui::Button("Bake depth picture"))
		{
			if (!riverDepthTexture[0]) echo("ERROR: Bake depth - name the depth texture above");
			else
			{
				nlohmann::json a;
				a["texture"] = std::string(riverDepthTexture);
				a["level"] = bakeLevel; a["range"] = bakeRange; a["size"] = bakeSize; a["above"] = bakeAbove;
				if (bakeSqrt) a["encoding"] = "sqrt";
				a["flow"] = bakeFlow;
				nlohmann::json out; std::string err;
				if (!AgentTerrain("terrain_bake_depth", a, out, err)) echo("ERROR: Bake depth - " + err);
			}
		}
		// A sea: a water surface at a level, with its own depth picture baked and its Sky's sea level set.
		ImGui::Separator();
		static float waterLevel = 0.f;
		ImGui::InputFloat("Water level##add_water", &waterLevel);
		if (ImGui::Button("Add water"))
		{
			nlohmann::json a, out; std::string err;
			a["level"] = waterLevel;
			if (riverDepthTexture[0]) a["texture"] = std::string(riverDepthTexture);
			if (!AgentTerrain("terrain_add_water", a, out, err)) echo("ERROR: Add water - " + err);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("A sea at this level: the Water material (written into the project), its depth\npicture baked from the ground (the depth texture above, or assets/textures/water_depth.png)\nand the Sky effect's sea level set to match - add a Sky first to have both.");
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
		static int surfaceIndex = 0;
		static const char* surfaceLabels[] = { "Plain (four layers)", "Natural (rock, snow, shore)" };
		ImGui::Combo("Surface", &surfaceIndex, surfaceLabels, 2);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Natural: rock on what is steep, cobbles where nothing is painted, snow\nabove a line the weather brings down, wet sand at the water. Its sea\nlevel is the Sky post effect's (add one to the scene).");
		ImGui::TextDisabled("%.0f x %.0f m, centred on the origin.", params.tilesX * params.tileSize, params.tilesZ * params.tileSize);
		if (ImGui::Button("Create"))
		{
			params.name = name;
			params.samples = sampleValues[samplesIndex];
			json args;
			args["generator"] = generatorNames[generatorIndex];
			args["ground"] = surfaceIndex == 1 ? "natural" : "plain";
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
		out["riverPoints"] = (uint32)gRiverPoints.size(); out["riverPick"] = gRiverPick;
		out["river"] = nlohmann::json::array();
		for (size_t i = 0; i < gRiverPoints.size(); i++) out["river"].push_back({ gRiverPoints[i].x, gRiverPoints[i].y });
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
		// "riverPick": clicks in the view add river points (the River tools' checkbox); "section": "river" opens them.
		if (args.contains("riverPick") && args["riverPick"].is_boolean()) gRiverPick = args["riverPick"].get<bool>();
		// "riverPoints": [[x,z],...] - the river being laid out, replaced ([] clears it, as Cancel does)
		if (args.contains("riverPoints") && args["riverPoints"].is_array())
		{
			gRiverPoints.clear();
			for (const auto &pt : args["riverPoints"])
				if (pt.is_array() && pt.size() >= 2) gRiverPoints.push_back(Vec2(pt.at(0).get<f32>(), pt.at(1).get<f32>()));
		}
		if (args.value("section", std::string()) == "river") gOpenRiverSection = true;
		// "edit": true - what a click on the Terrain component in the tree does: it becomes the
		// selection, and its tools open. (false: the selection is dropped.)
		if (args.contains("edit") && args["edit"].is_boolean())
		{
			if (!args["edit"].get<bool>()) SelectSceneObject(NULL);
			else
			{
				SceneObject* found = NULL;
				const std::map<uint32, SceneObject*>& list = sceneObjects->GetList();
				for (std::map<uint32, SceneObject*>::const_iterator i = list.begin(); i != list.end() && !found; i++)
					if (i->second && i->second->GetType() == SceneObjectTypes::TERRAIN_COMPONENT) found = i->second;
				if (!found) { errOut = "the scene has no Terrain component"; return false; }
				SelectSceneObject(found);
			}
		}
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
	// terrain_populate {x, z, radius, count, spacing, seed, sea, rules: [{asset, minHeight, maxHeight,
	// maxSlope (degrees), weight, edge}]}: a wood, not a row of trees - `count` tries over the disc,
	// each planting whichever of the rules fits the ground there (heights are above `sea`), nothing
	// nearer another than `spacing`. A rule with "edge": true is kept to the rim. One undo step.
	// terrain_river {points: [[x,z],...], width, depth, bank, level}: a river's bed cut along a line
	// from its source to its mouth - `width` across at the bottom, `depth` under the ground it runs
	// through (or, with "level", down to that world height: under a sea's level the sea fills it),
	// the banks eased out over `bank` metres. One undo step. Without "points", the river laid out in
	// the view (terrain_brush riverPick / riverPoints; terrain_state lists it as "river").
	// terrain_bake_depth {texture, level, range, size, resolution}: how far the ground is under a
	// level (a sea's, a lake's), as a picture - 0 at the level, white `range` metres under it -
	// written over the live texture a water material samples (by its path), a square `size` metres
	// across centred on the origin. What a water shader draws its shallows, shore and foam from,
	// made again after the ground has been cut. (terrain_river does it when given "depthTexture".)
	// The settings a picture was baked with are kept beside it (<texture>.bake.json): a later bake that
	// does not name them - a river's, say - makes the same picture its water shader decodes.
	if (command == "terrain_bake_depth" || (command == "terrain_river" && args.contains("depthTexture") && args.value("bakeOnly", false)))
	{
		const std::string path = args.value("texture", args.value("depthTexture", std::string()));
		nlohmann::json kept = DepthBakeSettings(path);
		for (const char* key : { "level", "range", "size", "resolution", "above", "encoding", "flow" })
			if (args.contains(key)) kept[key] = args[key];
		const f32 level = kept.value("level", 0.f), range = kept.value("range", 12.f), size = kept.value("size", 1024.f);
		const uint32 res = std::min(2048u, std::max(64u, kept.value("resolution", 1024u)));
		std::vector<std::shared_ptr<Texture> > live = Texture::FindShared(path);
		if (live.empty()) { errOut = "no loaded texture ends with '" + path + "'"; return false; }
		std::vector<uchar> px((size_t)res * res * 4, 255);
		// Green says "this water is a river" (a water shader draws a creek's banks and bed there):
		// kept from the picture on disk, and added to along "points" (terrain_river's own).
		PaintableImage was;
		const bool hadFile = project && project->IsOpen() && was.Load(project->AbsolutePath(path), 4);
		std::vector<Vec2> line;
		if (args.contains("points") && args["points"].is_array())
			for (const auto &pt : args["points"]) if (pt.is_array() && pt.size() >= 2) line.push_back(Vec2(pt.at(0).get<f32>(), pt.at(pt.size() >= 3 ? 2 : 1).get<f32>()));
		const f32 reach = args.value("width", 10.f) * 0.5f + args.value("bank", 9.f) + 6.f;
		for (uint32 r = 0; r < res; r++)
			for (uint32 c = 0; c < res; c++)
			{
				f32 h = level - range;
				TerrainEditor::HeightAt(scene, ((f32)c + 0.5f) / (f32)res * size - size * 0.5f, ((f32)r + 0.5f) / (f32)res * size - size * 0.5f, h);
				// ("above": metres of ground over the water's level kept too - a water that runs up its shore needs them)
				const f32 above = std::max(0.f, kept.value("above", 0.f));
				const f32 d = std::min(1.f, std::max(0.f, (level - h + above) / (range + above)));
				uchar* p = &px[((size_t)r * res + c) * 4];
				// ("encoding": "sqrt" keeps the depth as its square root - fine steps in the shallows, where a
				// shore's foam is drawn. "flow": true keeps blue and alpha as the way a river runs there, 0.5 = none.)
				const bool rooted = kept.value("encoding", std::string()) == "sqrt", flowing = kept.value("flow", false);
				p[0] = (uchar)((rooted ? std::sqrt(d) : d) * 255.f + 0.5f);
				p[2] = flowing ? (hadFile ? (uchar)(was.Sample(((f32)c + 0.5f) / (f32)res, ((f32)r + 0.5f) / (f32)res, 2) * 255.f + 0.5f) : 128) : p[0];
				if (flowing) p[3] = hadFile ? (uchar)(was.Sample(((f32)c + 0.5f) / (f32)res, ((f32)r + 0.5f) / (f32)res, 3) * 255.f + 0.5f) : 128;
				f32 river = hadFile ? was.Sample(((f32)c + 0.5f) / (f32)res, ((f32)r + 0.5f) / (f32)res, 1) : 0.f;
				if (!line.empty())
				{
					const f32 wx = ((f32)c + 0.5f) / (f32)res * size - size * 0.5f, wz = ((f32)r + 0.5f) / (f32)res * size - size * 0.5f;
					for (size_t i = 0; i + 1 < line.size(); i++)
					{
						const Vec2 ab(line[i + 1].x - line[i].x, line[i + 1].y - line[i].y);
						const f32 l2 = ab.x * ab.x + ab.y * ab.y;
						const f32 tt = l2 > 0.f ? std::min(1.f, std::max(0.f, ((wx - line[i].x) * ab.x + (wz - line[i].y) * ab.y) / l2)) : 0.f;
						const f32 dx = wx - (line[i].x + ab.x * tt), dz = wz - (line[i].y + ab.y * tt);
						const f32 near_ = std::min(1.f, std::max(0.f, (reach - std::sqrt(dx * dx + dz * dz)) / 6.f));
						if (flowing && near_ > river && l2 > 0.f)
						{
							// (the new river's water runs along the line it was drawn with)
							const f32 il = 1.f / std::sqrt(l2);
							p[2] = (uchar)((0.5f + 0.5f * ab.x * il) * 255.f + 0.5f); p[3] = (uchar)((0.5f + 0.5f * ab.y * il) * 255.f + 0.5f);
						}
						river = std::max(river, near_);
					}
				}
				p[1] = (uchar)(std::min(1.f, river) * 255.f + 0.5f);
			}
		for (size_t i = 0; i < live.size(); i++)
		{
			live[i]->Resize(res, res);
			live[i]->UpdateData(&px[0]);
			live[i]->UpdateMipmap();
		}
		// (written to the project with the scene's Save, as the ground it was made from is)
		PendingDepthBake &pending = pendingDepthBakes[path];
		pending.res = res; pending.pixels.swap(px); pending.settings = kept;
		MarkSceneDirty();
		out["textures"] = (uint32)live.size();
		out["settings"] = kept;
		return true;
	}
	if (command == "terrain_river" && !args.contains("points") && gRiverPoints.size() >= 2)
	{
		// (no points given: the river laid out in the view, as the panel's Build does - and done with, as there)
		nlohmann::json a = args;
		a["points"] = nlohmann::json::array();
		for (size_t i = 0; i < gRiverPoints.size(); i++) a["points"].push_back({ gRiverPoints[i].x, gRiverPoints[i].y });
		if (!AgentTerrain("terrain_river", a, out, errOut)) return false;
		gRiverPoints.clear(); gRiverPick = false;
		return true;
	}
	if (command == "terrain_river")
	{
		if (playMode || sceneIsTwoD) { errOut = "a river needs a 3D scene outside play"; return false; }
		if (!args.contains("points") || !args["points"].is_array() || args["points"].size() < 2) { errOut = "'points' ([[x,z],...], two or more; or laid out in the view) is required"; return false; }
		const f32 width = args.value("width", 10.f), depth = args.value("depth", 2.4f), bank = args.value("bank", 9.f);
		const bool toLevel = args.contains("level");
		const f32 level = args.value("level", 0.f);
		const TerrainTools::Tool keptTool = t.tool;
		const f32 keptRadius = t.radius, keptStrength = t.strength, keptHardness = t.hardness;
		const bool wasActive = t.active;
		const nlohmann::json &pts = args["points"];
		const f32 x0 = pts[0].at(0).get<f32>(), z0 = pts[0].at(1).get<f32>();
		f32 h0; Vec3 n0;
		if (!TerrainTools::GroundPoint(scene, x0, z0, h0, n0)) { errOut = "the river's source is off the terrain"; return false; }
		t.tool = TerrainTools::Flatten; t.radius = width * 0.5f + bank; t.strength = 1.f;
		t.hardness = std::min(0.95f, (width * 0.5f) / std::max(0.5f, t.radius));
		t.active = true;
		t.Update(scene, true, Vec3(x0, h0 + 1000.f, z0), Vec3(0.f, -1.f, 0.f), 0.f);
		if (!BeginTerrainStroke()) { t.tool = keptTool; t.radius = keptRadius; t.strength = keptStrength; t.hardness = keptHardness; t.active = wasActive; errOut = "could not start a stroke there"; return false; }
		uint32 dabs = 0;
		f32 bed = h0 - depth;                 // a river only runs down: its bed never rises along its way
		for (size_t i = 0; i + 1 < pts.size(); i++)
		{
			const Vec2 a(pts[i].at(0).get<f32>(), pts[i].at(1).get<f32>()), b(pts[i + 1].at(0).get<f32>(), pts[i + 1].at(1).get<f32>());
			const f32 len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
			const int steps = std::max(1, (int)(len / std::max(1.f, width * 0.35f)));
			for (int s = 0; s <= steps; s++)
			{
				const f32 k = (f32)s / (f32)steps;
				const f32 px = a.x + (b.x - a.x) * k, pz = a.y + (b.y - a.y) * k;
				f32 h; Vec3 n;
				if (!TerrainTools::GroundPoint(scene, px, pz, h, n)) continue;
				bed = toLevel ? level : std::min(bed, h - depth);
				t.SetLevel(bed);
				for (int pass = 0; pass < 3; pass++) if (t.ApplyAt(scene, px, pz, 1.f)) dabs++;
			}
		}
		EndTerrainStroke();
		// Its banks: the ground's layers taken away in a band along it ("stones" metres past the water's
		// edge; 0 for none) - what a terrain shader lays there is its own business - and nothing growing on them.
		const f32 stones = args.value("stones", 0.f);
		const int keptLayer = t.layer; const f32 keptDensity = t.density;
		if (stones > 0.f)
		{
			t.radius = width * 0.5f + stones; t.strength = 1.f; t.hardness = 0.86f;
			for (int what = 0; what < 7; what++)
			{
				t.tool = what == 0 ? TerrainTools::PaintTexture : TerrainTools::PaintFoliage;
				t.layer = what == 0 ? 4 : what - 1; t.density = 0.f;
				if (!BeginTerrainStroke()) continue;
				for (size_t i = 0; i + 1 < pts.size(); i++)
				{
					const Vec2 a(pts[i].at(0).get<f32>(), pts[i].at(1).get<f32>()), b(pts[i + 1].at(0).get<f32>(), pts[i + 1].at(1).get<f32>());
					const f32 len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
					const int steps = std::max(1, (int)(len / std::max(1.f, t.radius * 0.4f)));
					for (int s = 0; s <= steps; s++)
					{
						const f32 k = (f32)s / (f32)steps;
						for (int pass = 0; pass < 4; pass++) t.ApplyAt(scene, a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, 1.f);
					}
				}
				EndTerrainStroke();
			}
		}
		t.layer = keptLayer; t.density = keptDensity;
		// What stood where the water now runs ("clear": true): every top-level object within the channel
		// that holds nothing but something drawn - a tree, a rock - is deleted (each its own undo step).
		// Objects with a terrain, a light, a script or anything else under them are left alone.
		uint32 cleared = 0;
		if (args.value("clear", false))
		{
			std::vector<uint32> gone;
			const std::map<uint32, SceneObject*> &all = sceneObjects->GetList();
			for (std::map<uint32, SceneObject*>::const_iterator i = all.begin(); i != all.end(); ++i)
			{
				SceneObject* o = i->second;
				if (!o || o->GetType() != SceneObjectTypes::GAMEOBJECT || o->GetParentID() != 0 || !o->GetPTR()) continue;
				bool drawn = false, other = false;
				for (std::map<uint32, SceneObject*>::const_iterator c = all.begin(); c != all.end(); ++c)
					if (c->second && c->second->GetParentID() == o->GetID())
					{
						if (c->second->GetType() == SceneObjectTypes::RENDERING_COMPONENT || c->second->GetType() == SceneObjectTypes::PHYSICS_COMPONENT) drawn = true;
						else if (c->second->GetType() != SceneObjectTypes::GAMEOBJECT) other = true;
					}
				if (!drawn || other) continue;
				const Vec3 p = ((GameObject*)o->GetPTR())->GetWorldPosition();
				for (size_t k = 0; k + 1 < pts.size(); k++)
				{
					const Vec2 a(pts[k].at(0).get<f32>(), pts[k].at(1).get<f32>()), b(pts[k + 1].at(0).get<f32>(), pts[k + 1].at(1).get<f32>());
					const f32 abx = b.x - a.x, abz = b.y - a.y, l2 = abx * abx + abz * abz;
					const f32 tt = l2 > 0.f ? std::min(1.f, std::max(0.f, ((p.x - a.x) * abx + (p.z - a.y) * abz) / l2)) : 0.f;
					const f32 dx = p.x - (a.x + abx * tt), dz = p.z - (a.y + abz * tt);
					if (dx * dx + dz * dz < (width * 0.5f + 1.5f) * (width * 0.5f + 1.5f)) { gone.push_back(o->GetID()); break; }
				}
			}
			for (size_t g = 0; g < gone.size(); g++) { std::string derr; if (OpDeleteGameObject(gone[g], derr)) cleared++; }
		}
		t.tool = keptTool; t.radius = keptRadius; t.strength = keptStrength; t.hardness = keptHardness; t.active = wasActive;
		out = t.State(scene);
		out["dabs"] = dabs;
		out["cleared"] = cleared;
		// (the water's picture of the depth, made again: or the new river is drawn as shore)
		if (args.contains("depthTexture"))
		{
			nlohmann::json bake = args; bake["texture"] = args["depthTexture"];
			// (the water's own level, not the bed's: "waterLevel"; without it, the level the picture was baked at)
			bake.erase("level"); bake.erase("depth");
			if (args.contains("waterLevel")) bake["level"] = args["waterLevel"];
			nlohmann::json baked; std::string berr;
			if (AgentTerrain("terrain_bake_depth", bake, baked, berr)) out["depthBaked"] = baked.value("textures", 0u);
			else out["depthError"] = berr;
		}
		return true;
	}
	// terrain_add_water {level, size, texture, name}: a sea (or a lake) - a plane at `level` drawn with the
	// Water template (written into the project as assets/materials/Water.glsl the first time), its depth
	// picture (`texture`, default assets/textures/water_depth.png) baked from the ground now, ripples made
	// for it, and the Sky effect's sea level set to the same height. `size`: metres the depth picture
	// covers, centred on the origin (default: as far as the terrains reach). One undo step for the object.
	if (command == "terrain_add_water")
	{
		if (playMode || sceneIsTwoD) { errOut = "water needs a 3D scene outside play"; return false; }
		if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }
		// (what it loads is named from the project, which a scene only knows once it has a file)
		if (scenePath.empty()) { errOut = "save the scene first"; return false; }
		namespace fs = std::filesystem;
		const fs::path root(project->GetProjectPath());
		std::error_code ec;
		f32 size = args.value("size", 0.f);
		if (size <= 0.f)
		{
			const std::vector<TerrainComponent*> terrains = SceneTerrains();
			for (size_t i = 0; i < terrains.size(); i++)
			{
				const TerrainComponent::Settings &ts = terrains[i]->GetSettings();
				const Vec3 at = terrains[i]->GetOwner() ? terrains[i]->GetOwner()->GetWorldPosition() : Vec3();
				size = std::max(size, std::max(ts.tilesX, ts.tilesZ) * ts.tileSize + 2.f * std::max(std::fabs(at.x), std::fabs(at.z)));
			}
			if (size <= 0.f) size = 1024.f;
		}
		size_t sky = postEffects.size();
		for (size_t i = 0; i < postEffects.size(); i++) if (postEffects[i].effect == "Sky") { sky = i; break; }
		f32 level = 0.f;
		if (sky < postEffects.size() && postEffects[sky].params.count("uSea") && !postEffects[sky].params["uSea"].empty()) level = postEffects[sky].params["uSea"][0];
		level = args.value("level", level);
		const std::string depthRel = args.value("texture", std::string("assets/textures/water_depth.png"));
		const std::string wavesRel = "assets/textures/water_normals.png", shaderRel = "assets/materials/Water.glsl";
		fs::create_directories((root / depthRel).parent_path(), ec);
		fs::create_directories((root / shaderRel).parent_path(), ec);
		// (the shader: the editor's template, laid out for this picture)
		if (!fs::exists(root / shaderRel, ec))
		{
			std::ifstream in("assets/materials/Water.glsl", std::ios::binary);
			if (!in) { errOut = "the editor's Water template (assets/materials/Water.glsl) is missing"; return false; }
			std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			const std::string key = "#define WATER_MAP_SIZE 1024.0";
			const size_t at = text.find(key);
			if (at != std::string::npos) { char v[64]; snprintf(v, sizeof(v), "#define WATER_MAP_SIZE %.1f", size); text.replace(at, key.size(), v); }
			std::ofstream out((root / shaderRel).string().c_str(), std::ios::binary | std::ios::trunc);
			out << text;
			if (!out) { errOut = "could not write " + shaderRel; return false; }
		}
		else echo("Add water: " + shaderRel + " is the project's already - kept as it is (its WATER_MAP_SIZE must match the bake's size)");
		if (!fs::exists(root / wavesRel, ec) && !WriteWaterNormals((root / wavesRel).string(), 7u)) { errOut = "could not write " + wavesRel; return false; }
		// (a picture to bake into: the material must hold it before the bake can find it)
		if (!fs::exists(root / depthRel, ec))
		{
			const uchar deep[16] = { 255, 0, 128, 128, 255, 0, 128, 128, 255, 0, 128, 128, 255, 0, 128, 128 };
			if (!PaintableImage::WritePNG((root / depthRel).string(), 2, 2, 4, deep)) { errOut = "could not write " + depthRel; return false; }
		}
		json m = { { "id", 0 }, { "kind", "custom" }, { "shaderFile", shaderRel }, { "castingShadows", false },
			{ "transparent", false }, { "cullFace", 0 },
			{ "samplers", json::array({ { { "name", "shore" }, { "texture", depthRel } }, { { "name", "waves" }, { "texture", wavesRel } } }) } };
		// (a fine grid: the shader moves it with the waves and lays it round the eye, so its own size hardly matters)
		json rc = { { "type", "RenderingComponent" }, { "castingShadows", false }, { "cullTest", false }, { "material", 0 },
			{ "renderable", { { "kind", "primitive" }, { "shape", "Plane" }, { "width", 58.0 }, { "height", 58.0 }, { "segments", 384 },
				{ "smooth", false }, { "flip", false }, { "tangentBitangent", true } } } };
		json rootObj = { { "name", args.value("name", std::string("Water")) }, { "position", { 0.0, level, 0.0 } },
			{ "rotation", { -1.5707963705062866, 0.0, 0.0 } }, { "scale", { 120.0, 120.0, 120.0 } }, { "static", false },
			{ "tags", json::array({ "Water" }) }, { "components", json::array({ rc }) }, { "children", json::array() } };
		json tree = { { "root", rootObj }, { "materials", json::array({ m }) } };
		SceneObject* obj = RawInsertSubtree(tree.dump(), 0, false, EditorCameraSettings(), true);
		if (!obj) { errOut = "the water's files were written, but it could not be built"; return false; }
		PushAddCommand(obj);
		SelectSceneObject(obj);
		MarkSceneDirty();
		// (the depth picture, from the ground as it is: square-rooted, with half a metre above the water, and the rivers' flow)
		json bake = { { "texture", depthRel }, { "level", level }, { "range", 12.0 }, { "above", 0.5 }, { "size", size },
			{ "resolution", (uint32)std::min(2048.f, std::max(256.f, size)) }, { "encoding", "sqrt" }, { "flow", true } };
		json baked; std::string berr;
		if (!AgentTerrain("terrain_bake_depth", bake, baked, berr)) echo("ERROR: Add water - baking the depth picture: " + berr);
		// (the world's sea level: where a Sky's haze starts and a natural ground lays its beach)
		if (sky < postEffects.size())
		{
			std::vector<SceneMeta::PostEffectEntry> next = postEffects;
			next[sky].params["uSea"] = std::vector<f32>(1, level);
			SetPostEffectChain(next);
		}
		out = { { "name", obj->GetName() }, { "level", level }, { "size", size }, { "texture", depthRel }, { "shader", shaderRel },
			{ "skySeaLevel", sky < postEffects.size() } };
		return true;
	}
	if (command == "terrain_populate")
	{
		if (playMode || sceneIsTwoD) { errOut = "populate needs a 3D scene outside play"; return false; }
		if (!args.contains("rules") || !args["rules"].is_array() || args["rules"].empty()) { errOut = "'rules' ([{asset, ...}]) is required"; return false; }
		uint32 planted = 0;
		if (!Populate(args.value("x", 0.f), args.value("z", 0.f), args.value("radius", 50.f), args.value("count", 100u), args.value("spacing", 3.f),
			args.value("seed", 1u), args.value("sea", -1e9f), args["rules"], planted, errOut)) return false;
		out = t.State(scene);
		out["planted"] = planted;
		return true;
	}
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
		// ("ground": "plain" | "natural" - the surface's shader; see TerrainTools::CreateParams)
		p.ground = args.value("ground", p.ground);
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
			ImGui::DragFloat("Thin out from", &s.thinFrom, 1.f, 0.f, 5000.f, "%.0f m"); track();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Far off, fewer plants are drawn and each one left is drawn bigger,\nso the ground stays covered. What a field costs is how many plants it is.");
			ImGui::DragFloat("Thinned by", &s.thinTo, 1.f, 0.f, 5000.f, "%.0f m"); track();
			ImGui::SliderFloat("Plants kept", &s.thinDensity, 0.1f, 1.f, "%.2f"); track();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("1: no thinning.");
			s.thinTo = std::max(s.thinTo, s.thinFrom);
			bool shadows = s.castShadows;
			if (ImGui::Checkbox("Cast shadows", &shadows)) { s.castShadows = shadows; immediate(true); }
			s.fadeDistance = std::max(s.fadeDistance, s.fullDistance);
			ImGui::TextDisabled("Density map: %s", s.densityMap.empty() ? "(none - paint with the Foliage brush)" : s.densityMap.c_str());
			ImGui::TextDisabled("Patches");
			ImGui::TextDisabled("Map: %s", s.patchMap.empty() ? "(none - set \"patchMap\" with set_foliage_layer)" : s.patchMap.c_str());
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("An image repeated over the ground: its red says where plants take the\nhigh tint and where the low, its green where they are thinned.\nA ground shader reading the same image has its bare earth under the gaps.");
			ImGui::DragFloat("Repeats every", &s.patchSize, 1.f, 0.f, 5000.f, "%.0f m"); track();
			ImGui::SliderFloat("Thin from", &s.patchFrom, 0.f, 1.f, "%.2f"); track();
			ImGui::SliderFloat("Thinned by##patch", &s.patchTo, 0.f, 1.f, "%.2f"); track();
			ImGui::SliderFloat("Kept in a patch", &s.patchKeep, 0.f, 1.f, "%.2f"); track();
			ImGui::SliderFloat("Tint by patch", &s.patchTint, 0.f, 1.f, "%.2f"); track();
			s.patchTo = std::max(s.patchTo, s.patchFrom);
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
		const bool ik = cmd == "add_ik" || cmd == "set_ik" || cmd == "remove_ik";
		const bool animator = cmd == "add_animator" || cmd == "set_animator" || cmd == "remove_animator";
		const bool cloth = cmd == "add_cloth" || cmd == "set_cloth" || cmd == "remove_cloth";
		const bool nav = cmd == "bake_navmesh" || cmd == "clear_navmesh";
		const bool navAgent = cmd == "add_nav_agent" || cmd == "set_nav_agent" || cmd == "remove_nav_agent";
		const bool ok = navAgent ? AgentNavAgent(cmd, op, out, err) : nav ? AgentNavigation(cmd, op, out, err) : cloth ? AgentCloth(cmd, op, out, err) : animator ? AgentAnimator(cmd, op, out, err) : ik ? AgentIK(cmd, op, out, err)
			: cmd == "move_to_cell" ? AgentMoveToCell(op, out, err)
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
