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
#include <Pyros3D/Rendering/Components/Layer2D/Layer2D.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Physics/Physics2D/Physics2D.h>
#include <Pyros3D/Rendering/Components/Occluder2D/Occluder2D.h>
#include <Pyros3D/Rendering/Components/BoneBind2D/BoneBind2D.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <Pyros3D/Ext/stb/stb_image.h>
#include <Pyros3D/Ext/stb/stb_image_write.h>

// True when this object already carries a component of that type. These 2D
// components are one-per-object by nature - a second Layer2D or Occluder2D on
// one object is not a richer setup, it is one of them being silently ignored.
static bool HasComponentOfType(GameObject* go, const uint32 type)
{
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == type) return true;
	return false;
}

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

// Every registry id in a live subtree, in the order Adopt() will re-create
// them. Taken immediately before an undo/redo deletes the subtree, and handed
// back to the re-insert, so the object comes back as the same id every other
// undo entry is still holding.
std::vector<uint32> SceneEditor::RawCollectSubtreeIds(uint32 objId)
{
	std::vector<uint32> ids;
	SceneObject* obj = sceneObjects->GetSceneObject(objId);
	if (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT)
		sceneObjects->CollectAdoptOrderIds((GameObject*)obj->GetPTR(), ids);
	return ids;
}

SceneObject* SceneEditor::RawInsertSubtree(const std::string& subtreeJson, uint32 parentId, bool wasCamera, const EditorCameraSettings& camSettings, bool hadHelper, const std::vector<uint32>* preferredIds)
{
	if (subtreeJson.empty()) return NULL;
#ifdef LUA_BINDINGS
	std::shared_ptr<GameObject> go = SceneSerializer::DeserializeSubtree(subtreeJson, scenePath, physics, sharedLua, NULL);
#else
	std::shared_ptr<GameObject> go = SceneSerializer::DeserializeSubtree(subtreeJson, scenePath, physics, NULL, NULL);
#endif
	if (!go) return NULL;
	scene->Add(go);
	size_t idCursor = 0;
	SceneObject* obj = sceneObjects->Adopt(go.get(), parentId, preferredIds, &idCursor);
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

// A sprite is a textured quad, not a new runtime concept - deliberately. The
// data a sprite needs (a mesh, a material, a texture, a transform) is exactly
// what RenderingComponent already carries, so this is an authoring shortcut
// rather than a component type: it builds the plane, gives it its own material
// so tinting one sprite does not tint every other, sizes it from the texture's
// pixel aspect, and turns on the alpha blending a cut-out sprite needs and a
// 3D mesh does not.
bool SceneEditor::OpAddSprite(uint32 goId, const std::string& texturePath, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT)
	{
		errOut = "not a game object";
		return false;
	}
	GameObject* go = (GameObject*)obj->GetPTR();
	const std::string before = SnapshotSubtree(goId);

	std::shared_ptr<Texture> tex;
	f32 aspect = 1.f;
	if (!texturePath.empty())
	{
		tex = std::make_shared<Texture>();
		if (!tex->LoadTexture(ResolveAssetPath(texturePath), TextureType::Texture))
		{
			errOut = "could not load " + texturePath;
			return false;
		}
		tex->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
		tex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		// Square by default; a wide texture gets a wide quad, so a sprite is
		// not stretched the moment it is created.
		if (tex->GetHeight() > 0)
			aspect = (f32)tex->GetWidth() / (f32)tex->GetHeight();
	}

	// Plane takes half-extents (see its constructor), so this is a 2x2 quad
	// scaled by the texture's aspect - a sensible size to then scale in the
	// gizmo rather than a pixel-exact one, which would need a
	// pixels-per-unit convention this engine does not have for world space.
	std::shared_ptr<Renderable> mesh = std::make_shared<Plane>(aspect, 1.f);

	// Its own material, not the shared GenericMaterial every primitive gets:
	// a sprite is expected to carry its own texture and tint.
	//
	// Ordinary 3D lighting by default: a sprite created in a 3D scene should
	// behave like everything around it, and 2D lighting is opt-in per sprite
	// (OpMakeSprite2DLit below) because dropping N.L is only the right answer
	// when the sprite really is lying in a 2D plane.
	// ShaderUsage::Texture is what makes the generated shader sample
	// uColormap at all - SetColorMap() only registers the uniform, it does not
	// turn the sampling on, so a material given a texture without this flag
	// renders flat and looks like a texture that failed to load.
	//
	// Always set, even for a sprite created without one, and backed by a 1x1
	// white texture until a real one is assigned. The options are fixed when
	// the material is constructed, so a sprite built without the flag could
	// never be given a working texture afterwards - which is exactly what the
	// Add > Sprite menu item creates.
	std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(
		ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::Texture);
	mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
	mat->SetColorMap(tex ? tex : UIImage::WhiteTexture());
	// Cut-out sprites are the normal case and a quad has no meaningful back
	// face - both of which are wrong defaults for a 3D mesh and right here.
	mat->EnableBlending();
	mat->BlendingEquation(BlendEq::Add);
	mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
	// EnableBlending() alone is not enough. It sets the blend state, but the
	// DeferredRenderer picks its pass off IsTransparent(): a material without
	// the flag is drawn into the G-buffer, and a G-buffer pass cannot blend -
	// it writes one albedo/normal/depth per pixel, so a fully transparent
	// texel is written out at full opacity. The symptom is a hard ring of the
	// texture's invisible border colour around every sprite, which reads as a
	// lighting or shadow bug and is not one. With the flag the sprite goes
	// through the translucent pass, which is forward-lit (lights are gathered
	// there) so it stays lit and shadowed exactly as before.
	mat->SetTransparencyFlag(true);
	mat->SetCullFace(CullFace::DoubleSided);

	go->Add(std::make_shared<RenderingComponent>(mesh, mat));
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Sprite");
	return true;
}

RenderingComponent* SceneEditor::FindRenderingComponent(GameObject* go)
{
	if (!go) return NULL;
	RenderingComponent* rc = NULL;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
			rc = static_cast<RenderingComponent*>(comps[i].get());
	return rc;
}

SkeletonAnimationInstance* SceneEditor::RebuildSkeletonInstance(RenderingComponent* rc)
{
	if (!rc || rc->GetSkeleton().empty()) return NULL;
	// A fresh SkeletonAnimation each time: an instance snapshots the skeleton
	// in its constructor (sizes every pose array from it), so adding a bone
	// means building a new one rather than mutating the old.
	//
	// Any clips already authored are carried across first. They live on the
	// SkeletonAnimation, so replacing it would quietly throw away every key
	// the moment another bone was added - the kind of loss that only shows up
	// after the work is gone.
	std::vector<Animation> existingClips;
	if (SkeletonAnimationInstance* old =
		static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation()))
		if (old->GetOwner()) existingClips = old->GetOwner()->GetAnimations();

	std::shared_ptr<SkeletonAnimation> anim = std::make_shared<SkeletonAnimation>();
	sceneAssets.skeletonAnimations.push_back(anim);
	if (!existingClips.empty()) anim->SetAnimations(existingClips);
	SkeletonAnimationInstance* inst = anim->CreateInstance(rc);
	if (inst) inst->ResetToBindPose();
	return inst;
}

bool SceneEditor::OpBindToBone2D(uint32 goId, const std::string& boneName, const Vec2 &offset, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	if (boneName.empty()) { errOut = "bind needs a bone name"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	// Checked here rather than left to fail silently at update time: a binding
	// to a bone that does not exist looks exactly like a binding that is not
	// working, and this is the one place that knows the answer.
	bool found = false;
	for (GameObject* p = go->GetParent(); p != NULL && !found; p = p->GetParent())
	{
		RenderingComponent* rc = FindRenderingComponent(p);
		if (!rc) continue;
		const std::map<StringID, Bone> &sk = rc->GetSkeleton();
		for (std::map<StringID, Bone>::const_iterator i = sk.begin(); i != sk.end(); ++i)
			if ((*i).second.name == boneName) { found = true; break; }
	}
	if (!found) { errOut = "no ancestor has a bone named '" + boneName + "'"; return false; }

	const std::string before = SnapshotSubtree(goId);
	// Re-bind rather than stack a second binding: two BoneBind2D on one object
	// would both write the transform and the last one to update would win.
	BoneBind2D* existing = NULL;
	{
		const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
			if (comps[i] && comps[i]->GetComponentType() == ComponentType::BoneBind2D)
				existing = static_cast<BoneBind2D*>(comps[i].get());
	}
	if (existing)
	{
		existing->SetBone(boneName);
		existing->SetOffset(offset);
	}
	else
	{
		std::shared_ptr<BoneBind2D> b = std::make_shared<BoneBind2D>(boneName);
		b->SetOffset(offset);
		go->Add(b);
	}
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Bind To Bone");
	return true;
}

std::vector<Bone> SceneEditor::BonesOf(RenderingComponent* rc)
{
	std::vector<Bone> out;
	if (!rc) return out;
	const std::map<StringID, Bone> &sk = rc->GetSkeleton();
	out.resize(sk.size());
	for (std::map<StringID, Bone>::const_iterator i = sk.begin(); i != sk.end(); ++i)
	{
		const int32 id = (*i).second.self;
		if (id < 0 || (size_t)id >= out.size()) return std::vector<Bone>();
		out[id] = (*i).second;
	}
	return out;
}

void SceneEditor::PushSkeletonUndo(uint32 goId, const std::vector<Bone>& before, const std::string& description)
{
	SceneEditor* self = this;
	auto apply = [self, goId](std::vector<Bone> bones) {
		SceneObject* obj = self->sceneObjects->GetSceneObject(goId);
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		RenderingComponent* rc = FindRenderingComponent((GameObject*)obj->GetPTR());
		if (!rc) return;
		// Clips are carried across by RebuildSkeletonInstance, so undoing a
		// bone edit does not take the animation with it.
		rc->SetSkeleton(bones);
		self->RebuildSkeletonInstance(rc);
		self->MarkSceneDirty();
	};
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	RenderingComponent* rc = (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT)
		? FindRenderingComponent((GameObject*)obj->GetPTR()) : NULL;
	const std::vector<Bone> after = BonesOf(rc);
	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[apply, before]() { apply(before); },
		[apply, after]() { apply(after); },
		description));
}

void SceneEditor::PushClipsUndo(uint32 goId, const std::vector<Animation>& before, const std::string& description)
{
	SceneEditor* self = this;
	auto apply = [self, goId](std::vector<Animation> clips) {
		SceneObject* obj = self->sceneObjects->GetSceneObject(goId);
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		RenderingComponent* rc = FindRenderingComponent((GameObject*)obj->GetPTR());
		if (!rc) return;
		SkeletonAnimationInstance* inst =
			static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
		if (!inst || !inst->GetOwner()) return;
		inst->GetOwner()->SetAnimations(clips);
		self->MarkSceneDirty();
	};
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	RenderingComponent* rc = (obj && obj->GetType() == SceneObjectTypes::GAMEOBJECT)
		? FindRenderingComponent((GameObject*)obj->GetPTR()) : NULL;
	SkeletonAnimationInstance* inst = rc
		? static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation()) : NULL;
	if (!inst || !inst->GetOwner()) return;
	const std::vector<Animation> after = inst->GetOwner()->GetAnimations();
	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[apply, before]() { apply(before); },
		[apply, after]() { apply(after); },
		description));
}

