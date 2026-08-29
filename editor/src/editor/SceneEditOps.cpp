// Unified UI/Agent edit chokepoint - see the undo/redo plan. Every Op*
// method here is the single place a given kind of scene edit happens,
// called both by interactive UI code (ShowProperties, DrawTreeNodeWidgets,
// AddFormSubmit, ...) and by the Agent* methods in SceneEditor.cpp, so the
// two front ends can no longer apply different fixups for the same edit.

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include "PrefabResolver.h"
#include <filesystem>
#include <fstream>
#include <sstream>

void SceneEditor::SelectAndFocusSceneObject(SceneObject* obj)
{
	if (obj)
	{
		SelectSceneObject(obj);
		node_clicked = (int32)obj->GetID();
	}
	else
	{
		DeselectMesh();
		DeselectSceneObject();
		selection.clear();
		node_clicked = -1;
	}
}

void SceneEditor::RawDeleteSubtree(uint32 objId)
{
	// Body identical to the old DeleteGameObjectById() - kept as one
	// implementation so both the interactive chokepoint (OpDeleteGameObject)
	// and undo/redo commands (AddGameObjectCommand::Undo,
	// DeleteGameObjectCommand::Redo) share it exactly.
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	GameObject* go = (GameObject*)obj->GetPTR();

	const bool wasCamera = IsSceneCamera(objId);
	if (wasCamera)
		editorDebugDraw->ForgetCamera(go);
	for (std::map<uint32, SceneObject*>::const_iterator j = sceneObjects->GetList().begin(); j != sceneObjects->GetList().end(); j++)
	{
		if (j->second == NULL || j->second->GetType() == SceneObjectTypes::GAMEOBJECT) continue;
		uint32 pid = j->second->GetParentID();
		while (pid != 0)
		{
			if (pid == objId)
			{
				editorDebugDraw->ForgetComponent((IComponent*)j->second->GetPTR());
				break;
			}
			SceneObject* parent = sceneObjects->GetSceneObject(pid);
			if (parent == NULL || parent->GetType() != SceneObjectTypes::GAMEOBJECT) break;
			pid = parent->GetParentID();
		}
	}
	if (SelectedSceneObject == obj)
	{
		DeselectMesh();
		DeselectSceneObject();
		selection.clear();
	}
	if (scriptRenderCamera == go)
		scriptRenderCamera = nullptr;
	sceneObjects->DestroySceneObject(objId);
	if (wasCamera)
	{
		if (activeSceneCameraId == objId)
			activeSceneCameraId = 0;
		UnregisterSceneCamera(objId);
	}
	node_clicked = -1;
	MarkSceneDirty();
}

SceneObject* SceneEditor::RawInsertSubtree(const std::string& subtreeJson, uint32 parentId, bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper)
{
	if (subtreeJson.empty()) return NULL;
#ifdef LUA_BINDINGS
	std::shared_ptr<GameObject> go = SceneSerializer::DeserializeSubtree(subtreeJson, scenePath, physics, sharedLua, NULL);
#else
	std::shared_ptr<GameObject> go = SceneSerializer::DeserializeSubtree(subtreeJson, scenePath, physics, NULL, NULL);
#endif
	if (!go) return NULL;
	scene->Add(go);
	SceneObject* obj = sceneObjects->Adopt(go.get(), parentId);
	if (obj)
	{
		// A subtree that came from a prefab instance carries which one in
		// its root (SnapshotSubtree writes it; the engine neither writes nor
		// reads it). Recovering it here is what keeps an instance an
		// instance across undo, redo, delete-and-restore and duplicate,
		// without any of those needing to know prefabs exist.
		try
		{
			const nlohmann::json parsed = nlohmann::json::parse(subtreeJson);
			if (parsed.is_object() && parsed.find("root") != parsed.end())
				obj->prefabSource = prefab::LinkOf(parsed["root"]);
		}
		catch (const std::exception&) { /* not our concern - the load above already succeeded */ }

		// Give icon helpers to the just-reinserted subtree only (self +
		// descendants) - the same per-object rule RebuildHelpers() applies
		// after a full scene load, but scoped here so it doesn't
		// retroactively helper-ify unrelated objects elsewhere in the scene
		// that happen to lack one (e.g. anything created via the Agent API,
		// which never creates a helper itself). An unscoped RebuildHelpers()
		// call here would silently attach real scene-graph roots to those
		// objects too, which then leak into anything that walks
		// scene->GetAllGameObjectList() (AgentSceneState/get_scene_objects)
		// until the next save/load cycle's DetachEditorObjects pass.
		// `hadHelper` further gates the root itself so an object that never
		// had an icon (e.g. Agent-created) doesn't gain one purely from
		// being deleted and undone - descendants still follow the
		// unconditional per-type rule, matching RebuildHelpers().
		const uint32 rootId = obj->GetID();
		for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin(); i != sceneObjects->GetList().end(); i++)
		{
			SceneObject* o = i->second;
			if (!o || o->Helper) continue;
			if (o->GetID() == rootId) { if (!hadHelper) continue; }
			else if (!sceneObjects->IsDescendant(rootId, o->GetID())) continue;

			if (o->GetType() == SceneObjectTypes::GAMEOBJECT)
			{
				std::shared_ptr<GameObjectHelper> h = std::make_shared<GameObjectHelper>((GameObject*)o->GetPTR());
				o->Helper = h;
				scene->Add(h);
			}
			else if (o->GetType() == SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT ||
					 o->GetType() == SceneObjectTypes::POINTLIGHT_COMPONENT ||
					 o->GetType() == SceneObjectTypes::SPOTLIGHT_COMPONENT)
			{
				IComponent* c = (IComponent*)o->GetPTR();
				if (c && c->GetOwner())
				{
					std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(c->GetOwner());
					o->Helper = h;
					scene->Add(h);
				}
			}
			else if (o->GetType() == SceneObjectTypes::AUDIO_SOURCE_COMPONENT)
			{
				IComponent* c = (IComponent*)o->GetPTR();
				if (c && c->GetOwner())
				{
					std::shared_ptr<SoundHelper> h = std::make_shared<SoundHelper>(c->GetOwner());
					o->Helper = h;
					scene->Add(h);
				}
			}
			else if (o->GetType() == SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT)
			{
				IComponent* c = (IComponent*)o->GetPTR();
				if (c && c->GetOwner())
				{
					std::shared_ptr<ParticleHelper> h = std::make_shared<ParticleHelper>(c->GetOwner());
					o->Helper = h;
					scene->Add(h);
				}
			}
		}
	}
	if (wasCamera && obj)
		RegisterSceneCamera(obj->GetID(), camSettings);
	MarkSceneDirty();
	return obj;
}

