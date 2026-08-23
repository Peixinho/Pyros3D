// Unified UI/Agent edit chokepoint - see the undo/redo plan. Every Op*
// method here is the single place a given kind of scene edit happens,
// called both by interactive UI code (ShowProperties, DrawTreeNodeWidgets,
// AddFormSubmit, ...) and by the Agent* methods in SceneEditor.cpp, so the
// two front ends can no longer apply different fixups for the same edit.

#include "SceneEditor.h"
#include "SceneCommands.h"
#include <Pyros3D/Utils/Serialization/SceneSerializer.h>

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
#ifdef LUA_BINDINGS
	std::string snapshot = SceneSerializer::SerializeSubtree(go, scenePath, sharedLua);
#else
	std::string snapshot = SceneSerializer::SerializeSubtree(go, scenePath, NULL);
#endif
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
#ifdef LUA_BINDINGS
	std::string afterSnapshot = SceneSerializer::SerializeSubtree(go, scenePath, sharedLua);
#else
	std::string afterSnapshot = SceneSerializer::SerializeSubtree(go, scenePath, NULL);
#endif
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
#ifdef LUA_BINDINGS
	std::string snapshot = SceneSerializer::SerializeSubtree(go, scenePath, sharedLua);
#else
	std::string snapshot = SceneSerializer::SerializeSubtree(go, scenePath, NULL);
#endif

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