bool SceneEditor::CapturePoseFor(uint32 goId, std::vector<Matrix>& out) const
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return false;
	RenderingComponent* rc = FindRenderingComponent((GameObject*)obj->GetPTR());
	if (!rc) return false;
	SkeletonAnimationInstance* inst =
		static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
	if (!inst) return false;
	inst->CapturePose(out);
	return true;
}

void SceneEditor::PushPoseUndo(uint32 goId, const std::vector<Matrix>& before, const std::string& description)
{
	std::vector<Matrix> after;
	if (!CapturePoseFor(goId, after)) return;
	if (before.size() != after.size()) return;

	bool changed = false;
	for (size_t i = 0; i < before.size() && !changed; i++)
		for (int k = 0; k < 16; k++)
			if (fabsf(before[i].m[k] - after[i].m[k]) > 1e-6f) { changed = true; break; }
	if (!changed) return;   // a drag that moved nothing is not an undo step

	SceneEditor* self = this;
	auto apply = [self, goId](std::vector<Matrix> pose) {
		SceneObject* obj = self->sceneObjects->GetSceneObject(goId);
		if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
		RenderingComponent* rc = FindRenderingComponent((GameObject*)obj->GetPTR());
		if (!rc) return;
		SkeletonAnimationInstance* inst =
			static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
		if (!inst || inst->GetNumberBones() != pose.size()) return;
		inst->ApplyPose(pose);
	};
	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[apply, before]() { apply(before); },
		[apply, after]() { apply(after); },
		description));
	MarkSceneDirty();
}

bool SceneEditor::GetSelectedRig(RenderingComponent*& outRc, SkeletonAnimationInstance*& outInst) const
{
	outRc = NULL; outInst = NULL;
	if (!SelectedSceneObject || SelectedSceneObject->GetType() != SceneObjectTypes::GAMEOBJECT) return false;
	RenderingComponent* rc = FindRenderingComponent((GameObject*)SelectedSceneObject->GetPTR());
	if (!rc || rc->GetSkeleton().empty()) return false;
	SkeletonAnimationInstance* inst =
		static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation());
	if (!inst || !inst->GetOwner()) return false;
	outRc = rc; outInst = inst;
	return true;
}

bool SceneEditor::OpNewClip2D(const std::string& clipName, std::string& errOut)
{
	RenderingComponent* rc = NULL; SkeletonAnimationInstance* inst = NULL;
	if (!GetSelectedRig(rc, inst)) { errOut = "select an object with a skeleton"; return false; }
	if (clipName.empty()) { errOut = "clip needs a name"; return false; }

	std::vector<Animation> clips = inst->GetOwner()->GetAnimations();
	for (size_t i = 0; i < clips.size(); i++)
		if (clips[i].AnimationName == clipName) { errOut = "a clip named '" + clipName + "' already exists"; return false; }

	Animation a;
	a.AnimationName = clipName;
	a.Duration = 1.f;      // a zero-length clip has nothing to scrub
	a.TicksPerSecond = 1.f;
	a.Flags = 0;
	const std::vector<Animation> beforeClips = inst->GetOwner()->GetAnimations();
	clips.push_back(a);
	inst->GetOwner()->SetAnimations(clips);
	MarkSceneDirty();
	PushClipsUndo(SelectedSceneObject->GetID(), beforeClips, "New Clip");
	return true;
}

bool SceneEditor::OpKeyPose2D(const std::string& clipName, const f32 time,
	const std::string& onlyBone, std::string& errOut)
{
	RenderingComponent* rc = NULL; SkeletonAnimationInstance* inst = NULL;
	if (!GetSelectedRig(rc, inst)) { errOut = "select an object with a skeleton"; return false; }
	if (!SelectedSceneObject) { errOut = "nothing selected"; return false; }

	const std::vector<Bone> bones = inst->GetSkeletonBones();
	const std::string objName = ((GameObject*)SelectedSceneObject->GetPTR())->GetName();
	const std::vector<Animation> beforeKey = inst->GetOwner()->GetAnimations();
	bool any = false;
	for (size_t i = 0; i < bones.size(); i++)
	{
		if (!onlyBone.empty() && bones[i].name != onlyBone) continue;
		// Read the pose back out as degrees and re-key through the same path
		// the agent command uses, so there is one implementation of what a
		// key is rather than two that can drift.
		Matrix local = inst->GetBoneLocalTransform(bones[i].self);
		const f32 deg = RADTODEG(local.GetEulerFromRotationMatrix().z);
		std::string e;
		if (AgentKeyBone2D(objName, clipName, bones[i].name, time, deg, e)) any = true;
		else errOut = e;
	}
	if (!any && errOut.empty()) errOut = "nothing keyed";
	if (any) PushClipsUndo(SelectedSceneObject->GetID(), beforeKey,
		onlyBone.empty() ? "Key Pose" : "Key Bone");
	return any;
}

bool SceneEditor::OpDeleteKey2D(const std::string& clipName, const std::string& boneName,
	const f32 time, std::string& errOut)
{
	RenderingComponent* rc = NULL; SkeletonAnimationInstance* inst = NULL;
	if (!GetSelectedRig(rc, inst)) { errOut = "select an object with a skeleton"; return false; }

	const std::vector<Animation> beforeDel = inst->GetOwner()->GetAnimations();
	std::vector<Animation> clips = inst->GetOwner()->GetAnimations();
	for (size_t i = 0; i < clips.size(); i++)
	{
		if (clips[i].AnimationName != clipName) continue;
		for (size_t c = 0; c < clips[i].Channels.size(); c++)
		{
			if (clips[i].Channels[c].NodeName != boneName) continue;
			std::vector<RotationData> &rots = clips[i].Channels[c].rotations;
			std::vector<PositionData> &poss = clips[i].Channels[c].positions;
			for (size_t k = 0; k < rots.size(); k++)
				if (fabsf(rots[k].Time - time) < 1e-3f) { rots.erase(rots.begin() + k); break; }
			// The position key written alongside it goes too, or the bone
			// keeps a translation key with no rotation and the clip samples a
			// pose nobody authored.
			for (size_t k = 0; k < poss.size(); k++)
				if (fabsf(poss[k].Time - time) < 1e-3f) { poss.erase(poss.begin() + k); break; }
			inst->GetOwner()->SetAnimations(clips);
			MarkSceneDirty();
			PushClipsUndo(SelectedSceneObject->GetID(), beforeDel, "Delete Key");
			return true;
		}
	}
	errOut = "no key at that time";
	return false;
}

bool SceneEditor::OpRemoveBone2D(uint32 goId, const std::string& boneName, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	RenderingComponent* rc = FindRenderingComponent(go);
	if (!rc) { errOut = "object has no RenderingComponent"; return false; }

	const std::map<StringID, Bone> &cur = rc->GetSkeleton();
	std::vector<Bone> bones(cur.size());
	for (std::map<StringID, Bone>::const_iterator i = cur.begin(); i != cur.end(); ++i)
	{
		const int32 id = (*i).second.self;
		if (id < 0 || (size_t)id >= bones.size()) { errOut = "skeleton has out-of-range bone ids"; return false; }
		bones[id] = (*i).second;
	}

	int32 target = -1;
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].name == boneName) { target = bones[i].self; break; }
	if (target < 0) { errOut = "bone '" + boneName + "' not found"; return false; }

	// Descendants go with it. Bones are appended parent-before-child, so a
	// single forward sweep is enough to close the set.
	std::vector<bool> doomed(bones.size(), false);
	doomed[target] = true;
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].parent >= 0 && doomed[bones[i].parent]) doomed[i] = true;

	std::vector<int32> remap(bones.size(), -1);
	std::vector<Bone> kept;
	for (size_t i = 0; i < bones.size(); i++)
	{
		if (doomed[i]) continue;
		remap[i] = (int32)kept.size();
		kept.push_back(bones[i]);
	}
	if (kept.size() == bones.size()) { errOut = "nothing removed"; return false; }

	for (size_t i = 0; i < kept.size(); i++)
	{
		kept[i].self = (int32)i;
		kept[i].parent = (kept[i].parent >= 0) ? remap[kept[i].parent] : -1;
	}

	const std::vector<Bone> beforeBones = BonesOf(rc);
	rc->SetSkeleton(kept);
	RebuildSkeletonInstance(rc);
	MarkSceneDirty();
	PushSkeletonUndo(goId, beforeBones, "Remove Bone");
	return true;
}

bool SceneEditor::OpAddBone2D(uint32 goId, const std::string& boneName,
	const std::string& parentBone, const Vec2 &localPos, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	if (boneName.empty()) { errOut = "bone needs a name"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	RenderingComponent* rc = FindRenderingComponent(go);
	if (!rc) { errOut = "object has no RenderingComponent to carry a skeleton"; return false; }

	// Current bones in id order - Bone::self is the index every downstream
	// array is keyed by, so the order has to be preserved exactly.
	const std::map<StringID, Bone> &cur = rc->GetSkeleton();
	std::vector<Bone> bones(cur.size());
	for (std::map<StringID, Bone>::const_iterator i = cur.begin(); i != cur.end(); ++i)
	{
		if ((*i).second.self < 0 || (size_t)(*i).second.self >= bones.size())
		{ errOut = "existing skeleton has out-of-range bone ids"; return false; }
		bones[(*i).second.self] = (*i).second;
	}

	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].name == boneName) { errOut = "a bone named '" + boneName + "' already exists"; return false; }

	int32 parentId = -1;
	if (!parentBone.empty())
	{
		for (size_t i = 0; i < bones.size(); i++)
			if (bones[i].name == parentBone) { parentId = bones[i].self; break; }
		if (parentId < 0) { errOut = "parent bone '" + parentBone + "' not found"; return false; }
	}

	Bone b;
	b.name = boneName;
	b.self = (int32)bones.size();
	b.parent = parentId;
	b.pos = Vec3(localPos.x, localPos.y, 0.f);
	b.rot = Quaternion();
	b.scale = Vec3(1.f, 1.f, 1.f);
	// Local to the parent. The instance seeds its pose array from bindPoseMat
	// and composes each bone through the parent chain, so this is the bone's
	// rest transform, not a global one.
	b.bindPoseMat = Matrix();
	b.bindPoseMat.Translate(b.pos);
	b.skinned = false;

	const std::vector<Bone> beforeBones = BonesOf(rc);
	bones.push_back(b);
	rc->SetSkeleton(bones);
	RebuildSkeletonInstance(rc);

	MarkSceneDirty();
	PushSkeletonUndo(goId, beforeBones, "Add Bone");
	return true;
}