void SceneEditor::ApplyTransform(uint32 objId, const Vec3& pos, const Vec3& rot, const Vec3& scale)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	GameObject* go = (GameObject*)obj->GetPTR();
	go->SetPosition(pos);
	go->SetRotation(rot);
	go->SetScale(scale);
	// Resync the Properties-panel scratch fields (and LocalTransform/
	// ScaleTransform) when this is the selected object, otherwise
	// Update()'s per-frame push (guarded by !gizmoDragging) stomps this
	// edit back to the stale _translation/_rotation/_scale next frame.
	if (obj == SelectedSceneObject)
		SyncTransformFromGameObject(obj);
	MarkSceneDirty();
}

void SceneEditor::PushAddCommand(SceneObject* created)
{
	if (!created) return;
	GameObject* go = (GameObject*)created->GetPTR();
	if (!go) return;
	const bool wasCamera = IsSceneCamera(created->GetID());
	EditorCameraSettings camSettings = wasCamera ? sceneCameras[created->GetID()] : EditorCameraSettings();
	const bool hadHelper = (created->Helper != nullptr);
	std::string snapshot = SnapshotSubtree(created->GetID());
	sceneUndo.Push(std::make_unique<AddGameObjectCommand>(this, created->GetParentID(), snapshot,
		wasCamera, camSettings, hadHelper, created->GetName(), created->GetID()));
}

void SceneEditor::PushReplaceCommand(uint32 ownerId, const std::string& beforeSnapshot, const std::string& description)
{
	SceneObject* owner = sceneObjects->GetSceneObject(ownerId);
	if (!owner) return;
	GameObject* go = (GameObject*)owner->GetPTR();
	if (!go) return;
	const bool wasCamera = IsSceneCamera(ownerId);
	EditorCameraSettings camSettings = wasCamera ? sceneCameras[ownerId] : EditorCameraSettings();
	const bool hadHelper = (owner->Helper != nullptr);
	std::string afterSnapshot = SnapshotSubtree(ownerId);
	sceneUndo.Push(std::make_unique<ReplaceGameObjectCommand>(this, owner->GetParentID(),
		beforeSnapshot, afterSnapshot, wasCamera, camSettings, hadHelper, ownerId, description));
}

bool SceneEditor::OpDeleteGameObject(uint32 objId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (IsInternalGameObject(go)) { errOut = "cannot delete an internal object"; return false; }

	const uint32 parentId = obj->GetParentID();
	const bool wasCamera = IsSceneCamera(objId);
	EditorCameraSettings camSettings = wasCamera ? sceneCameras[objId] : EditorCameraSettings();
	const bool hadHelper = (obj->Helper != nullptr);
	const std::string name = obj->GetName();
	std::string snapshot = SnapshotSubtree(objId);

	RawDeleteSubtree(objId);

	sceneUndo.Push(std::make_unique<DeleteGameObjectCommand>(this, parentId, snapshot, wasCamera, camSettings, hadHelper, name));
	return true;
}

uint32 SceneEditor::OpDuplicateGameObject(uint32 objId, std::string& errOut)
{
	SceneObject* src = sceneObjects->GetSceneObject(objId);
	if (!src || src->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return 0; }
	GameObject* go = (GameObject*)src->GetPTR();
	if (IsInternalGameObject(go)) { errOut = "cannot duplicate an internal object"; return 0; }

	const bool wasCamera = IsSceneCamera(objId);
	EditorCameraSettings camSettings = wasCamera ? sceneCameras[objId] : EditorCameraSettings();

	SceneObject* dup = sceneObjects->DuplicateGameObject(objId, physics);
	if (!dup) { errOut = "duplicate failed"; return 0; }

	if (wasCamera)
		RegisterSceneCamera(dup->GetID(), camSettings);

	SelectAndFocusSceneObject(dup);
	MarkSceneDirty();
	PushAddCommand(dup);
	return dup->GetID();
}

bool SceneEditor::OpReparentGameObject(uint32 childId, uint32 newParentId, std::string& errOut)
{
	SceneObject* child = sceneObjects->GetSceneObject(childId);
	if (!child || child->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	const uint32 oldParentId = child->GetParentID();
	if (oldParentId == newParentId) return true; // no-op, nothing to push

	const std::string name = child->GetName();
	if (!sceneObjects->ReparentGameObject(childId, newParentId)) { errOut = "reparent failed (cycle or invalid)"; return false; }
	MarkSceneDirty();
	sceneUndo.Push(std::make_unique<ReparentGameObjectCommand>(this, childId, oldParentId, newParentId, name));
	return true;
}

bool SceneEditor::OpRenameGameObject(uint32 objId, const std::string& newName, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	const std::string oldName = obj->GetName();
	sceneObjects->SetName(objId, newName);
	const std::string finalName = obj->GetName(); // SetName may have deduped
	MarkSceneDirty();
	if (finalName != oldName)
		sceneUndo.Push(std::make_unique<RenameGameObjectCommand>(this, objId, oldName, finalName));
	return true;
}

bool SceneEditor::OpSetTransform(uint32 objId, const Vec3& pos, const Vec3& rot, const Vec3& scale, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	const Vec3 oldPos = go->GetPosition();
	const Vec3 oldRot = go->GetRotation();
	const Vec3 oldScale = go->GetScale();
	const std::string name = obj->GetName();

	ApplyTransform(objId, pos, rot, scale);
	sceneUndo.Push(std::make_unique<SetTransformCommand>(this, objId, oldPos, oldRot, oldScale, pos, rot, scale, name));
	return true;
}

void SceneEditor::RawAssignMaterial(uint32 goId, int submeshIndex, std::shared_ptr<p3d::IMaterial> mat)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	GameObject* go = (GameObject*)obj->GetPTR();
	RenderingComponent* rc = NULL;
	for (auto& c : go->GetComponents())
		if ((rc = dynamic_cast<RenderingComponent*>(c.get()))) break;
	if (!rc) return;
	std::vector<RenderingMesh*>& meshes = rc->GetMeshes(0);
	if (submeshIndex < 0 || (size_t)submeshIndex >= meshes.size()) return;
	meshes[submeshIndex]->Material = mat;
	MarkSceneDirty();
}

