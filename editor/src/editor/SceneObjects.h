#pragma once
#include<iostream>
#include<map>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Capsule.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/TorusKnot.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/Audio/AudioSource.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <memory>

#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#endif

namespace p3d { class Physics; }

using namespace p3d;

namespace SceneObjectTypes {
	enum {
		GAMEOBJECT = 0,
		RENDERING_COMPONENT,
		DIRECTIONALLIGHT_COMPONENT,
		POINTLIGHT_COMPONENT,
		SPOTLIGHT_COMPONENT,
		PHYSICS_COMPONENT,
		AUDIO_SOURCE_COMPONENT,
		LUA_COMPONENT,
		PARTICLE_SYSTEM_COMPONENT
	};
};

class SceneObject {
	friend class SceneObjects;
	friend class SceneEditor;
	
	protected:
		uint32 ID;
		// Observing pointer - the GameObject/component itself is owned by
		// the SceneGraph (GameObjects) or by its GameObject (components).
		void* PTR;
		// Owning, unlike PTR: helpers are added to the scene but the
		// SceneObject is what decides when one goes away.
		std::shared_ptr<GameObject> Helper;
		std::string Name;
		uint32 NameID;
		uint32 ParentID;
		uint32 Type;

	public:
		SceneObject(const std::string &Name, void* ptr, const uint32 id, const uint32 type = 0);
		void* GetPTR() { return PTR; }
		virtual ~SceneObject();
		const std::string &GetName() const { return Name; }
		const uint32 GetID() const { return ID; }
		Matrix ScaleTransform, LocalTransform, globalRotation;

		// The .prefab this object is an instance of, project-relative, or
		// empty. Editor-side state on purpose: a prefab is an authoring
		// concept, and the engine's GameObject has no business carrying one
		// (see shared/PrefabResolver.h). Only ever set on the root of an
		// instance - the objects below it belong to the prefab's subtree.
		std::string prefabSource;

		void SetParentID(const uint32 id) { ParentID = id; }
		const uint32 &GetParentID() const { return ParentID; }
		const uint32 &GetType() const;
};

class SceneObjects
{
	friend class SceneEditor;
	public:
		SceneObjects(SceneGraph* scene);
		virtual ~SceneObjects(void);

		// GameObjects
		SceneObject* CreateGameObject(const std::string &Name);

		// Registers a GameObject that already exists in the SceneGraph -
		// together with its components and children, recursively - into this
		// registry. SceneSerializer::LoadScene() builds real GameObjects
		// directly in the SceneGraph and knows nothing about the editor's
		// parallel id/name/parent bookkeeping, so a loaded scene is invisible
		// to the tree, selection and gizmos until it has been adopted.
		SceneObject* Adopt(GameObject* go, const uint32 parentID = 0);

		// Removes every registered object, leaving the registry empty. Used
		// by File > New and before loading over the current scene.
		void DestroyAll();
		void SetName(const uint32 id, const std::string &Name);
		void DestroySceneObject(const uint32 id = 0);
		SceneObject* GetSceneObject(const uint32 id);
		uint32 GetSceneObjectID(void* go);
		const std::map<uint32,SceneObject*> &GetList() const { return listObjects; }

		bool ReparentGameObject(const uint32 childId, const uint32 newParentId);
		bool MoveComponent(const uint32 compId, const uint32 targetGoId);
		bool IsDescendant(const uint32 ancestorId, const uint32 candidateId) const;

		// Deep-clones a GameObject (transform, components, child hierarchy).
		// When physicsEngine is non-null, physics components are recreated with
		// the same shape parameters rather than sharing rigid bodies.
		SceneObject* DuplicateGameObject(const uint32 id, Physics* physicsEngine = NULL);

		static std::shared_ptr<GameObject> FindSharedGameObject(SceneGraph* scene, GameObject* go);
		static std::shared_ptr<IComponent> FindSharedComponent(GameObject* owner, IComponent* comp);

		// Rendering Components
		SceneObject* CreateRenderingCube(GameObject *go,const f32 width, const f32 height, const f32 depth, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingSphere(GameObject *go, const f32 radius, const f32 segmentsw, const f32 segmentsh, bool smoothnormals, bool halfsphere, bool flipnormals);
		SceneObject* CreateRenderingCapsule(GameObject *go, const f32 radius, const f32 height, const f32 nrings, const f32 segmentsw, const f32 segmentsh, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingCone(GameObject *go, const f32 radius, const f32 height, const f32 segmentsw, const f32 segmentsh, bool openended, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingCylinder(GameObject *go, const f32 radius, const f32 height, const f32 segmentsw, const f32 segmentsh, bool openended, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingPlane(GameObject *go, const f32 width, const f32 height, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingTorus(GameObject *go, const f32 radius, const f32 tube, const f32 segmentsw, const f32 segmentsh, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingTorusKnot(GameObject *go, const f32 radius, const f32 tube, const f32 segmentsw, const f32 segmentsh, const f32 p, const f32 q, bool smoothnormals, bool flipnormals);
		SceneObject* CreateRenderingModel(GameObject *go, const std::string &path);
		SceneObject* CreateDirectionalLight(GameObject *go, const Vec3 &direction, const Vec4 &color);
		SceneObject* CreatePointLight(GameObject *go, const f32 radius, const Vec4 &color);
		SceneObject* CreateSpotLight(GameObject *go, const f32 radius, const Vec3 &direction, const f32 outter, const f32 inner, const Vec4 &color);
		SceneObject* CreateAudioSource(GameObject *go, const std::string &path, bool stream = false,
			bool looping = false, bool spatialized = true, f32 volume = 1.f);
		SceneObject* CreateParticleSystem(GameObject *go, const ParticleSystemDesc &desc);
#ifdef LUA_BINDINGS
		// Registers an already-built LuaComponent (from LuaComponent_FromFile /
		// load) as a tree node under `go`.
		SceneObject* CreateLuaComponent(GameObject *go, const std::shared_ptr<LuaComponent> &comp);
#endif

	protected:
		std::map<uint32,SceneObject*> listObjects;
		SceneGraph* Scene;
		std::shared_ptr<GenericShaderMaterial> GenericMaterial;
		SceneGraph* SceneHelpers;

	private:
		SceneObject* DuplicateGameObjectUnder(const uint32 id, const uint32 newParentId, Physics* physicsEngine);
		uint32 _ID;
};