// Reads the normalized pivot back out of the Pivot matrix. The matrix holds
// the negated local offset, so this is the inverse of OpSetSpritePivot's map.
bool SceneEditor::GetSpritePivot(RenderingComponent* rc, Vec2 &outNorm)
{
	if (!rc) return false;
	// The Renderable's bounds, not RenderingMesh::Geometry's: Geometry is the
	// submesh (IGeometry) and a primitive leaves its bounds at zero - only
	// the Renderable itself runs CalculateBounding(). Reading the submesh
	// gave a 0..0 box, so every normalized pivot mapped to the origin and
	// setting one did nothing at all.
	Renderable* rnd = rc->GetRenderable();
	std::vector<RenderingMesh*> &meshes = rc->GetMeshes();
	if (meshes.empty() || !meshes[0] || !rnd) return false;

	const Vec3 mn = rnd->GetBoundingMinValue();
	const Vec3 mx = rnd->GetBoundingMaxValue();
	const f32 w = mx.x - mn.x, h = mx.y - mn.y;
	if (w <= 0.0001f || h <= 0.0001f) return false;

	const Vec3 off = meshes[0]->Pivot.GetTranslation();
	outNorm.x = ((-off.x) - mn.x) / w;
	outNorm.y = ((-off.y) - mn.y) / h;
	return true;
}

// Normalized (0..1 over the geometry's bounds) to the mesh Pivot matrix.
// Applied to every mesh of the component so a multi-submesh sprite keeps one
// pivot rather than a different one per submesh.
bool SceneEditor::OpSetSpritePivot(uint32 goId, const Vec2 &norm, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	RenderingComponent* rc = NULL;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
			rc = static_cast<RenderingComponent*>(comps[i].get());
	if (!rc) { errOut = "object has no RenderingComponent"; return false; }

	// See GetSpritePivot for why this is the Renderable's box, not the
	// submesh's.
	Renderable* rnd = rc->GetRenderable();
	std::vector<RenderingMesh*> &meshes = rc->GetMeshes();
	if (meshes.empty() || !meshes[0] || !rnd) { errOut = "no geometry"; return false; }
	const Vec3 mn = rnd->GetBoundingMinValue();
	const Vec3 mx = rnd->GetBoundingMaxValue();

	const f32 lx = mn.x + norm.x * (mx.x - mn.x);
	const f32 ly = mn.y + norm.y * (mx.y - mn.y);

	Matrix pv;
	pv.Translate(Vec3(-lx, -ly, 0.f));
	for (size_t i = 0; i < meshes.size(); i++)
		if (meshes[i]) meshes[i]->Pivot = pv;
	MarkSceneDirty();
	return true;
}

// Slices a spritesheet into frames and plays them on the object's sprite.
//
// Row-major, left to right then top to bottom, which is how every sheet
// exporter lays them out. Cell size is the integer division of the sheet by
// cols/rows: a sheet whose dimensions are not an exact multiple loses the
// remainder rather than smearing half a pixel of the neighbouring cell into
// every frame.
bool SceneEditor::OpSliceSpritesheet(uint32 goId, const std::string& sheetPath,
	int cols, int rows, f32 fps, bool loop, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "not a game object"; return false; }
	if (cols < 1 || rows < 1) { errOut = "cols and rows must be at least 1"; return false; }
	if (fps <= 0.f) { errOut = "fps must be positive"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	RenderingComponent* rc = NULL;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
		if (comps[i] && comps[i]->GetComponentType() == ComponentType::RenderingComponent)
			rc = static_cast<RenderingComponent*>(comps[i].get());
	if (!rc) { errOut = "object has no RenderingComponent to animate"; return false; }

	const std::string abs = ResolveAssetPath(sheetPath);
	int w = 0, h = 0, bpp = 0;
	uchar* img = stbi_load(abs.c_str(), &w, &h, &bpp, 4);
	if (!img) { errOut = "could not read " + sheetPath; return false; }

	const int cw = w / cols, ch = h / rows;
	if (cw < 1 || ch < 1) { stbi_image_free(img); errOut = "cols/rows larger than the sheet"; return false; }

	namespace fs = std::filesystem;
	const fs::path sheetFile(abs);
	const std::string stem = sheetFile.stem().string();
	const fs::path outDir = sheetFile.parent_path() / (stem + "_frames");
	std::error_code ec;
	fs::create_directories(outDir, ec);

	const std::string before = SnapshotSubtree(goId);

	std::shared_ptr<TextureAnimation> anim = std::make_shared<TextureAnimation>();
	std::vector<uchar> cell((size_t)cw * ch * 4);
	bool ok = true;
	for (int r = 0; r < rows && ok; r++)
	{
		for (int c = 0; c < cols && ok; c++)
		{
			for (int y = 0; y < ch; y++)
				memcpy(&cell[(size_t)y * cw * 4],
					img + (((size_t)(r * ch + y) * w) + (size_t)c * cw) * 4,
					(size_t)cw * 4);

			char name[64];
			snprintf(name, sizeof(name), "frame_%02d.png", r * cols + c);
			const std::string framePath = (outDir / name).string();
			if (!stbi_write_png(framePath.c_str(), cw, ch, 4, cell.data(), cw * 4))
			{
				errOut = "could not write " + framePath; ok = false; break;
			}

			std::shared_ptr<Texture> t = std::make_shared<Texture>();
			if (!t->LoadTexture(framePath, TextureType::Texture)) { errOut = "could not load " + framePath; ok = false; break; }
			t->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
			t->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
			anim->AddFrame(t);
		}
	}
	stbi_image_free(img);
	if (!ok) return false;

	// Kept alive here for the same reason the loader needs an out-assets
	// list: RenderingComponent only holds a raw back-pointer.
	sceneAssets.textureAnimations.push_back(anim);

	TextureAnimationInstance* inst = anim->CreateInstance(fps);
	inst->Play(loop ? -1 : 1);
	rc->SetActiveTextureAnimation(inst);

	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Slice Spritesheet");
	return true;
}

// Swaps a sprite's material for the 2D-lit one: distance-only falloff, no
// N.L. See ShaderUsage::Lighting2D for why - a flat quad has one normal, so a
// light in its own plane is at grazing incidence and leaves it unlit, which is
// exactly where 2D authoring puts lights.

bool SceneEditor::OpMakeSprite2DLit(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT)
	{
		errOut = "not a game object";
		return false;
	}
	GameObject* go = (GameObject*)obj->GetPTR();
	// The material swap is a subtree change like any other - without this
	// it was the one edit in the 2D set that could not be undone.
	const std::string before = SnapshotSubtree(goId);

	// The options are fixed at construction, and whether to sample a texture
	// is one of them, so the existing colour map has to be found before the
	// new material is built rather than copied onto it afterwards.
	std::shared_ptr<Texture> existing;
	{
		const std::vector<std::shared_ptr<IComponent> > &pre = go->GetComponents();
		for (size_t i = 0; i < pre.size() && !existing; i++)
		{
			if (!pre[i] || pre[i]->GetComponentType() != ComponentType::RenderingComponent) continue;
			std::vector<RenderingMesh*> &ms = static_cast<RenderingComponent*>(pre[i].get())->GetMeshes();
			for (size_t m = 0; m < ms.size() && !existing; m++)
			{
				GenericShaderMaterial* prev = ms[m]->Material
					? dynamic_cast<GenericShaderMaterial*>(ms[m]->Material.get()) : NULL;
				if (prev) existing = prev->GetColorMapShared();
			}
		}
	}

	// A GenericShaderMaterial with ShaderUsage::Lighting2D, not a bespoke
	// shader file - see that flag's comment. This serializes completely and
	// therefore survives a scene reload, which the custom-material version
	// did not.
	std::shared_ptr<GenericShaderMaterial> mat = std::make_shared<GenericShaderMaterial>(
		ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::Texture | ShaderUsage::Lighting2D);
	mat->SetColor(Vec4(1.f, 1.f, 1.f, 1.f));
	mat->SetColorMap(existing ? existing : UIImage::WhiteTexture());
	mat->EnableBlending();
	mat->BlendingEquation(BlendEq::Add);
	mat->BlendingFunction(BlendFunc::Src_Alpha, BlendFunc::One_Minus_Src_Alpha);
	// See OpAddSprite for why the flag is needed on top of EnableBlending().
	mat->SetTransparencyFlag(true);
	mat->SetCullFace(CullFace::DoubleSided);

	bool any = false;
	const std::vector<std::shared_ptr<IComponent> > &comps = go->GetComponents();
	for (size_t i = 0; i < comps.size(); i++)
	{
		if (!comps[i] || comps[i]->GetComponentType() != ComponentType::RenderingComponent) continue;
		RenderingComponent* rc = static_cast<RenderingComponent*>(comps[i].get());
		std::vector<RenderingMesh*> &meshes = rc->GetMeshes();
		for (size_t m = 0; m < meshes.size(); m++)
		{
			meshes[m]->Material = mat;
			any = true;
		}
	}
	if (!any) { errOut = "no RenderingComponent to convert"; return false; }
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Use 2D Lighting");
	return true;
}

// A Box2D rigid body. Defaults to a dynamic half-unit box, which is the shape
// a sprite created next to it already has.
bool SceneEditor::OpAddOccluder2D(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT)
	{
		errOut = "not a game object";
		return false;
	}
	GameObject* go = (GameObject*)obj->GetPTR();
	// Snapshot before the edit, replace after - the same undo pairing
	// every other component add uses.
	if (HasComponentOfType(go, ComponentType::Occluder2D))
	{
		errOut = "this object already has an Occluder2D";
		return false;
	}
	const std::string before = SnapshotSubtree(goId);
	go->Add(std::make_shared<Occluder2D>());
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Occluder 2D");
	return true;
}

bool SceneEditor::OpAddPhysics2D(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT)
	{
		errOut = "not a game object";
		return false;
	}
	GameObject* go = (GameObject*)obj->GetPTR();
	// Snapshot before the edit, replace after - the same undo pairing
	// every other component add uses.
	if (HasComponentOfType(go, ComponentType::Physics2D))
	{
		errOut = "this object already has a Physics2D";
		return false;
	}
	const std::string before = SnapshotSubtree(goId);
	go->Add(std::make_shared<Physics2D>());
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Physics 2D");
	return true;
}

bool SceneEditor::OpAddLayer2D(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT)
	{
		errOut = "not a game object";
		return false;
	}
	GameObject* go = (GameObject*)obj->GetPTR();
	// Snapshot before the edit, replace after - the same undo pairing
	// every other component add uses.
	if (HasComponentOfType(go, ComponentType::Layer2D))
	{
		errOut = "this object already has a Layer2D";
		return false;
	}
	const std::string before = SnapshotSubtree(goId);
	go->Add(std::make_shared<Layer2D>());
	MarkSceneDirty();
	PushReplaceCommand(goId, before, "Add Layer 2D");
	return true;
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
	else if (k == "button")
	{
		// A button is an image you can click, so it brings one - a button
		// with nothing to show is a button nobody can find.
		if (!HasUIRect(go))
		{
			std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
			r->SetAnchoredPosition(Vec2(0.5f, 0.5f), Vec2(-160.f, -36.f), Vec2(320.f, 72.f));
			go->Add(std::static_pointer_cast<IComponent>(r));
		}
		bool hasImage = false;
		const std::vector<std::shared_ptr<IComponent> >& existing = go->GetComponents();
		for (size_t i = 0; i < existing.size(); i++)
			if (existing[i] && existing[i]->GetComponentType() == ComponentType::UIImage) hasImage = true;
		if (!hasImage)
			go->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.16f, 0.19f, 0.25f, 0.95f))));

		std::shared_ptr<UIButton> b = std::make_shared<UIButton>();
		// Defaults you can see: without a hover and a press that visibly
		// differ, a new button looks broken even when it works.
		b->State(UIState::Hover).hasTint = true;
		b->State(UIState::Hover).tint = Vec4(0.24f, 0.30f, 0.40f, 0.98f);
		b->State(UIState::Pressed).hasTint = true;
		b->State(UIState::Pressed).tint = Vec4(0.22f, 0.74f, 0.98f, 1.f);
		b->State(UIState::Pressed).offset = Vec2(0.f, 2.f);
		b->State(UIState::Disabled).hasTint = true;
		b->State(UIState::Disabled).tint = Vec4(0.14f, 0.15f, 0.18f, 0.6f);
		go->Add(std::static_pointer_cast<IComponent>(b));
	}
	else if (k == "toggle" || k == "slider" || k == "input" || k == "list"
		|| k == "dropdown" || k == "menu" || k == "popup")
	{
		if (!AddUIWidget(go, goId, k, fontPath, errOut)) return false;
	}
	else
	{
		errOut = "unknown UI component '" + kind +
			"' (canvas, rect, image, text, button, toggle, slider, input, list, dropdown, menu or popup)";
		return false;
	}

	PushReplaceCommand(goId, before, std::string("Add UI ") + k);
	MarkSceneDirty();
	return true;
}