bool SceneEditor::OpAssignMaterial(uint32 goId, int submeshIndex, std::shared_ptr<p3d::IMaterial> mat, std::string& errOut)
{
	if (!mat) { errOut = "no material to assign"; return false; }
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	RenderingComponent* rc = NULL;
	for (auto& c : go->GetComponents())
		if ((rc = dynamic_cast<RenderingComponent*>(c.get()))) break;
	if (!rc) { errOut = "object has no RenderingComponent"; return false; }
	std::vector<RenderingMesh*>& meshes = rc->GetMeshes(0);
	if (submeshIndex < 0 || (size_t)submeshIndex >= meshes.size())
		{ errOut = "submesh index " + std::to_string(submeshIndex) + " out of range (object has " + std::to_string(meshes.size()) + ")"; return false; }

	std::shared_ptr<p3d::IMaterial> oldMat = meshes[submeshIndex]->Material;
	RawAssignMaterial(goId, submeshIndex, mat);
	sceneUndo.Push(std::make_unique<AssignMaterialCommand>(this, goId, submeshIndex, oldMat, mat, obj->GetName()));
	return true;
}

void SceneEditor::ApplyCameraFov(uint32 goId, f32 fov)
{
	std::map<uint32, EditorCameraSettings>::iterator it = sceneCameras.find(goId);
	if (it == sceneCameras.end()) return;
	it->second.fov = fov;
}

void SceneEditor::ApplyCameraOrthographic(uint32 goId, bool orthographic)
{
	std::map<uint32, EditorCameraSettings>::iterator it = sceneCameras.find(goId);
	if (it == sceneCameras.end()) return;
	it->second.orthographic = orthographic;
	MarkSceneDirty();
}

void SceneEditor::ApplyCameraOrthoSize(uint32 goId, f32 orthoSize)
{
	std::map<uint32, EditorCameraSettings>::iterator it = sceneCameras.find(goId);
	if (it == sceneCameras.end()) return;
	it->second.orthoSize = orthoSize > 0.001f ? orthoSize : 0.001f;
	MarkSceneDirty();
}

void SceneEditor::ApplyCameraNear(uint32 goId, f32 nearPlane)
{
	std::map<uint32, EditorCameraSettings>::iterator it = sceneCameras.find(goId);
	if (it == sceneCameras.end()) return;
	it->second.nearPlane = nearPlane;
}

void SceneEditor::ApplyCameraFar(uint32 goId, f32 farPlane)
{
	std::map<uint32, EditorCameraSettings>::iterator it = sceneCameras.find(goId);
	if (it == sceneCameras.end()) return;
	it->second.farPlane = farPlane;
}

void SceneEditor::ApplyLightColor(uint32 lightId, const Vec4& color)
{
	SceneObject* obj = sceneObjects->GetSceneObject(lightId);
	if (!obj) return;
	ILightComponent* l = dynamic_cast<ILightComponent*>((IComponent*)obj->GetPTR());
	if (!l) return;
	l->SetLightColor(color);
	if (SelectedSceneObject == obj) PropertiesLightColor = color;
	MarkSceneDirty();
}

void SceneEditor::ApplyLightDirection(uint32 lightId, const Vec3& direction)
{
	SceneObject* obj = sceneObjects->GetSceneObject(lightId);
	if (!obj) return;
	IComponent* c = (IComponent*)obj->GetPTR();
	if (DirectionalLight* l = dynamic_cast<DirectionalLight*>(c)) l->SetLightDirection(direction);
	else if (SpotLight* l = dynamic_cast<SpotLight*>(c)) l->SetLightDirection(direction);
	else return;
	if (SelectedSceneObject == obj) PropertiesLightDirection = direction;
	MarkSceneDirty();
}

void SceneEditor::ApplyLightRadius(uint32 lightId, f32 radius)
{
	SceneObject* obj = sceneObjects->GetSceneObject(lightId);
	if (!obj) return;
	IComponent* c = (IComponent*)obj->GetPTR();
	if (PointLight* l = dynamic_cast<PointLight*>(c)) l->SetLightRadius(radius);
	else if (SpotLight* l = dynamic_cast<SpotLight*>(c)) l->SetLightRadius(radius);
	else return;
	if (SelectedSceneObject == obj) PropertiesLightRadius = radius;
	MarkSceneDirty();
}

void SceneEditor::ApplyLightInnerCone(uint32 lightId, f32 innerCone)
{
	SceneObject* obj = sceneObjects->GetSceneObject(lightId);
	if (!obj) return;
	SpotLight* l = dynamic_cast<SpotLight*>((IComponent*)obj->GetPTR());
	if (!l) return;
	l->SetLightInnerCone(innerCone);
	if (SelectedSceneObject == obj) PropertiesLightInnerCone = innerCone;
	MarkSceneDirty();
}

void SceneEditor::ApplyLightOuterCone(uint32 lightId, f32 outerCone)
{
	SceneObject* obj = sceneObjects->GetSceneObject(lightId);
	if (!obj) return;
	SpotLight* l = dynamic_cast<SpotLight*>((IComponent*)obj->GetPTR());
	if (!l) return;
	l->SetLightOutterCone(outerCone);
	if (SelectedSceneObject == obj) PropertiesLightOutterCone = outerCone;
	MarkSceneDirty();
}

void SceneEditor::ApplyParticleDesc(uint32 psId, const ParticleSystemDesc& desc)
{
	SceneObject* obj = sceneObjects->GetSceneObject(psId);
	if (!obj) return;
	ParticleSystem* ps = dynamic_cast<ParticleSystem*>((IComponent*)obj->GetPTR());
	if (!ps) return;

	// SetMaxParticles() first, and only when it actually differs - it is the
	// one setter that throws away every live particle, so re-applying an
	// unchanged capacity on any other field's undo would visibly wipe the
	// effect (SetMaxParticles' own early-out is what makes this safe).
	ps->SetMaxParticles(desc.maxParticles);
	ps->SetTexture(desc.texture);
	ps->SetLooping(desc.looping);
	ps->SetEmissionRate(desc.emissionRate);
	ps->SetBurstCount(desc.burstCount);
	ps->SetLifetime(desc.minLifetime, desc.maxLifetime);
	ps->SetDirection(desc.direction);
	ps->SetSpread(desc.spreadAngle);
	ps->SetSpeed(desc.minSpeed, desc.maxSpeed);
	ps->SetGravity(desc.gravity);
	ps->SetDamping(desc.damping);
	ps->SetSizes(desc.startSize, desc.endSize, desc.sizeRandomJitter);
	ps->SetColors(desc.startColor, desc.endColor);
	ps->SetFade(desc.fadeInFraction, desc.fadeOutFraction);
	ps->SetRotationSpeed(desc.minRotationSpeed, desc.maxRotationSpeed);
	ps->SetBlendMode(desc.blendMode);

	// The panel's own draft fields are the only state that doesn't re-read
	// the desc every frame - see propertiesParticle* in SceneEditor.h.
	if (SelectedSceneObject == obj)
	{
		// Project-relative, same reason as the other seeding site in
		// SceneEditor.cpp: the Texture remembers the absolute path it was
		// loaded from, which is not what belongs in the inspector.
		propertiesParticleTexturePath = desc.texture
			? (project ? project->DisplayPath(desc.texture->GetFilename())
				: desc.texture->GetFilename())
			: std::string();
		propertiesParticleMax = (int32)desc.maxParticles;
	}
	MarkSceneDirty();
}

// Both descs are taken BY VALUE on purpose: callers routinely pass the
// component's own live desc as `before` (via ParticleSystem::GetDesc()'s
// reference), and the ApplyParticleDesc() call below overwrites exactly
// that - by-reference parameters would leave `before` already mutated by
// the time the undo closure captured it.
void SceneEditor::PushParticleDescCommand(uint32 psId, const ParticleSystemDesc before,
	const ParticleSystemDesc after, const std::string& label)
{
	ApplyParticleDesc(psId, after);
	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[this, psId, before]() { ApplyParticleDesc(psId, before); },
		[this, psId, after]() { ApplyParticleDesc(psId, after); }, label));
}

// ============================ Prefabs ==================================
//
// The reference/expand/collapse machinery is shared/PrefabResolver.h, which
// knows nothing about the editor, and the engine knows nothing about any of
// it. What lives here is the editor-side bookkeeping: which live object is
// an instance of what (SceneObject::prefabSource), keeping that link alive
// across undo, and the four operations the context menu offers.

std::string SceneEditor::PrefabPathOf(uint32 objId) const
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	return obj ? obj->prefabSource : std::string();
}

// SerializeSubtree() with the instance link written into the root.
//
// Every undo command stores a subtree as text and rebuilds from it, and the
// engine neither writes nor reads this key - so without it an instance would
// silently stop being one after a single Ctrl+Z. RawInsertSubtree() reads it
// back on the way in, which is what makes the link survive undo, redo,
// delete-and-restore and duplicate without any of those knowing it exists.
std::string SceneEditor::SnapshotSubtree(uint32 objId)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return std::string();
	GameObject* go = (GameObject*)obj->GetPTR();
	if (!go) return std::string();

#ifdef LUA_BINDINGS
	const std::string text = SceneSerializer::SerializeSubtree(go, scenePath, sharedLua);
#else
	const std::string text = SceneSerializer::SerializeSubtree(go, scenePath, NULL);
#endif
	if (obj->prefabSource.empty() || text.empty()) return text;

	try
	{
		nlohmann::json j = nlohmann::json::parse(text);
		j["root"]["prefab"] = obj->prefabSource;
		return j.dump();
	}
	catch (const std::exception&) { return text; }
}

nlohmann::json SceneEditor::LoadPrefabJson(const std::string& relPath) const
{
	if (!project || !project->IsOpen()) return nlohmann::json();
	return prefab::ReadPrefabFile(project->AbsolutePath(relPath));
}

bool SceneEditor::OpCreatePrefab(uint32 objId, const std::string& name, std::string& outRelPath, std::string& errOut)
{
	if (playMode || editorDisabled) { errOut = "not while playing"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }

	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (IsInternalGameObject(go)) { errOut = "cannot make a prefab from an editor object"; return false; }

	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories(project->PrefabsPath(), ec);

	std::string stem = name.empty() ? go->GetName() : name;
	if (stem.empty()) stem = "Prefab";
	// Never overwrite an existing prefab implicitly: a second "Create
	// Prefab" on a same-named object would otherwise silently redefine
	// every instance of the first one.
	std::string rel = "assets/prefabs/" + stem + ".prefab";
	for (int n = 2; fs::exists(project->AbsolutePath(rel), ec); ++n)
		rel = "assets/prefabs/" + stem + "_" + std::to_string(n) + ".prefab";

	const std::string before = SnapshotSubtree(objId);
	nlohmann::json subtree;
	try { subtree = nlohmann::json::parse(before); }
	catch (const std::exception&) { errOut = "could not capture the object"; return false; }

	if (!prefab::WritePrefabFile(subtree, project->AbsolutePath(rel)))
	{
		errOut = "could not write " + rel;
		return false;
	}

	obj->prefabSource = rel;
	// The scene-side half (this object becoming an instance) is undoable;
	// the .prefab it wrote is not removed by that undo, the same way undoing
	// a material assignment does not delete the .mat. The file is inert
	// until something references it.
	PushReplaceCommand(objId, before, "Create Prefab '" + stem + "'");
	MarkSceneDirty();
	outRelPath = rel;
	echo("SUCCESS: Created prefab " + rel);
	return true;
}