// Builds one of the composite widgets: the component plus the child elements
// it drives. A checkbox with no tick, a slider with no handle or a list with
// no rows is not something anyone would want to be handed and then have to
// assemble - and the names these children are given are exactly the ones the
// components look for by default.
bool SceneEditor::AddUIWidget(GameObject* go, uint32 goId, const std::string& kind,
	const std::string& fontPath, std::string& errOut)
{
	// Every one of these has a label of some kind, so the font is resolved
	// once, up front, and a missing one fails before anything is built.
	const std::string font = ResolveUIFontPath(fontPath, errOut);
	if (font.empty()) return false;

	// A child element: its own object, its own rect, adopted into the
	// registry so it shows up in the tree like anything else the editor made.
	struct Builder {
		SceneEditor* editor;
		uint32 parentId;
		GameObject* MakeChild(GameObject* parent, uint32 parentRegistryId, const char* name,
			const Vec2 &anchorMin, const Vec2 &anchorMax, const Vec2 &offsetMin, const Vec2 &offsetMax,
			uint32 &outId)
		{
			std::shared_ptr<GameObject> child = std::make_shared<GameObject>();
			child->SetName(name);
			std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
			r->SetAnchors(anchorMin, anchorMax);
			r->SetOffsets(offsetMin, offsetMax);
			r->SetPivot(Vec2(0.5f, 0.5f));
			child->Add(std::static_pointer_cast<IComponent>(r));
			parent->Add(child);
			SceneObject* obj = editor->sceneObjects->Adopt(child.get(), parentRegistryId);
			outId = obj ? obj->GetID() : 0;
			return child.get();
		}
	};
	Builder build; build.editor = this; build.parentId = goId;

	// The rect a just-built child carries, for the few places the template
	// has to reach back into one (hiding a menu panel, say).
	struct Local {
		static UIRect* RectOf(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
					return static_cast<UIRect*>(cs[i].get());
			return NULL;
		}
	};

	if (!HasUIRect(go))
	{
		std::shared_ptr<UIRect> r = std::make_shared<UIRect>();
		if (kind == "popup")
		{
			// The root covers the canvas: the scrim has to reach the edges,
			// and "clicked outside the dialog" is measured against a child.
			r->SetAnchors(Vec2(0.f, 0.f), Vec2(1.f, 1.f));
			r->SetOffsets(Vec2(0.f, 0.f), Vec2(0.f, 0.f));
			r->SetPivot(Vec2(0.5f, 0.5f));
		}
		else if (kind == "menu")
		{
			// A strip across the top, which is where a menu bar goes and
			// what makes the titles line up without being moved first.
			r->SetAnchors(Vec2(0.f, 0.f), Vec2(1.f, 0.f));
			r->SetOffsets(Vec2(0.f, 0.f), Vec2(0.f, 32.f));
			r->SetPivot(Vec2(0.5f, 0.5f));
		}
		else
		{
			const bool tall = (kind == "list" || kind == "dropdown");
			r->SetAnchoredPosition(Vec2(0.5f, 0.5f), Vec2(-160.f, tall ? -120.f : -24.f),
				Vec2(320.f, tall ? 240.f : 48.f));
		}
		go->Add(std::static_pointer_cast<IComponent>(r));
	}

	// The frame every one of them sits in.
	const bool wantsFrame = (kind != "toggle" && kind != "menu" && kind != "popup");
	if (wantsFrame)
		go->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.12f, 0.14f, 0.18f, 0.95f))));

	uint32 childId = 0;
	std::shared_ptr<Font> f = std::make_shared<Font>(font, 24.f);

	if (kind == "toggle")
	{
		// The box, and the tick inside it that the toggle shows and hides.
		go->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.16f, 0.19f, 0.25f, 0.95f))));
		GameObject* check = build.MakeChild(go, goId, "Check", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(6.f, 6.f), Vec2(-6.f, -6.f), childId);
		check->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.22f, 0.74f, 0.98f, 1.f))));

		std::shared_ptr<UIToggle> t = std::make_shared<UIToggle>();
		t->State(UIState::Hover).hasTint = true;
		t->State(UIState::Hover).tint = Vec4(0.24f, 0.30f, 0.40f, 0.98f);
		t->State(UIState::Pressed).hasTint = true;
		t->State(UIState::Pressed).tint = Vec4(0.30f, 0.38f, 0.50f, 1.f);
		go->Add(std::static_pointer_cast<IComponent>(t));
	}
	else if (kind == "slider")
	{
		// Fill stretched across the track, handle pinned to a point on it -
		// the two shapes UISlider writes anchors into.
		GameObject* fill = build.MakeChild(go, goId, "Fill", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(2.f, 2.f), Vec2(-2.f, -2.f), childId);
		fill->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.22f, 0.74f, 0.98f, 1.f))));
		GameObject* handle = build.MakeChild(go, goId, "Handle", Vec2(0.f, 0.f), Vec2(0.f, 1.f),
			Vec2(-10.f, -2.f), Vec2(10.f, 2.f), childId);
		handle->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.92f, 0.94f, 1.f, 1.f))));

		std::shared_ptr<UISlider> sl = std::make_shared<UISlider>();
		sl->SetRange(0.f, 1.f);
		sl->SetValue(0.5f);
		go->Add(std::static_pointer_cast<IComponent>(sl));
	}
	else if (kind == "input")
	{
		GameObject* placeholder = build.MakeChild(go, goId, "Placeholder", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(10.f, 0.f), Vec2(-10.f, 0.f), childId);
		std::shared_ptr<UIText> ph = std::make_shared<UIText>(f, "", 22.f, Vec4(0.5f, 0.55f, 0.65f, 1.f));
		ph->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
		placeholder->Add(std::static_pointer_cast<IComponent>(ph));

		GameObject* label = build.MakeChild(go, goId, "Text", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(10.f, 0.f), Vec2(-10.f, 0.f), childId);
		std::shared_ptr<UIText> lt = std::make_shared<UIText>(f, "", 22.f, Vec4(0.95f, 0.96f, 1.f, 1.f));
		lt->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
		label->Add(std::static_pointer_cast<IComponent>(lt));

		GameObject* caret = build.MakeChild(go, goId, "Caret", Vec2(0.f, 0.f), Vec2(0.f, 1.f),
			Vec2(10.f, 8.f), Vec2(12.f, -8.f), childId);
		caret->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.22f, 0.74f, 0.98f, 1.f))));

		std::shared_ptr<UIInput> in = std::make_shared<UIInput>();
		in->SetPlaceholder("Type here");
		go->Add(std::static_pointer_cast<IComponent>(in));
	}
	else if (kind == "list" || kind == "dropdown")
	{
		// A dropdown is a label plus a popup holding a list, so the list
		// half is built into whichever node wants it.
		GameObject* listOwner = go;
		uint32 listOwnerId = goId;

		if (kind == "dropdown")
		{
			GameObject* label = build.MakeChild(go, goId, "Label", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
				Vec2(12.f, 0.f), Vec2(-12.f, 40.f), childId);
			std::shared_ptr<UIText> lt = std::make_shared<UIText>(f, "", 22.f, Vec4(0.95f, 0.96f, 1.f, 1.f));
			lt->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
			label->Add(std::static_pointer_cast<IComponent>(lt));

			// Below the closed dropdown, which is where a popup goes.
			GameObject* popup = build.MakeChild(go, goId, "Popup", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
				Vec2(0.f, 42.f), Vec2(0.f, 162.f), childId);
			popup->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.10f, 0.12f, 0.16f, 0.98f))));
			listOwner = popup;
			listOwnerId = childId;
		}

		// Four rows: enough to cover the viewport, which is all a list ever
		// needs - see UIList's comment.
		const f32 rowHeight = 30.f;
		for (int i = 0; i < 4; i++)
		{
			char name[16];
			snprintf(name, sizeof(name), "Row%d", i);
			uint32 rowId = 0;
			GameObject* row = build.MakeChild(listOwner, listOwnerId, name, Vec2(0.f, 0.f), Vec2(1.f, 0.f),
				Vec2(0.f, (f32)i * rowHeight), Vec2(0.f, (f32)(i + 1) * rowHeight), rowId);

			uint32 partId = 0;
			GameObject* highlight = build.MakeChild(row, rowId, "Highlight", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
				Vec2(0.f, 0.f), Vec2(0.f, 0.f), partId);
			highlight->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.22f, 0.74f, 0.98f, 0.35f))));

			GameObject* rowLabel = build.MakeChild(row, rowId, "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
				Vec2(12.f, 0.f), Vec2(-12.f, 0.f), partId);
			std::shared_ptr<UIText> rt = std::make_shared<UIText>(f, "", 20.f, Vec4(0.90f, 0.92f, 0.98f, 1.f));
			rt->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
			rowLabel->Add(std::static_pointer_cast<IComponent>(rt));
		}

		std::shared_ptr<UIList> list = std::make_shared<UIList>();
		list->SetItemHeight(rowHeight);
		std::vector<std::string> sample;
		// Something in it, so a new list is visibly a list rather than an
		// empty box that looks broken.
		sample.push_back("First"); sample.push_back("Second"); sample.push_back("Third");
		list->SetItems(sample);
		listOwner->Add(std::static_pointer_cast<IComponent>(list));

		if (kind == "dropdown")
		{
			std::shared_ptr<UIDropdown> dd = std::make_shared<UIDropdown>();
			dd->SetOptions(sample);
			dd->SetPlaceholder("Choose...");
			go->Add(std::static_pointer_cast<IComponent>(dd));
		}
	}

	else if (kind == "menu")
	{
		// A bar with two menus, one of which has a submenu: the shape is
		// the documentation. Anything else is copy, paste and rename.
		std::shared_ptr<UIMenu> menu = std::make_shared<UIMenu>();
		go->Add(std::static_pointer_cast<IComponent>(menu));

		const f32 titleWidth = 90.f;
		const f32 rowHeight = 28.f;
		const char* titles[2] = { "File", "Edit" };
		const char* entries[2][3] = { { "New", "Open", "Recent" }, { "Undo", "Redo", "Preferences" } };

		for (int m = 0; m < 2; m++)
		{
			uint32 titleId = 0;
			GameObject* title = build.MakeChild(go, goId, titles[m], Vec2(0.f, 0.f), Vec2(0.f, 1.f),
				Vec2((f32)m * titleWidth, 0.f), Vec2((f32)(m + 1) * titleWidth, 0.f), titleId);
			title->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.16f, 0.19f, 0.25f, 0.f))));

			uint32 partId = 0;
			GameObject* titleLabel = build.MakeChild(title, titleId, "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
				Vec2(0.f, 0.f), Vec2(0.f, 0.f), partId);
			std::shared_ptr<UIText> tl = std::make_shared<UIText>(f, titles[m], 20.f, Vec4(0.92f, 0.94f, 0.99f, 1.f));
			tl->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);
			titleLabel->Add(std::static_pointer_cast<IComponent>(tl));

			// The panel this title opens, hidden until it does.
			uint32 panelId = 0;
			GameObject* panel = build.MakeChild(title, titleId, (std::string(titles[m]) + "Menu").c_str(),
				Vec2(0.f, 1.f), Vec2(0.f, 1.f), Vec2(0.f, 2.f), Vec2(200.f, 2.f + rowHeight * 3.f), panelId);
			panel->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.10f, 0.12f, 0.16f, 0.98f))));
			if (UIRect* pr = Local::RectOf(panel)) pr->SetVisible(false);

			std::shared_ptr<UIMenuItem> titleItem = std::make_shared<UIMenuItem>();
			titleItem->SetSubmenu(panel->GetName());
			titleItem->State(UIState::Hover).hasTint = true;
			titleItem->State(UIState::Hover).tint = Vec4(0.24f, 0.30f, 0.40f, 1.f);
			// A menu title does not sink when pressed - it opens.
			titleItem->State(UIState::Pressed) = titleItem->GetState(UIState::Hover);
			title->Add(std::static_pointer_cast<IComponent>(titleItem));

			for (int e = 0; e < 3; e++)
			{
				uint32 entryId = 0;
				GameObject* entry = build.MakeChild(panel, panelId, entries[m][e], Vec2(0.f, 0.f), Vec2(1.f, 0.f),
					Vec2(0.f, (f32)e * rowHeight), Vec2(0.f, (f32)(e + 1) * rowHeight), entryId);
				entry->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.16f, 0.19f, 0.25f, 0.f))));

				uint32 labelId = 0;
				GameObject* entryLabel = build.MakeChild(entry, entryId, "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
					Vec2(14.f, 0.f), Vec2(-14.f, 0.f), labelId);
				std::shared_ptr<UIText> el = std::make_shared<UIText>(f, entries[m][e], 19.f, Vec4(0.90f, 0.92f, 0.98f, 1.f));
				el->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
				entryLabel->Add(std::static_pointer_cast<IComponent>(el));

				std::shared_ptr<UIMenuItem> item = std::make_shared<UIMenuItem>();
				item->State(UIState::Hover).hasTint = true;
				item->State(UIState::Hover).tint = Vec4(0.22f, 0.74f, 0.98f, 0.85f);
				item->State(UIState::Pressed) = item->GetState(UIState::Hover);

				// The last entry of the first menu opens a submenu, so the
				// nesting is there to look at rather than described.
				if (m == 0 && e == 2)
				{
					uint32 subId = 0;
					GameObject* sub = build.MakeChild(entry, entryId, "RecentMenu",
						Vec2(1.f, 0.f), Vec2(1.f, 0.f), Vec2(0.f, 0.f), Vec2(200.f, rowHeight * 2.f), subId);
					sub->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.10f, 0.12f, 0.16f, 0.98f))));
					if (UIRect* sr = Local::RectOf(sub)) sr->SetVisible(false);
					item->SetSubmenu(sub->GetName());

					for (int r = 0; r < 2; r++)
					{
						uint32 rowId = 0;
						char rowName[32];
						snprintf(rowName, sizeof(rowName), "Recent%d", r + 1);
						GameObject* row = build.MakeChild(sub, subId, rowName, Vec2(0.f, 0.f), Vec2(1.f, 0.f),
							Vec2(0.f, (f32)r * rowHeight), Vec2(0.f, (f32)(r + 1) * rowHeight), rowId);
						row->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.16f, 0.19f, 0.25f, 0.f))));
						uint32 rowLabelId = 0;
						GameObject* rowLabel = build.MakeChild(row, rowId, "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
							Vec2(14.f, 0.f), Vec2(-14.f, 0.f), rowLabelId);
						std::shared_ptr<UIText> rl = std::make_shared<UIText>(f, rowName, 19.f, Vec4(0.90f, 0.92f, 0.98f, 1.f));
						rl->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
						rowLabel->Add(std::static_pointer_cast<IComponent>(rl));
						std::shared_ptr<UIMenuItem> rowItem = std::make_shared<UIMenuItem>();
						rowItem->State(UIState::Hover).hasTint = true;
						rowItem->State(UIState::Hover).tint = Vec4(0.22f, 0.74f, 0.98f, 0.85f);
						rowItem->State(UIState::Pressed) = rowItem->GetState(UIState::Hover);
						row->Add(std::static_pointer_cast<IComponent>(rowItem));
					}
				}

				entry->Add(std::static_pointer_cast<IComponent>(item));
			}
		}
	}
	else if (kind == "popup")
	{
		// A dialog is three things: a full-canvas root that is shown and
		// hidden as one, a scrim over everything under it, and the panel
		// itself. Handing over only the component would leave all three to
		// be worked out.
		std::shared_ptr<UIPopup> popup = std::make_shared<UIPopup>();
		go->Add(std::static_pointer_cast<IComponent>(popup));

		uint32 scrimId = 0;
		GameObject* scrim = build.MakeChild(go, goId, "Scrim", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
			Vec2(0.f, 0.f), Vec2(0.f, 0.f), scrimId);
		scrim->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.f, 0.f, 0.f, 0.55f))));

		uint32 dialogId = 0;
		GameObject* dialog = build.MakeChild(go, goId, "Dialog", Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f),
			Vec2(-220.f, -140.f), Vec2(220.f, 140.f), dialogId);
		dialog->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(Vec4(0.12f, 0.14f, 0.18f, 1.f))));

		uint32 partId = 0;
		GameObject* titleGO = build.MakeChild(dialog, dialogId, "Title", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
			Vec2(24.f, 20.f), Vec2(-24.f, 60.f), partId);
		std::shared_ptr<UIText> title = std::make_shared<UIText>(f, "Are you sure?", 24.f, Vec4(0.95f, 0.96f, 1.f, 1.f));
		title->SetAlignment(UIAlign::Left, UIVerticalAlign::Middle);
		titleGO->Add(std::static_pointer_cast<IComponent>(title));

		GameObject* bodyGO = build.MakeChild(dialog, dialogId, "Body", Vec2(0.f, 0.f), Vec2(1.f, 0.f),
			Vec2(24.f, 68.f), Vec2(-24.f, 150.f), partId);
		std::shared_ptr<UIText> body = std::make_shared<UIText>(f, "This cannot be undone.", 20.f, Vec4(0.62f, 0.66f, 0.75f, 1.f));
		body->SetAlignment(UIAlign::Left, UIVerticalAlign::Top);
		body->SetWordWrap(true);
		bodyGO->Add(std::static_pointer_cast<IComponent>(body));

		// Two buttons, because a dialog with one is a message and a dialog
		// with none is a decoration.
		const char* names[2] = { "Cancel", "Confirm" };
		const Vec4 tints[2] = { Vec4(0.16f, 0.19f, 0.25f, 1.f), Vec4(0.22f, 0.74f, 0.98f, 1.f) };
		for (int b = 0; b < 2; b++)
		{
			uint32 buttonId = 0;
			GameObject* button = build.MakeChild(dialog, dialogId, names[b], Vec2(1.f, 1.f), Vec2(1.f, 1.f),
				Vec2(b == 0 ? -300.f : -150.f, -70.f), Vec2(b == 0 ? -160.f : -20.f, -24.f), buttonId);
			button->Add(std::static_pointer_cast<IComponent>(std::make_shared<UIImage>(tints[b])));
			std::shared_ptr<UIButton> ub = std::make_shared<UIButton>();
			ub->State(UIState::Hover).hasTint = true;
			ub->State(UIState::Hover).tint = Vec4(tints[b].x * 1.3f, tints[b].y * 1.3f, tints[b].z * 1.3f, 1.f);
			ub->State(UIState::Pressed).hasTint = true;
			ub->State(UIState::Pressed).tint = Vec4(tints[b].x * 0.8f, tints[b].y * 0.8f, tints[b].z * 0.8f, 1.f);
			ub->State(UIState::Pressed).offset = Vec2(0.f, 2.f);
			button->Add(std::static_pointer_cast<IComponent>(ub));

			uint32 labelId = 0;
			GameObject* label = build.MakeChild(button, buttonId, "Label", Vec2(0.f, 0.f), Vec2(1.f, 1.f),
				Vec2(0.f, 0.f), Vec2(0.f, 0.f), labelId);
			std::shared_ptr<UIText> lt = std::make_shared<UIText>(f, names[b], 20.f, Vec4(0.95f, 0.96f, 1.f, 1.f));
			lt->SetAlignment(UIAlign::Center, UIVerticalAlign::Middle);
			label->Add(std::static_pointer_cast<IComponent>(lt));
		}

		// Authored open, so it can be laid out. Closing it is one checkbox
		// in the inspector, and a script or a menu entry opens it.
		popup->SetOpen(true);
	}
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

// Every UI property of every UI component on this object, as one flat bag.
// Small - fifteen or so numbers - and it is what undo stores instead of a
// serialized subtree. That matters: replacing a subtree re-creates it with a
// fresh SceneObject id, which orphans every older undo entry pointing at the
// old one, so a second undo did nothing. Values have no such problem.
json SceneEditor::CaptureUIProperties(GameObject* go)
{
	json out = json::object();
	if (!go) return out;
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
	{
		if (!cs[i]) continue;
		switch (cs[i]->GetComponentType())
		{
		case ComponentType::UICanvas:
		{
			UICanvas* c = static_cast<UICanvas*>(cs[i].get());
			out["referenceWidth"] = c->GetReferenceResolution().x;
			out["referenceHeight"] = c->GetReferenceResolution().y;
			switch (c->GetScaleMode())
			{
			case UIScaleMode::ConstantPixel: out["scaleMode"] = "ConstantPixel"; break;
			case UIScaleMode::MatchHeight:   out["scaleMode"] = "MatchHeight"; break;
			case UIScaleMode::Stretch:       out["scaleMode"] = "Stretch"; break;
			default:                         out["scaleMode"] = "MatchWidth"; break;
			}
			out["sortOrder"] = c->GetSortOrder();
			break;
		}
		case ComponentType::UIRect:
		{
			UIRect* r = static_cast<UIRect*>(cs[i].get());
			out["anchorMin"] = json::array({ r->GetAnchorMin().x, r->GetAnchorMin().y });
			out["anchorMax"] = json::array({ r->GetAnchorMax().x, r->GetAnchorMax().y });
			out["offsetMin"] = json::array({ r->GetOffsetMin().x, r->GetOffsetMin().y });
			out["offsetMax"] = json::array({ r->GetOffsetMax().x, r->GetOffsetMax().y });
			out["pivot"] = json::array({ r->GetPivot().x, r->GetPivot().y });
			break;
		}
		case ComponentType::UIImage:
		{
			UIImage* img = static_cast<UIImage*>(cs[i].get());
			out["tint"] = json::array({ img->GetTint().x, img->GetTint().y, img->GetTint().z, img->GetTint().w });
			out["border"] = json::array({ img->GetBorder().x, img->GetBorder().y, img->GetBorder().z, img->GetBorder().w });
			// The path, not the Texture - a path reloads, and an image with
			// no recoverable source (the shared default white, anything
			// generated at runtime) correctly restores as untextured.
			out["texture"] = (img->GetTexture() && !img->GetTexture()->GetFilename().empty())
				? DisplayPath(img->GetTexture()->GetFilename()) : std::string();
			break;
		}
		case ComponentType::UIButton:
		{
			UIButton* b = static_cast<UIButton*>(cs[i].get());
			out["interactable"] = b->IsInteractable();
			out["transition"] = b->GetTransition();
			out["onClick"] = b->GetOnClick();
			// Flat, prefixed keys rather than a nested table: the bag is a
			// flat namespace everywhere else, and "hoverTint" reads better
			// in a call than states.Hover.tint.
			const char* names[3] = { "hover", "pressed", "disabled" };
			const uint32 ids[3] = { UIState::Hover, UIState::Pressed, UIState::Disabled };
			for (int k = 0; k < 3; k++)
			{
				const UIStateStyle &ss = b->GetState(ids[k]);
				if (ss.hasTint) out[std::string(names[k]) + "Tint"] = json::array({ ss.tint.x, ss.tint.y, ss.tint.z, ss.tint.w });
				if (ss.hasTextColor) out[std::string(names[k]) + "TextColor"] = json::array({ ss.textColor.x, ss.textColor.y, ss.textColor.z, ss.textColor.w });
			}
			out["pressedOffset"] = json::array({ b->GetState(UIState::Pressed).offset.x, b->GetState(UIState::Pressed).offset.y });
			break;
		}
		case ComponentType::UIPopup:
		{
			UIPopup* p = static_cast<UIPopup*>(cs[i].get());
			out["open"] = p->IsOpen();
			out["modal"] = p->IsModalPopup();
			out["closeOnEscape"] = p->ClosesOnEscape();
			out["closeOnOutside"] = p->ClosesOnOutside();
			out["dialogElement"] = p->GetDialogElement();
			out["onClose"] = p->GetOnClose();
			break;
		}
		case ComponentType::UIMenuItem:
		{
			UIMenuItem* m = static_cast<UIMenuItem*>(cs[i].get());
			out["submenu"] = m->GetSubmenu();
			out["interactable"] = m->IsInteractable();
			out["transition"] = m->GetTransition();
			out["onClick"] = m->GetOnClick();
			break;
		}
		case ComponentType::UIToggle:
		{
			UIToggle* t = static_cast<UIToggle*>(cs[i].get());
			out["value"] = t->GetValue();
			out["interactable"] = t->IsInteractable();
			out["transition"] = t->GetTransition();
			out["onClick"] = t->GetOnClick();
			out["onChange"] = t->GetOnChange();
			out["check"] = t->GetCheckElement();
			out["group"] = t->GetGroup();
			const char* names[3] = { "hover", "pressed", "disabled" };
			const uint32 ids[3] = { UIState::Hover, UIState::Pressed, UIState::Disabled };
			for (int k = 0; k < 3; k++)
			{
				const UIStateStyle &ss = t->GetState(ids[k]);
				if (ss.hasTint) out[std::string(names[k]) + "Tint"] = json::array({ ss.tint.x, ss.tint.y, ss.tint.z, ss.tint.w });
				if (ss.hasTextColor) out[std::string(names[k]) + "TextColor"] = json::array({ ss.textColor.x, ss.textColor.y, ss.textColor.z, ss.textColor.w });
			}
			break;
		}
		case ComponentType::UISlider:
		{
			UISlider* sl = static_cast<UISlider*>(cs[i].get());
			out["value"] = sl->GetValue();
			out["min"] = sl->GetMin();
			out["max"] = sl->GetMax();
			out["step"] = sl->GetStep();
			out["vertical"] = sl->IsVertical();
			out["interactable"] = sl->IsInteractable();
			out["onChange"] = sl->GetOnChange();
			out["fill"] = sl->GetFillElement();
			out["handle"] = sl->GetHandleElement();
			break;
		}
		case ComponentType::UIInput:
		{
			UIInput* in = static_cast<UIInput*>(cs[i].get());
			out["text"] = in->GetText();
			out["placeholder"] = in->GetPlaceholder();
			out["maxLength"] = in->GetMaxLength();
			out["password"] = in->IsPassword();
			out["readOnly"] = in->IsReadOnly();
			out["filter"] = in->GetFilter();
			out["blinkRate"] = in->GetBlinkRate();
			out["interactable"] = in->IsInteractable();
			out["onChange"] = in->GetOnChange();
			out["onSubmit"] = in->GetOnSubmit();
			break;
		}
		case ComponentType::UIList:
		{
			UIList* l = static_cast<UIList*>(cs[i].get());
			json arr = json::array();
			for (size_t k = 0; k < l->GetItems().size(); k++) arr.push_back(l->GetItems()[k]);
			out["items"] = arr;
			out["selected"] = l->GetSelected();
			out["itemHeight"] = l->GetItemHeight();
			out["interactable"] = l->IsInteractable();
			out["onChange"] = l->GetOnChange();
			out["onSubmit"] = l->GetOnSubmit();
			break;
		}
		case ComponentType::UIDropdown:
		{
			UIDropdown* d = static_cast<UIDropdown*>(cs[i].get());
			json arr = json::array();
			for (size_t k = 0; k < d->GetOptions().size(); k++) arr.push_back(d->GetOptions()[k]);
			out["options"] = arr;
			out["selected"] = d->GetSelected();
			out["placeholder"] = d->GetPlaceholder();
			out["interactable"] = d->IsInteractable();
			out["onChange"] = d->GetOnChange();
			break;
		}
		case ComponentType::UIText:
		{
			UIText* t = static_cast<UIText*>(cs[i].get());
			out["text"] = t->GetText();
			out["size"] = t->GetSize();
			out["color"] = json::array({ t->GetColor().x, t->GetColor().y, t->GetColor().z, t->GetColor().w });
			out["wrap"] = t->IsWordWrap();
			out["sdf"] = t->IsFontSDF();
			out["align"] = (t->GetHorizontalAlignment() == UIAlign::Center) ? "Center"
				: (t->GetHorizontalAlignment() == UIAlign::Right) ? "Right" : "Left";
			out["verticalAlign"] = (t->GetVerticalAlignment() == UIVerticalAlign::Middle) ? "Middle"
				: (t->GetVerticalAlignment() == UIVerticalAlign::Bottom) ? "Bottom" : "Top";
			break;
		}
		default: break;
		}
	}
	return out;
}