uint32 SceneEditor::OpInstantiatePrefab(const std::string& relPath, const Vec3& position, std::string& errOut)
{
	if (playMode || editorDisabled) { errOut = "not while playing"; return 0; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return 0; }

	nlohmann::json subtree = LoadPrefabJson(relPath);
	if (!subtree.is_object()) { errOut = "could not read " + relPath; return 0; }

	// A .prefab file is exactly the shape RawInsertSubtree already takes
	// (SerializeSubtree's {"root", "materials"}), so instantiating one is
	// the insert path the undo system uses, with two fields written in.
	subtree["root"]["position"] = { position.x, position.y, position.z };
	subtree["root"]["prefab"] = relPath;

	SceneObject* created = RawInsertSubtree(subtree.dump(), 0, false, EditorCameraSettings(), true);
	if (!created) { errOut = "could not instantiate " + relPath; return 0; }

	SelectAndFocusSceneObject(created);
	PushAddCommand(created);
	MarkSceneDirty();
	echo("SUCCESS: Instantiated " + relPath);
	return created->GetID();
}

uint32 SceneEditor::RawRebuildPrefabInstance(uint32 objId, const std::string& relPath)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return 0;
	GameObject* go = (GameObject*)obj->GetPTR();

	const uint32 parentId = obj->GetParentID();
	const bool wasCamera = IsSceneCamera(objId);
	const EditorCameraSettings camSettings = wasCamera ? sceneCameras[objId] : EditorCameraSettings();
	const bool hadHelper = (obj->Helper != nullptr);

	nlohmann::json subtree = LoadPrefabJson(relPath);
	if (!subtree.is_object()) return 0;

	// The instance keeps its own name, transform and tags - reverting an
	// object is not the same as moving it back to where the prefab was
	// authored.
	nlohmann::json overrides;
	overrides["name"] = go->GetName();
	overrides["position"] = { go->GetPosition().x, go->GetPosition().y, go->GetPosition().z };
	overrides["rotation"] = { go->GetRotation().x, go->GetRotation().y, go->GetRotation().z };
	overrides["scale"] = { go->GetScale().x, go->GetScale().y, go->GetScale().z };
	nlohmann::json tags = nlohmann::json::array();
	const std::map<uint32, std::string>& tagsMap = go->GetTags();
	for (std::map<uint32, std::string>::const_iterator i = tagsMap.begin(); i != tagsMap.end(); ++i)
		tags.push_back(i->second);
	overrides["tags"] = tags;
	prefab::detail::ApplyOverrides(subtree["root"], overrides);
	subtree["root"]["prefab"] = relPath;

	RawDeleteSubtree(objId);
	SceneObject* rebuilt = RawInsertSubtree(subtree.dump(), parentId, wasCamera, camSettings, hadHelper);
	return rebuilt ? rebuilt->GetID() : 0;
}

bool SceneEditor::OpRevertPrefab(uint32 objId, std::string& errOut)
{
	if (playMode || editorDisabled) { errOut = "not while playing"; return false; }

	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	if (obj->prefabSource.empty()) { errOut = "not a prefab instance"; return false; }

	const std::string rel = obj->prefabSource;
	const uint32 parentId = obj->GetParentID();
	const bool wasCamera = IsSceneCamera(objId);
	const EditorCameraSettings camSettings = wasCamera ? sceneCameras[objId] : EditorCameraSettings();
	const bool hadHelper = (obj->Helper != nullptr);
	const std::string before = SnapshotSubtree(objId);

	const uint32 newId = RawRebuildPrefabInstance(objId, rel);
	if (!newId) { errOut = "could not read " + rel; return false; }

	sceneUndo.Push(std::make_unique<ReplaceGameObjectCommand>(this, parentId, before, SnapshotSubtree(newId),
		wasCamera, camSettings, hadHelper, newId, "Revert to Prefab"));

	SelectAndFocusSceneObject(sceneObjects->GetSceneObject(newId));
	MarkSceneDirty();
	echo("SUCCESS: Reverted to " + rel);
	return true;
}

bool SceneEditor::OpUnpackPrefab(uint32 objId, std::string& errOut)
{
	if (playMode || editorDisabled) { errOut = "not while playing"; return false; }

	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	if (obj->prefabSource.empty()) { errOut = "not a prefab instance"; return false; }

	const std::string rel = obj->prefabSource;
	const std::string before = SnapshotSubtree(objId);

	// Nothing about the objects changes - only that the scene will now store
	// them in full and stop tracking the prefab.
	obj->prefabSource.clear();

	PushReplaceCommand(objId, before, "Unpack Prefab");
	MarkSceneDirty();
	echo("SUCCESS: Unpacked from " + rel);
	return true;
}

bool SceneEditor::PrefabInstanceIsModified(uint32 objId)
{
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->prefabSource.empty()) return false;

	const nlohmann::json prefabJson = LoadPrefabJson(obj->prefabSource);
	if (!prefabJson.is_object()) return false;
	try
	{
		const nlohmann::json subtree = nlohmann::json::parse(SnapshotSubtree(objId));
		return !prefab::MatchesPrefab(subtree["root"], subtree.value("materials", nlohmann::json::array()), prefabJson);
	}
	catch (const std::exception&) { return false; }
}

std::vector<uint32> SceneEditor::FindPrefabInstances(const std::string& relPath, uint32 skipId, bool modified)
{
	std::vector<uint32> out;
	for (std::map<uint32, SceneObject*>::const_iterator i = sceneObjects->GetList().begin();
		i != sceneObjects->GetList().end(); ++i)
	{
		SceneObject* o = i->second;
		if (!o || o->GetType() != SceneObjectTypes::GAMEOBJECT || o->GetID() == skipId) continue;
		if (o->prefabSource != relPath) continue;
		GameObject* go = (GameObject*)o->GetPTR();
		if (!go || IsInternalGameObject(go)) continue;
		if (PrefabInstanceIsModified(o->GetID()) != modified) continue;
		out.push_back(o->GetID());
	}
	return out;
}

bool SceneEditor::OpApplyPrefab(uint32 objId, std::string& errOut)
{
	if (playMode || editorDisabled) { errOut = "not while playing"; return false; }
	if (!project || !project->IsOpen()) { errOut = "no project open"; return false; }

	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	if (obj->prefabSource.empty()) { errOut = "not a prefab instance"; return false; }

	const std::string rel = obj->prefabSource;

	// Which instances to carry along, decided BEFORE the file changes:
	// afterwards every instance differs from it, so there would be no way
	// left to tell "was in sync" from "had local changes of its own" - and
	// silently discarding somebody's local changes is exactly what Apply
	// must not do.
	const std::vector<uint32> inSync = FindPrefabInstances(rel, objId, false);

	nlohmann::json subtree;
	try { subtree = nlohmann::json::parse(SnapshotSubtree(objId)); }
	catch (const std::exception&) { errOut = "could not capture the object"; return false; }

	if (!prefab::WritePrefabFile(subtree, project->AbsolutePath(rel)))
	{
		errOut = "could not write " + rel;
		return false;
	}

	int refreshed = 0;
	for (size_t i = 0; i < inSync.size(); ++i)
		if (RawRebuildPrefabInstance(inSync[i], rel)) ++refreshed;

	// Deliberately outside the undo stack: this wrote a project asset that
	// other scenes reference, and an undo that silently rewrote a file those
	// scenes are already using would be a worse surprise than not offering
	// one. The confirmation at the call site says so.
	sceneUndo.Clear();
	MarkSceneDirty();
	echo("SUCCESS: Applied to " + rel + " (" + std::to_string(refreshed)
		+ " other instance(s) in this scene updated)");
	return true;
}

// ------------------- prefab references in the scene file -------------------
//
// The engine reads and writes scenes whose roots are written out in full.
// These two wrap its calls so that what is stored on disk is references, and
// what the engine ever sees is not.

std::string SceneEditor::ExpandSceneFileForLoad(const std::string& path)
{
	std::ifstream in(path.c_str());
	if (!in.is_open()) return std::string();
	std::stringstream buffer;
	buffer << in.rdbuf();
	in.close();

	if (!project || !project->IsOpen()) return buffer.str();

	nlohmann::json sceneJson;
	try { sceneJson = nlohmann::json::parse(buffer.str()); }
	catch (const std::exception&) { return buffer.str(); } // the engine reports it

	std::vector<prefab::Link> links;
	std::vector<std::string> errors;
	ProjectManager* proj = project;
	prefab::ExpandScene(sceneJson,
		[proj](const std::string& rel) { return prefab::ReadPrefabFile(proj->AbsolutePath(rel)); },
		links, errors);

	for (size_t i = 0; i < errors.size(); ++i)
		echo("ERROR: prefab not found, its objects are missing from this scene: " + errors[i]);

	if (links.empty()) return buffer.str();
	return sceneJson.dump();
}

void SceneEditor::RelinkPrefabInstancesAfterLoad(const std::vector<std::string>& rootPrefabPaths)
{
	// Root order is load order: LoadScene walks "roots" in file order and
	// Scene->Add()s each one, and GetAllGameObjectList() is a vector pushed
	// in that same order. Must run before any editor furniture is attached,
	// or the indices no longer line up.
	std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
	for (size_t i = 0; i < all.size() && i < rootPrefabPaths.size(); ++i)
	{
		if (rootPrefabPaths[i].empty()) continue;
		const uint32 id = sceneObjects->GetSceneObjectID(all[i].get());
		if (SceneObject* obj = sceneObjects->GetSceneObject(id))
			obj->prefabSource = rootPrefabPaths[i];
	}
}

void SceneEditor::CollapseSceneFileAfterSave(const std::string& path)
{
	if (!project || !project->IsOpen()) return;

	// Which root is an instance of what, in the order SaveScene just wrote
	// them (GetAllGameObjectList()). Editor furniture is detached around the
	// save, so this list is user content only - the same list the file has.
	std::vector<std::string> rootPrefabPaths;
	bool anyLinked = false;
	std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
	for (size_t i = 0; i < all.size(); ++i)
	{
		const uint32 id = sceneObjects->GetSceneObjectID(all[i].get());
		SceneObject* obj = sceneObjects->GetSceneObject(id);
		rootPrefabPaths.push_back(obj ? obj->prefabSource : std::string());
		if (obj && !obj->prefabSource.empty()) anyLinked = true;
	}
	if (!anyLinked) return;

	nlohmann::json sceneJson;
	{
		std::ifstream in(path.c_str());
		if (!in.is_open()) return;
		try { in >> sceneJson; }
		catch (const std::exception&) { return; }
	}

	std::vector<std::string> modified, missing;
	ProjectManager* proj = project;
	prefab::CollapseScene(sceneJson, rootPrefabPaths,
		[proj](const std::string& rel) { return prefab::ReadPrefabFile(proj->AbsolutePath(rel)); },
		modified, missing);

	for (size_t i = 0; i < modified.size(); ++i)
		echo("WARNING: prefab instance has local changes, stored in full: " + modified[i]
			+ " (Apply pushes them to every instance, Revert discards them, Unpack drops the link)");
	for (size_t i = 0; i < missing.size(); ++i)
		echo("WARNING: prefab missing, instance stored in full and unlinked: " + missing[i]);

	std::ofstream out(path.c_str());
	if (!out.is_open()) return;
	out << sceneJson.dump(4);
}