// Applies a bag of UI properties. No undo entry - callers that want one wrap
// this in a pair of bags (see OpSetUIProperties and the Properties panel).
bool SceneEditor::RawSetUIProperties(uint32 goId, const json& p, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();
	if (!go) { errOut = "object not found"; return false; }
	if (!p.is_object()) { errOut = "expected an object of properties"; return false; }

	UICanvas* canvas = NULL; UIRect* rect = NULL; UIImage* image = NULL; UIText* text = NULL; UIButton* button = NULL;
	UIToggle* toggle = NULL; UISlider* slider = NULL; UIInput* input = NULL;
	UIList* list = NULL; UIDropdown* dropdown = NULL; UIMenuItem* menuItem = NULL;
	UIPopup* popup = NULL;
	// Whatever interactive component is on this object, for the handful of
	// keys every widget has.
	UIWidget* widget = NULL;
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
		case ComponentType::UIButton: button = static_cast<UIButton*>(cs[i].get()); break;
		case ComponentType::UIToggle:
			// A toggle is a button, so it answers the button keys too -
			// the states, the transition, the click handler.
			toggle = static_cast<UIToggle*>(cs[i].get());
			button = toggle;
			break;
		case ComponentType::UIMenuItem:
			// And so is a menu entry.
			menuItem = static_cast<UIMenuItem*>(cs[i].get());
			button = menuItem;
			break;
		case ComponentType::UIPopup:  popup = static_cast<UIPopup*>(cs[i].get()); break;
		case ComponentType::UISlider:   slider   = static_cast<UISlider*>(cs[i].get());   break;
		case ComponentType::UIInput:    input    = static_cast<UIInput*>(cs[i].get());    break;
		case ComponentType::UIList:     list     = static_cast<UIList*>(cs[i].get());     break;
		case ComponentType::UIDropdown: dropdown = static_cast<UIDropdown*>(cs[i].get()); break;
		default: break;
		}
		if (UIWidget* w = dynamic_cast<UIWidget*>(cs[i].get())) widget = w;
	}
	if (!canvas && !rect && !image && !text && !widget) { errOut = "'" + obj->GetName() + "' has no UI components"; return false; }

	bool touched = false;
	// Unknown keys are an error rather than a silent no-op: a caller that
	// misspells "tint" should hear about it, not wonder why nothing changed.
	for (json::const_iterator it = p.begin(); it != p.end(); ++it)
	{
		const std::string k = it.key();
		const json& v = it.value();
		Vec2 v2; Vec4 v4;
		// Numbers, not merely an array of the right length: a list of four
		// items is an array of size 4, and reading it as a colour throws.
		const bool allNumbers = v.is_array() && [&]() {
			for (size_t n = 0; n < v.size(); n++) if (!v[n].is_number()) return false;
			return true;
		}();
		const bool isVec2 = allNumbers && v.size() == 2;
		const bool isVec4 = allNumbers && v.size() == 4;
		if (isVec2) v2 = Vec2(v[0].get<f32>(), v[1].get<f32>());
		if (isVec4) v4 = Vec4(v[0].get<f32>(), v[1].get<f32>(), v[2].get<f32>(), v[3].get<f32>());

		if (rect && (k == "anchorMin" || k == "anchorMax"))
		{
			if (!isVec2) { errOut = k + " must be [x, y]"; return false; }
			if (k == "anchorMin") rect->SetAnchors(v2, rect->GetAnchorMax());
			else rect->SetAnchors(rect->GetAnchorMin(), v2);
			touched = true;
		}
		else if (rect && (k == "offsetMin" || k == "offsetMax"))
		{
			if (!isVec2) { errOut = k + " must be [x, y]"; return false; }
			if (k == "offsetMin") rect->SetOffsets(v2, rect->GetOffsetMax());
			else rect->SetOffsets(rect->GetOffsetMin(), v2);
			touched = true;
		}
		else if (rect && k == "pivot")
		{
			if (!isVec2) { errOut = "pivot must be [x, y]"; return false; }
			rect->SetPivot(v2); touched = true;
		}
		else if (image && k == "tint")
		{
			if (!isVec4) { errOut = "tint must be [r, g, b, a]"; return false; }
			image->SetTint(v4); touched = true;
		}
		else if (image && k == "border")
		{
			if (!isVec4) { errOut = "border must be [left, top, right, bottom]"; return false; }
			image->SetBorder(v4); touched = true;
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
			if (!isVec4) { errOut = "color must be [r, g, b, a]"; return false; }
			text->SetColor(v4); touched = true;
		}
		else if (text && k == "sdf")
		{
			if (!v.is_boolean()) { errOut = "sdf must be true or false"; return false; }
			text->SetFontSDF(v.get<bool>()); touched = true;
		}
		else if (text && k == "wrap")
		{
			if (!v.is_boolean()) { errOut = "wrap must be true or false"; return false; }
			text->SetWordWrap(v.get<bool>()); touched = true;
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
		else if (button && k == "interactable")
		{
			if (!v.is_boolean()) { errOut = "interactable must be true or false"; return false; }
			button->SetInteractable(v.get<bool>()); touched = true;
		}
		else if (button && k == "transition")
		{
			if (!v.is_number()) { errOut = "transition must be a number of seconds"; return false; }
			button->SetTransition(v.get<f32>()); touched = true;
		}
		else if (button && k == "onClick")
		{
			if (!v.is_string()) { errOut = "onClick must be a handler name"; return false; }
			button->SetOnClick(v.get<std::string>()); touched = true;
		}
		else if (button && k == "pressedOffset")
		{
			if (!isVec2) { errOut = "pressedOffset must be [x, y]"; return false; }
			button->State(UIState::Pressed).offset = v2; touched = true;
		}
		else if (button && (k == "hoverTint" || k == "pressedTint" || k == "disabledTint"))
		{
			if (!isVec4) { errOut = k + " must be [r, g, b, a]"; return false; }
			const uint32 st = (k[0] == 'h') ? UIState::Hover : (k[0] == 'p') ? UIState::Pressed : UIState::Disabled;
			button->State(st).hasTint = true; button->State(st).tint = v4; touched = true;
		}
		else if (button && (k == "hoverTextColor" || k == "pressedTextColor" || k == "disabledTextColor"))
		{
			if (!isVec4) { errOut = k + " must be [r, g, b, a]"; return false; }
			const uint32 st = (k[0] == 'h') ? UIState::Hover : (k[0] == 'p') ? UIState::Pressed : UIState::Disabled;
			button->State(st).hasTextColor = true; button->State(st).textColor = v4; touched = true;
		}
		// ---- the rest of the widget set ----
		// A node carries one widget, so these keys never compete: an input's
		// label and a list's rows are children, not siblings of the
		// component that drives them.
		else if (toggle && k == "value")
		{
			if (!v.is_boolean()) { errOut = "value must be true or false"; return false; }
			toggle->SetValue(v.get<bool>()); touched = true;
		}
		else if (popup && k == "open")
		{
			if (!v.is_boolean()) { errOut = "open must be true or false"; return false; }
			popup->SetOpen(v.get<bool>()); touched = true;
		}
		else if (popup && k == "modal")
		{
			if (!v.is_boolean()) { errOut = "modal must be true or false"; return false; }
			popup->SetModal(v.get<bool>()); touched = true;
		}
		else if (popup && k == "closeOnEscape")
		{
			if (!v.is_boolean()) { errOut = "closeOnEscape must be true or false"; return false; }
			popup->SetCloseOnEscape(v.get<bool>()); touched = true;
		}
		else if (popup && k == "closeOnOutside")
		{
			if (!v.is_boolean()) { errOut = "closeOnOutside must be true or false"; return false; }
			popup->SetCloseOnOutside(v.get<bool>()); touched = true;
		}
		else if (popup && k == "dialogElement")
		{
			if (!v.is_string()) { errOut = "dialogElement must be an element name"; return false; }
			popup->SetDialogElement(v.get<std::string>()); touched = true;
		}
		else if (popup && k == "onClose")
		{
			if (!v.is_string()) { errOut = "onClose must be a handler name"; return false; }
			popup->SetOnClose(v.get<std::string>()); touched = true;
		}
		else if (menuItem && k == "submenu")
		{
			if (!v.is_string()) { errOut = "submenu must be an element name"; return false; }
			menuItem->SetSubmenu(v.get<std::string>()); touched = true;
		}
		else if (toggle && k == "check")
		{
			if (!v.is_string()) { errOut = "check must be an element name"; return false; }
			toggle->SetCheckElement(v.get<std::string>()); touched = true;
		}
		else if (toggle && k == "group")
		{
			if (!v.is_string()) { errOut = "group must be a name"; return false; }
			toggle->SetGroup(v.get<std::string>()); touched = true;
		}
		else if (slider && k == "value")
		{
			if (!v.is_number()) { errOut = "value must be a number"; return false; }
			slider->SetValue(v.get<f32>()); touched = true;
		}
		else if (slider && (k == "min" || k == "max"))
		{
			if (!v.is_number()) { errOut = k + " must be a number"; return false; }
			if (k == "min") slider->SetRange(v.get<f32>(), slider->GetMax());
			else slider->SetRange(slider->GetMin(), v.get<f32>());
			touched = true;
		}
		else if (slider && k == "step")
		{
			if (!v.is_number()) { errOut = "step must be a number"; return false; }
			slider->SetStep(v.get<f32>()); touched = true;
		}
		else if (slider && k == "vertical")
		{
			if (!v.is_boolean()) { errOut = "vertical must be true or false"; return false; }
			slider->SetVertical(v.get<bool>()); touched = true;
		}
		else if (slider && k == "fill")
		{
			if (!v.is_string()) { errOut = "fill must be an element name"; return false; }
			slider->SetFillElement(v.get<std::string>()); touched = true;
		}
		else if (slider && k == "handle")
		{
			if (!v.is_string()) { errOut = "handle must be an element name"; return false; }
			slider->SetHandleElement(v.get<std::string>()); touched = true;
		}
		else if (input && k == "text")
		{
			if (!v.is_string()) { errOut = "text must be a string"; return false; }
			input->SetText(v.get<std::string>()); touched = true;
		}
		else if (input && k == "placeholder")
		{
			if (!v.is_string()) { errOut = "placeholder must be a string"; return false; }
			input->SetPlaceholder(v.get<std::string>()); touched = true;
		}
		else if (input && k == "maxLength")
		{
			if (!v.is_number()) { errOut = "maxLength must be a number"; return false; }
			input->SetMaxLength((uint32)v.get<int>()); touched = true;
		}
		else if (input && k == "password")
		{
			if (!v.is_boolean()) { errOut = "password must be true or false"; return false; }
			input->SetPassword(v.get<bool>()); touched = true;
		}
		else if (input && k == "readOnly")
		{
			if (!v.is_boolean()) { errOut = "readOnly must be true or false"; return false; }
			input->SetReadOnly(v.get<bool>()); touched = true;
		}
		else if (input && k == "filter")
		{
			if (!v.is_string()) { errOut = "filter must be a string of allowed characters"; return false; }
			input->SetFilter(v.get<std::string>()); touched = true;
		}
		else if (input && k == "blinkRate")
		{
			if (!v.is_number()) { errOut = "blinkRate must be a number"; return false; }
			input->SetBlinkRate(v.get<f32>()); touched = true;
		}
		else if ((list || dropdown) && (k == "items" || k == "options"))
		{
			if (!v.is_array()) { errOut = k + " must be an array of strings"; return false; }
			std::vector<std::string> values;
			for (size_t n = 0; n < v.size(); n++)
			{
				if (!v[n].is_string()) { errOut = k + " must be an array of strings"; return false; }
				values.push_back(v[n].get<std::string>());
			}
			if (list) list->SetItems(values);
			if (dropdown) dropdown->SetOptions(values);
			touched = true;
		}
		else if ((list || dropdown) && k == "selected")
		{
			if (!v.is_number()) { errOut = "selected must be an index, or -1"; return false; }
			if (list) list->SetSelected(v.get<int>());
			if (dropdown) dropdown->SetSelected(v.get<int>());
			touched = true;
		}
		else if (list && k == "itemHeight")
		{
			if (!v.is_number()) { errOut = "itemHeight must be a number"; return false; }
			list->SetItemHeight(v.get<f32>()); touched = true;
		}
		else if (dropdown && k == "placeholder")
		{
			if (!v.is_string()) { errOut = "placeholder must be a string"; return false; }
			dropdown->SetPlaceholder(v.get<std::string>()); touched = true;
		}
		else if (widget && k == "interactable")
		{
			if (!v.is_boolean()) { errOut = "interactable must be true or false"; return false; }
			widget->SetInteractable(v.get<bool>()); touched = true;
		}
		else if (widget && k == "onChange")
		{
			if (!v.is_string()) { errOut = "onChange must be a handler name"; return false; }
			widget->SetOnChange(v.get<std::string>()); touched = true;
		}
		else if (input && k == "onSubmit")
		{
			if (!v.is_string()) { errOut = "onSubmit must be a handler name"; return false; }
			input->SetOnSubmit(v.get<std::string>()); touched = true;
		}
		else if (list && k == "onSubmit")
		{
			if (!v.is_string()) { errOut = "onSubmit must be a handler name"; return false; }
			list->SetOnSubmit(v.get<std::string>()); touched = true;
		}
		else
		{
			errOut = "'" + k + "' is not a property of this object's UI components";
			return false;
		}
	}

	if (!touched) { errOut = "nothing to set"; return false; }
	MarkSceneDirty();
	return true;
}