// ============================================================================
// UI components
//
// Screen-space UI is authored the same way everything else is: UICanvas /
// UIRect / UIImage / UIText are ordinary components on ordinary GameObjects,
// so they land in the hierarchy, in the scene file and in prefabs with no
// special casing anywhere. These Ops exist for the same reason all the
// others do - so the menu, the agent and the AI assistant apply the exact
// same edit.
// ============================================================================

// A UIText needs a real font file, and a scene that references the editor's
// own bundled font would break the moment it is opened by the player. So:
// an explicitly requested font wins; otherwise the first .ttf already in the
// project is reused; otherwise the editor's font is imported INTO the
// project, which is what makes "Add Text" work on a fresh project instead of
// producing a label that cannot be saved.
std::string SceneEditor::ResolveUIFontPath(const std::string& requested, std::string& errOut)
{
	namespace fs = std::filesystem;
	std::error_code ec;

	if (!requested.empty())
	{
		std::string abs = (project && project->IsOpen()) ? project->AbsolutePath(requested) : requested;
		if (fs::exists(abs, ec)) return abs;
		if (fs::exists(requested, ec)) return requested;
		errOut = "font not found: " + requested;
		return std::string();
	}

	if (project && project->IsOpen())
	{
		const std::string assets = project->AssetsPath();
		if (fs::exists(assets, ec))
		{
			std::string best;
			for (fs::recursive_directory_iterator it(assets, ec), end; it != end && !ec; it.increment(ec))
			{
				if (!it->is_regular_file(ec)) continue;
				std::string ext = it->path().extension().string();
				for (size_t i = 0; i < ext.size(); i++) ext[i] = (char)tolower((unsigned char)ext[i]);
				if (ext == ".ttf" || ext == ".otf") { best = it->path().string(); break; }
			}
			if (!best.empty()) return best;
		}

		// Nothing in the project yet - bring the editor's own font in, so
		// the resulting scene is self-contained.
		const std::string editorFont = "assets/arialbd.ttf";
		if (fs::exists(editorFont, ec))
		{
			std::string imported, importErr;
			if (project->ImportAssetFile(editorFont, imported, &importErr) && !imported.empty())
			{
				echo("Imported the editor's default font into this project for UIText");
				return imported;
			}
		}
		errOut = "no font in the project, and the editor's default could not be imported";
		return std::string();
	}

	// No project open (a bare scene) - the editor's own font is all there
	// is, and a scene like that is not portable anyway.
	if (fs::exists("assets/arialbd.ttf", ec)) return "assets/arialbd.ttf";
	errOut = "no font available";
	return std::string();
}

bool SceneEditor::OpAddUIComponent(uint32 goId, const std::string& kind, const std::string& fontPath, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (!go) { errOut = "object not found"; return false; }

	std::string k = kind;
	for (size_t i = 0; i < k.size(); i++) k[i] = (char)tolower((unsigned char)k[i]);

	// Snapshot before the edit, replace after - the same undo pairing every
	// other component add uses, and it works here only because these
	// components serialize.
	const std::string before = SnapshotSubtree(goId);

	if (k == "canvas")
	{
		go->Add(std::static_pointer_cast<IComponent>(std::make_shared<UICanvas>(1920.f, 1080.f)));
	}
	else if (k == "rect")
	{
		// A visible default rather than the component's own zero-size one:
		// an element you cannot see is an element you cannot select.
		std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
		r->SetAnchoredPosition(Vec2(0.5f, 0.5f), Vec2(-160.f, -48.f), Vec2(320.f, 96.f));
		go->Add(std::static_pointer_cast<IComponent>(r));
	}
	else if (k == "image")
	{
		if (!HasUIRect(go))
		{
			std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
			r->SetAnchoredPosition(Vec2(0.5f, 0.5f), Vec2(-160.f, -48.f), Vec2(320.f, 96.f));
			go->Add(std::static_pointer_cast<IComponent>(r));
		}
		// A dark panel rather than white: the very next thing anyone does
		// is put a label on it, and white-on-white is not a default, it is
		// a bug report.
		go->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.14f, 0.16f, 0.21f, 0.92f))));
	}
	else if (k == "text")
	{
		const std::string font = ResolveUIFontPath(fontPath, errOut);
		if (font.empty()) return false;
		if (!HasUIRect(go))
		{
			std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
			r->SetAnchoredPosition(Vec2(0.5f, 0.5f), Vec2(-160.f, -32.f), Vec2(320.f, 64.f));
			go->Add(std::static_pointer_cast<IComponent>(r));
		}
		std::shared_ptr<Font> f = std::make_shared<Font>(font, 32.f);
		std::shared_ptr<UIText> t = std::make_shared<UIText>(f, "Text", 40.f, Vec4(0.95f, 0.96f, 1.f, 1.f));
		t->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);
		go->Add(std::static_pointer_cast<IComponent>(t));
	}
	else
	{
		errOut = "unknown UI component '" + kind + "' (canvas, rect, image or text)";
		return false;
	}

	PushReplaceCommand(goId, before, std::string("Add UI ") + k);
	MarkSceneDirty();
	return true;
}

bool SceneEditor::HasUIRect(GameObject* go)
{
	if (!go) return false;
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect) return true;
	return false;
}

// Sets any subset of a UI component's properties in one edit. A bag rather
// than one Op per field because these are all "assign this value to that
// component", and because a caller building a menu wants to set eight things
// at once and undo them as one step - which is also what the Properties
// panel does when a drag is released.
bool SceneEditor::OpSetUIProperties(uint32 goId, const json& p, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (!go) { errOut = "object not found"; return false; }
	if (!p.is_object()) { errOut = "expected an object of properties"; return false; }

	UICanvas* canvas = NULL; UIRect* rect = NULL; UIImage* image = NULL; UIText* text = NULL;
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
	{
		if (!cs[i]) continue;
		switch (cs[i]->GetComponentType())
		{
		case ComponentType::UICanvas: canvas = static_cast<UICanvas*>(cs[i].get()); break;
		case ComponentType::UIRect:   rect   = static_cast<UIRect*>(cs[i].get());   break;
		case ComponentType::UIImage:  image  = static_cast<UIImage*>(cs[i].get());  break;
		case ComponentType::UIText:   text   = static_cast<UIText*>(cs[i].get());   break;
		default: break;
		}
	}
	if (!canvas && !rect && !image && !text) { errOut = "'" + obj->GetName() + "' has no UI components"; return false; }

	const std::string before = SnapshotSubtree(goId);
	bool touched = false;

	// Unknown keys are an error rather than a silent no-op: a caller that
	// misspells "tint" should hear about it, not wonder why nothing changed.
	for (json::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		const std::string k = it.key();
		const json& v = it.value();
		auto vec2 = [&](Vec2& out) -> bool {
			if (!v.is_array() || v.size() != 2) return false;
			out = Vec2(v[0].get<f32>(), v[1].get<f32>()); return true;
		};
		auto vec4 = [&](Vec4& out) -> bool {
			if (!v.is_array() || v.size() != 4) return false;
			out = Vec4(v[0].get<f32>(), v[1].get<f32>(), v[2].get<f32>(), v[3].get<f32>()); return true;
		};

		if (rect && (k == "anchorMin" || k == "anchorMax"))
		{
			Vec2 a; if (!vec2(a)) { errOut = k + " must be [x, y]"; return false; }
			if (k == "anchorMin") rect->SetAnchors(a, rect->GetAnchorMax());
			else rect->SetAnchors(rect->GetAnchorMin(), a);
			touched = true;
		}
		else if (rect && (k == "offsetMin" || k == "offsetMax"))
		{
			Vec2 o; if (!vec2(o)) { errOut = k + " must be [x, y]"; return false; }
			if (k == "offsetMin") rect->SetOffsets(o, rect->GetOffsetMax());
			else rect->SetOffsets(rect->GetOffsetMin(), o);
			touched = true;
		}
		else if (rect && k == "pivot")
		{
			Vec2 pv; if (!vec2(pv)) { errOut = "pivot must be [x, y]"; return false; }
			rect->SetPivot(pv); touched = true;
		}
		else if (image && k == "tint")
		{
			Vec4 c; if (!vec4(c)) { errOut = "tint must be [r, g, b, a]"; return false; }
			image->SetTint(c); touched = true;
		}
		else if (image && k == "border")
		{
			Vec4 b; if (!vec4(b)) { errOut = "border must be [left, top, right, bottom]"; return false; }
			image->SetBorder(b); touched = true;
		}
		else if (image && k == "texture")
		{
			if (!v.is_string()) { errOut = "texture must be a path"; return false; }
			const std::string rel = v.get<std::string>();
			if (rel.empty()) image->SetTexture(std::shared_ptr<Texture>());
			else
			{
				std::shared_ptr<Texture> t = std::make_shared<Texture>();
				if (!t->LoadTexture(ResolveAssetPath(rel), TextureType::Texture)) { errOut = "could not load " + rel; return false; }
				t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
				t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
				image->SetTexture(t);
			}
			touched = true;
		}
		else if (text && k == "text")
		{
			if (!v.is_string()) { errOut = "text must be a string"; return false; }
			text->SetText(v.get<std::string>()); touched = true;
		}
		else if (text && k == "size")
		{
			if (!v.is_number()) { errOut = "size must be a number"; return false; }
			text->SetSize(v.get<f32>()); touched = true;
		}
		else if (text && k == "color")
		{
			Vec4 c; if (!vec4(c)) { errOut = "color must be [r, g, b, a]"; return false; }
			text->SetColor(c); touched = true;
		}
		else if (text && (k == "align" || k == "verticalAlign"))
		{
			if (!v.is_string()) { errOut = k + " must be a string"; return false; }
			const std::string n = v.get<std::string>();
			uint32 h = text->GetHorizontalAlignment(), vt = text->GetVerticalAlignment();
			if (k == "align")
				h = (n == "Center" || n == "center") ? UIAlign::Center : (n == "Right" || n == "right") ? UIAlign::Right : UIAlign::Left;
			else
				vt = (n == "Middle" || n == "middle") ? UIVerticalAlign::Middle : (n == "Bottom" || n == "bottom") ? UIVerticalAlign::Bottom : UIVerticalAlign::Top;
			text->SetAlignment(h, vt); touched = true;
		}
		else if (canvas && (k == "referenceWidth" || k == "referenceHeight"))
		{
			if (!v.is_number()) { errOut = k + " must be a number"; return false; }
			const Vec2 r = canvas->GetReferenceResolution();
			canvas->SetReferenceResolution(k == "referenceWidth" ? v.get<f32>() : r.x,
				k == "referenceHeight" ? v.get<f32>() : r.y);
			touched = true;
		}
		else if (canvas && k == "scaleMode")
		{
			if (!v.is_string()) { errOut = "scaleMode must be a string"; return false; }
			const std::string n = v.get<std::string>();
			if (n == "ConstantPixel") canvas->SetScaleMode(UIScaleMode::ConstantPixel);
			else if (n == "MatchHeight") canvas->SetScaleMode(UIScaleMode::MatchHeight);
			else if (n == "Stretch") canvas->SetScaleMode(UIScaleMode::Stretch);
			else if (n == "MatchWidth") canvas->SetScaleMode(UIScaleMode::MatchWidth);
			else { errOut = "scaleMode must be ConstantPixel, MatchWidth, MatchHeight or Stretch"; return false; }
			touched = true;
		}
		else if (canvas && k == "sortOrder")
		{
			if (!v.is_number()) { errOut = "sortOrder must be a number"; return false; }
			canvas->SetSortOrder(v.get<int32>()); touched = true;
		}
		else
		{
			errOut = "'" + k + "' is not a property of this object's UI components";
			return false;
		}
	}

	if (!touched) { errOut = "nothing to set"; return false; }
	PushReplaceCommand(goId, before, "Set UI Properties");
	MarkSceneDirty();
	return true;
}