// Applies a bag and records the reverse, so any number of consecutive edits
// to the same element undo one at a time.
void SceneEditor::PushUIPropertyUndo(uint32 goId, const json& before, const json& after, const char* what)
{
	if (before == after) return;

	// This is the single place a hand-edit to a UI property lands, so it is
	// also where an override is recorded. Applying a style does NOT come
	// through here (it pushes its own command), which is exactly the
	// distinction wanted: the style setting a tint is not an override of
	// itself, and the user then changing that tint is.
	if (SceneObject* obj = sceneObjects->GetSceneObject(goId))
		if (obj->GetType() == SceneObjectTypes::GAMEOBJECT)
		{
			GameObject* go = (GameObject*)obj->GetPTR();
			UIRect* rect = NULL;
			const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
				{ rect = static_cast<UIRect*>(cs[i].get()); break; }
			if (rect && !rect->GetStyleRef().empty())
				for (json::const_iterator it = after.begin(); it != after.end(); ++it)
					if (before.find(it.key()) == before.end() || before[it.key()] != it.value())
						rect->AddStyleOverride(it.key());
		}

	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[this, goId, before]() { std::string e; RawSetUIProperties(goId, before, e); },
		[this, goId, after]()  { std::string e; RawSetUIProperties(goId, after, e); },
		what));
}

bool SceneEditor::OpSetUIProperties(uint32 goId, const json& p, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	// Only the keys being set, so undoing one edit does not quietly revert
	// an unrelated property somebody changed in between.
	const json all = CaptureUIProperties(go);
	json before = json::object();
	if (p.is_object())
		for (json::const_iterator it = p.begin(); it != p.end(); ++it)
			if (all.find(it.key()) != all.end()) before[it.key()] = all[it.key()];

	if (!RawSetUIProperties(goId, p, errOut)) return false;
	PushUIPropertyUndo(goId, before, p, "Set UI Properties");
	return true;
}

// ============================================================================
// UI styles
//
// See shared/UIStyleResolver.h for why the format lives outside the engine.
// These are the editor's three verbs over it: apply one to an element, write
// one out from an element, and re-apply everything after a load.
// ============================================================================

std::string SceneEditor::UIStylePalettePath() const
{
	// One palette per project, at a fixed path. A style names "@accent" and
	// something has to say what that is; making it discoverable beats making
	// it configurable, since a project with two palettes has a theme problem
	// rather than a path problem.
	if (!project || !project->IsOpen()) return std::string();
	return project->AbsolutePath("assets/ui/theme.palette");
}

bool SceneEditor::OpApplyUIStyle(uint32 goId, const std::string& stylePath, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	UIRect* rect = NULL;
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
		{ rect = static_cast<UIRect*>(cs[i].get()); break; }
	if (!rect) { errOut = "'" + obj->GetName() + "' is not a UI element"; return false; }

	const std::string abs = (project && project->IsOpen()) ? project->AbsolutePath(stylePath) : stylePath;
	nlohmann::json style;
	if (!uistyle::ReadJsonFile(abs, style)) { errOut = "could not read " + stylePath; return false; }

	nlohmann::json bag;
	const uistyle::Palette palette = uistyle::LoadPalette(UIStylePalettePath());
	if (!uistyle::Resolve(style, palette, bag, errOut)) return false;

	const json before = CaptureUIProperties(go);
	const std::string beforeRef = rect->GetStyleRef();
	const std::string assetRoot = (project && project->IsOpen()) ? project->GetProjectPath() : std::string();
	if (!uistyle::ApplyProperties(go, bag, assetRoot, errOut)) return false;

	// The reference is what makes the next edit of the style file reach this
	// element - without it, applying a style is a one-off paste.
	const std::string afterRef = stylePath;
	rect->SetStyleRef(afterRef);
	// Applying a style is a fresh start - whatever was hand-edited relative
	// to the PREVIOUS style has no meaning under this one.
	const std::vector<std::string> beforeOverrides = rect->GetStyleOverrides();
	rect->ClearStyleOverrides();
	const json after = CaptureUIProperties(go);

	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[this, goId, before, beforeRef, beforeOverrides]() {
			std::string e; RawSetUIProperties(goId, before, e); RawSetUIStyleRef(goId, beforeRef);
			RawSetUIStyleOverrides(goId, beforeOverrides);
		},
		[this, goId, after, afterRef]() {
			std::string e; RawSetUIProperties(goId, after, e); RawSetUIStyleRef(goId, afterRef);
			RawSetUIStyleOverrides(goId, std::vector<std::string>());
		},
		"Apply UI Style"));
	MarkSceneDirty();
	return true;
}

void SceneEditor::RawSetUIStyleRef(uint32 goId, const std::string& ref)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	const std::vector<std::shared_ptr<IComponent> >& cs = ((GameObject*)obj->GetPTR())->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
		{ static_cast<UIRect*>(cs[i].get())->SetStyleRef(ref); return; }
}

bool SceneEditor::OpExtractUIStyle(uint32 goId, const std::string& name, std::string& outPath, std::string& errOut)
{
	if (!project || !project->IsOpen()) { errOut = "open a project first"; return false; }
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	std::string stem = name.empty() ? obj->GetName() : name;
	if (stem.empty()) stem = "Style";

	namespace fs = std::filesystem;
	std::error_code ec;
	const std::string dir = project->AbsolutePath("assets/ui");
	fs::create_directories(dir, ec);
	const std::string abs = dir + "/" + stem + ".uistyle";

	const uistyle::Palette palette = uistyle::LoadPalette(UIStylePalettePath());
	const nlohmann::json style = uistyle::ExtractFromElement(go, palette);
	if (!uistyle::WriteJsonFile(abs, style)) { errOut = "could not write " + abs; return false; }

	outPath = project->RelativePath(abs);
	// The element it came from adopts it, so promoting a hand-authored
	// button immediately makes that button one of the style's users rather
	// than an unmanaged copy of it.
	std::string applyErr;
	OpApplyUIStyle(goId, outPath, applyErr);
	return true;
}

// Re-applies styles when a .uistyle or the palette changes on disk. Polled
// rather than watched: there is no file watcher in this editor, the set of
// files is tiny (one palette plus whatever styles the open scene references),
// and half a second of latency is invisible next to alt-tabbing back from an
// editor. Without it, styling is edit-file, reload-scene, look - which is
// exactly the loop a style asset exists to avoid.
void SceneEditor::PollUIStyleFiles(const f64 time)
{
	if (!project || !project->IsOpen() || playMode) return;
	if (time - lastUIStylePoll < 0.5) return;
	lastUIStylePoll = time;

	namespace fs = std::filesystem;
	std::error_code ec;

	// Which files matter is derived from the scene each time, so a style
	// that stops being referenced stops being watched.
	std::vector<std::string> paths;
	paths.push_back(UIStylePalettePath());
	std::vector<std::shared_ptr<GameObject> > &roots = scene->GetAllGameObjectList();
	std::vector<GameObject*> stack;
	for (size_t i = 0; i < roots.size(); i++) stack.push_back(roots[i].get());
	while (!stack.empty())
	{
		GameObject* go = stack.back(); stack.pop_back();
		if (!go) continue;
		const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
			if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
			{
				const std::string ref = static_cast<UIRect*>(cs[i].get())->GetStyleRef();
				if (!ref.empty()) paths.push_back(project->AbsolutePath(ref));
				break;
			}
		const std::vector<std::shared_ptr<GameObject> >& kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++) stack.push_back(kids[i].get());
	}

	bool changed = false;
	for (size_t i = 0; i < paths.size(); i++)
	{
		if (paths[i].empty()) continue;
		const fs::file_time_type t = fs::last_write_time(paths[i], ec);
		if (ec) { ec.clear(); continue; }
		std::map<std::string, fs::file_time_type>::iterator seen = uiStyleMTimes.find(paths[i]);
		if (seen == uiStyleMTimes.end()) uiStyleMTimes[paths[i]] = t;
		else if (seen->second != t) { seen->second = t; changed = true; }
	}
	if (!changed) return;

	const int n = ReapplyUIStyles();
	// Deliberately not marking the scene dirty: nothing the user did changed,
	// and a style edit should not turn every open scene into unsaved work.
	// Saving later will of course write the new values, which is correct -
	// they are the values the scene now has.
	if (n > 0)
	{
		char buf[80];
		snprintf(buf, sizeof(buf), "UI style changed - restyled %d element(s)", n);
		echo(buf);
	}
}

int SceneEditor::ReapplyUIStyles()
{
	if (!scene) return 0;
	const std::string assetRoot = (project && project->IsOpen()) ? project->GetProjectPath() : std::string();
	std::string err;
	const int n = uistyle::ApplyToScene(scene, assetRoot, UIStylePalettePath(), err);
	if (!err.empty()) echo("WARNING: UI styles - " + err);
	if (n > 0)
	{
		char buf[80];
		snprintf(buf, sizeof(buf), "Applied UI styles to %d element(s)", n);
		echo(buf);
	}
	return n;
}

// Every .uistyle in the project, as project-relative paths. Scanned on
// demand rather than cached: there are a handful of them, the Properties
// panel only asks while its combo is open, and a cache would need
// invalidating on every file the user creates outside the editor.
std::vector<std::string> SceneEditor::ListUIStyles() const
{
	std::vector<std::string> out;
	if (!project || !project->IsOpen()) return out;
	namespace fs = std::filesystem;
	std::error_code ec;
	const std::string dir = project->AbsolutePath("assets/ui");
	if (!fs::exists(dir, ec)) return out;
	for (fs::directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec))
	{
		if (!it->is_regular_file(ec)) continue;
		if (it->path().extension().string() != ".uistyle") continue;
		out.push_back(project->RelativePath(it->path().string()));
	}
	std::sort(out.begin(), out.end());
	return out;
}

// Puts an element back under its style's control: drop the record of what
// was hand-edited, then re-apply ignoring overrides. The inverse of every
// tweak made since the style was applied, and nothing else.
bool SceneEditor::OpRevertUIStyle(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	UIRect* rect = NULL;
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
		{ rect = static_cast<UIRect*>(cs[i].get()); break; }
	if (!rect) { errOut = "'" + obj->GetName() + "' is not a UI element"; return false; }
	if (rect->GetStyleRef().empty()) { errOut = "'" + obj->GetName() + "' has no style to revert to"; return false; }

	const std::string abs = (project && project->IsOpen())
		? project->AbsolutePath(rect->GetStyleRef()) : rect->GetStyleRef();
	nlohmann::json style;
	if (!uistyle::ReadJsonFile(abs, style)) { errOut = "could not read " + rect->GetStyleRef(); return false; }
	nlohmann::json bag;
	const uistyle::Palette palette = uistyle::LoadPalette(UIStylePalettePath());
	if (!uistyle::Resolve(style, palette, bag, errOut)) return false;

	const json before = CaptureUIProperties(go);
	const std::vector<std::string> beforeOverrides = rect->GetStyleOverrides();
	rect->ClearStyleOverrides();
	const std::string assetRoot = (project && project->IsOpen()) ? project->GetProjectPath() : std::string();
	if (!uistyle::ApplyProperties(go, bag, assetRoot, errOut, false)) return false;
	const json after = CaptureUIProperties(go);

	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[this, goId, before, beforeOverrides]() {
			std::string e; RawSetUIProperties(goId, before, e); RawSetUIStyleOverrides(goId, beforeOverrides);
		},
		[this, goId, after]() {
			std::string e; RawSetUIProperties(goId, after, e); RawSetUIStyleOverrides(goId, std::vector<std::string>());
		},
		"Revert to UI Style"));
	MarkSceneDirty();
	return true;
}

void SceneEditor::RawSetUIStyleOverrides(uint32 goId, const std::vector<std::string>& keys)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) return;
	const std::vector<std::shared_ptr<IComponent> >& cs = ((GameObject*)obj->GetPTR())->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
		{ static_cast<UIRect*>(cs[i].get())->SetStyleOverrides(keys); return; }
}

bool SceneEditor::OpClearUIStyle(uint32 goId, std::string& errOut)
{
	SceneObject* obj = sceneObjects->GetSceneObject(goId);
	if (!obj || obj->GetType() != SceneObjectTypes::GAMEOBJECT) { errOut = "object not found"; return false; }
	GameObject* go = (GameObject*)obj->GetPTR();

	UIRect* rect = NULL;
	const std::vector<std::shared_ptr<IComponent> >& cs = go->GetComponents();
	for (size_t i = 0; i < cs.size(); i++)
		if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
		{ rect = static_cast<UIRect*>(cs[i].get()); break; }
	if (!rect) { errOut = "'" + obj->GetName() + "' is not a UI element"; return false; }
	if (rect->GetStyleRef().empty()) { errOut = "'" + obj->GetName() + "' has no style"; return false; }

	// Only the link goes. The values the style put there stay, which is what
	// "unlink" should mean - the element keeps the look it has and simply
	// stops following the file.
	const std::string before = rect->GetStyleRef();
	rect->SetStyleRef(std::string());
	sceneUndo.Push(std::make_unique<ApplyClosureCommand>(
		[this, goId, before]() { RawSetUIStyleRef(goId, before); },
		[this, goId]() { RawSetUIStyleRef(goId, std::string()); },
		"Clear UI Style"));
	MarkSceneDirty();
	return true;
}
