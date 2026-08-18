#include "SceneObjects.h"
#include "Helpers/IHelper.h"
#include "Helpers/LightHelper.h"
#include "Helpers/GameObjectHelper.h"
#include "Helpers/SoundHelper.h"
#include "Helpers/ParticleHelper.h"
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Physics/Components/Box/PhysicsBox.h>
#include <Pyros3D/Physics/Components/Sphere/PhysicsSphere.h>
#include <Pyros3D/Physics/Components/Capsule/PhysicsCapsule.h>
#include <Pyros3D/Physics/Components/Cone/PhysicsCone.h>
#include <Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.h>
#include <Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <filesystem>
#include <ostream>
#include <vector>

#ifdef _WIN32 // Windows
	#include <Windows.h>
#else
	#include <limits.h>
	#include <unistd.h>
#endif

namespace {
	std::string ScriptStemName(const std::string& path)
	{
		if (path.empty()) return "Script";
		std::string stem = std::filesystem::path(path).stem().string();
		return stem.empty() ? "Script" : stem;
	}
}
	SceneObjects::SceneObjects(SceneGraph* scene)
	{
		Scene = scene;
		_ID = 0;
		// Shadow variants are back: IRenderer now assigns the shadow samplers
		// distinct fallback texture units when the scene has no shadow casters,
		// so they no longer collide on unit 0 and fail every draw.
		GenericMaterial = std::make_shared<GenericShaderMaterial>(ShaderUsage::Color | ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow | ShaderUsage::SpotShadow);
		GenericMaterial->SetColor(Vec4(1,1,1,1));
	}

	SceneObjects::~SceneObjects(void)
	{

	}

    SceneObject* SceneObjects::GetSceneObject(const uint32 id)
    {
        if (id == 0) return NULL;
        std::map<uint32, SceneObject*>::iterator it = listObjects.find(id);
        if (it == listObjects.end()) return NULL;
        return it->second;
    }
	SceneObject::SceneObject(const std::string &Name, void* ptr, const uint32 id, const uint32 type)
	{
		std::ostringstream name; name << Name.c_str();
		this->Name = name.str();
		this->PTR = ptr;
		this->NameID = MakeStringID(name.str());
		this->ParentID = 0;
		this->ID = id;
		this->Type = type;

    // Initialize transforms to sane defaults
    LocalTransform.identity();
    ScaleTransform.identity();
    globalRotation.identity();
    
    // Ensure ScaleTransform has non-zero scale values to avoid division by zero issues
    ScaleTransform.ForceScale(1.0f, 1.0f, 1.0f);
	}
    SceneObject::~SceneObject()
    {
        // Do not delete PTR here; ownership is managed elsewhere
        PTR = NULL;
    }

	const uint32 &SceneObject::GetType() const
	{
		return Type;
	}

	SceneObject* SceneObjects::CreateGameObject(const std::string &Name)
	{
		try {
			uint32 id = ++_ID;
#ifdef LUA_BINDINGS
			// LUA_GameObject so script init(owner) sol usertype resolution works.
			std::shared_ptr<GameObject> go = std::static_pointer_cast<GameObject>(std::make_shared<LUA_GameObject>());
#else
			std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
#endif
			Scene->Add(go);

			SceneObject* obj = new SceneObject(Name, go.get(), id, SceneObjectTypes::GAMEOBJECT);
			listObjects[id] = obj;

			SetName(id, Name);

			return obj;
		}
		catch (const std::exception& e) {
			echo(std::string("ERROR: Failed to create GameObject: ") + e.what());
			return NULL;
		}
		catch (...) {
			echo("ERROR: Failed to create GameObject: Unknown error");
			return NULL;
		}
	}
	SceneObject* SceneObjects::Adopt(GameObject* go, const uint32 parentID)
	{
		if (go == NULL) return NULL;

		uint32 id = ++_ID;
		std::string name = go->GetName().size() > 0 ? go->GetName() : std::string("GameObject");
		SceneObject* obj = new SceneObject(name, go, id, SceneObjectTypes::GAMEOBJECT);
		listObjects[id] = obj;
		obj->SetParentID(parentID);
		SetName(id, name);

		// Components become their own registry entries parented to the
		// GameObject, exactly as the Create* methods above arrange them.
		const std::vector<std::shared_ptr<IComponent>> &components = go->GetComponents();
		for (std::vector<std::shared_ptr<IComponent>>::const_iterator c = components.begin(); c != components.end(); c++)
		{
			uint32 type;
			std::string cname;
			switch ((*c)->GetComponentType())
			{
			case ComponentType::RenderingComponent:  type = SceneObjectTypes::RENDERING_COMPONENT;      cname = "Mesh";              break;
			case ComponentType::DirectionalLight:    type = SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT; cname = "Directional Light"; break;
			case ComponentType::PointLight:          type = SceneObjectTypes::POINTLIGHT_COMPONENT;     cname = "Point Light";       break;
			case ComponentType::SpotLight:           type = SceneObjectTypes::SPOTLIGHT_COMPONENT;      cname = "Spot Light";        break;
			case ComponentType::Physics:             type = SceneObjectTypes::PHYSICS_COMPONENT;        cname = "Physics";           break;
			case ComponentType::AudioSource:         type = SceneObjectTypes::AUDIO_SOURCE_COMPONENT;  cname = "Sound";             break;
			case ComponentType::ParticleSystem:      type = SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT; cname = "Particles";     break;
#ifdef LUA_BINDINGS
			case ComponentType::LuaComponent:
			{
				type = SceneObjectTypes::LUA_COMPONENT;
				LuaComponent* lc = dynamic_cast<LuaComponent*>((*c).get());
				cname = lc ? ScriptStemName(lc->scriptFile) : "Script";
				break;
			}
#endif
			default:
				// Vehicles: the engine round-trips them, but this editor has
				// no UI for them, so they stay attached to their GameObject
				// and simply aren't listed.
				continue;
			}
			uint32 cid = ++_ID;
			SceneObject* cobj = new SceneObject(cname, (*c).get(), cid, type);
			listObjects[cid] = cobj;
			cobj->SetParentID(id);
			SetName(cid, cname);
		}

		const std::vector<std::shared_ptr<GameObject>> &children = go->GetChildren();
		for (std::vector<std::shared_ptr<GameObject>>::const_iterator k = children.begin(); k != children.end(); k++)
			Adopt((*k).get(), id);

		return obj;
	}

	void SceneObjects::DestroyAll()
	{
		// Roots first - DestroySceneObject() recurses into children, so
		// collect the ids up front rather than iterating a shrinking map.
		std::vector<uint32> roots;
		for (std::map<uint32, SceneObject*>::iterator i = listObjects.begin(); i != listObjects.end(); i++)
			if ((*i).second != NULL && (*i).second->GetParentID() == 0)
				roots.push_back((*i).second->GetID());

		for (std::vector<uint32>::iterator i = roots.begin(); i != roots.end(); i++)
			DestroySceneObject(*i);

		// Anything left was parented to something already gone.
		for (std::map<uint32, SceneObject*>::iterator i = listObjects.begin(); i != listObjects.end(); i++)
			delete (*i).second;
		listObjects.clear();
		_ID = 0;
	}

	void SceneObjects::SetName(const uint32 id, const std::string &Name)
	{
		SceneObject* obj = listObjects[id];
		uint32 nameID = MakeStringID(Name);
		std::string name = Name;

		bool found = true;
		std::ostringstream append;
		do
		{
			uint32 count = 1;
			for (std::map<uint32,SceneObject*>::iterator i=listObjects.begin();i!=listObjects.end();i++)
			{
				if ((*i).second->NameID==nameID && id!=(*i).second->GetID())
				{
					append.str("");
					append << "(" << count << ")";
					nameID = MakeStringID(name + append.str());
					i=listObjects.begin();
					count++;
				}
			}
			found = false;
		}while(found);

			obj->Name = name + append.str();
	obj->NameID = MakeStringID(obj->Name);

	// Push the resolved name onto the engine object too. Names used to live
	// only in this registry, which is not what gets serialized - so every
	// saved scene stored name:"" and everything came back as "GameObject",
	// "GameObject(1)", ... on load.
	if (obj->GetType() == SceneObjectTypes::GAMEOBJECT && obj->GetPTR() != NULL)
		((GameObject*)obj->GetPTR())->SetName(obj->Name);
	// Don't overwrite the type - it should already be set correctly
	}

	SceneObject* SceneObjects::CreateRenderingCube(GameObject *go, const f32 width, const f32 height, const f32 depth, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rCube;
		// Mesh
		std::shared_ptr<Renderable> cubeMesh;
		cubeMesh = std::make_shared<Cube>(width,height,depth,smoothnormals,flipnormals);
		rCube = std::make_shared<RenderingComponent>(cubeMesh, GenericMaterial);
		go->Add(rCube);

		SceneObject* obj = new SceneObject("Cube", rCube.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingSphere(GameObject *go, const f32 radius, const f32 segmentsw, const f32 segmentsh, bool smoothnormals, bool halfsphere, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rSphere;
		// Mesh
		std::shared_ptr<Renderable> sphereMesh;
		sphereMesh = std::make_shared<Sphere>(radius, segmentsw, segmentsh, smoothnormals, halfsphere, flipnormals);
		rSphere = std::make_shared<RenderingComponent>(sphereMesh, GenericMaterial);
		go->Add(rSphere);

		SceneObject* obj = new SceneObject("Sphere", rSphere.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingCapsule(GameObject *go, const f32 radius, const f32 height, const f32 nrings, const f32 segmentsw, const f32 segmentsh, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rCapsule;
		// Mesh
		std::shared_ptr<Renderable> capsuleMesh;
		capsuleMesh = std::make_shared<Capsule>(radius, height, nrings, segmentsw, segmentsh, smoothnormals, flipnormals);
		rCapsule = std::make_shared<RenderingComponent>(capsuleMesh, GenericMaterial);
		go->Add(rCapsule);

		SceneObject* obj = new SceneObject("Capsule", rCapsule.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingCone(GameObject *go, const f32 radius, const f32 height, const f32 segmentsw, const f32 segmentsh, bool openended, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rCone;
		// Mesh
		std::shared_ptr<Renderable> coneMesh;
		coneMesh = std::make_shared<Cone>(radius, height, segmentsw, segmentsh, openended, smoothnormals, flipnormals);
		rCone = std::make_shared<RenderingComponent>(coneMesh, GenericMaterial);
		go->Add(rCone);

		SceneObject* obj = new SceneObject("Cone", rCone.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingCylinder(GameObject *go, const f32 radius, const f32 height, const f32 segmentsw, const f32 segmentsh, bool openended, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rCylinder;
		// Mesh
		std::shared_ptr<Renderable> cylinderMesh;
		cylinderMesh = std::make_shared<Cylinder>(radius, height, segmentsw, segmentsh, openended, smoothnormals, flipnormals);
		rCylinder = std::make_shared<RenderingComponent>(cylinderMesh, GenericMaterial);
		go->Add(rCylinder);

		SceneObject* obj = new SceneObject("Cylinder", rCylinder.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingPlane(GameObject *go, const f32 width, const f32 height, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rPlane;
		// Mesh
		std::shared_ptr<Renderable> planeMesh;
		planeMesh = std::make_shared<Plane>(width, height, smoothnormals, flipnormals);
		rPlane = std::make_shared<RenderingComponent>(planeMesh, GenericMaterial);
		go->Add(rPlane);

		SceneObject* obj = new SceneObject("Plane", rPlane.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingTorus(GameObject *go, const f32 radius, const f32 tube, const f32 segmentsw, const f32 segmentsh, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rTorus;
		// Mesh
		std::shared_ptr<Renderable> torusMesh;
		torusMesh = std::make_shared<Torus>(radius, tube, segmentsw, segmentsh, smoothnormals, flipnormals);
		rTorus = std::make_shared<RenderingComponent>(torusMesh, GenericMaterial);
		go->Add(rTorus);

		SceneObject* obj = new SceneObject("Torus", rTorus.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateRenderingTorusKnot(GameObject *go, const f32 radius, const f32 tube, const f32 segmentsw, const f32 segmentsh, const f32 p, const f32 q, bool smoothnormals, bool flipnormals)
	{
		uint32 id = ++_ID;
		std::shared_ptr<RenderingComponent> rTorusKnot;
		// Mesh
		std::shared_ptr<Renderable> torusKnotMesh;
		torusKnotMesh = std::make_shared<TorusKnot>(radius, tube, segmentsw, segmentsh, p, q, smoothnormals, flipnormals);
		rTorusKnot = std::make_shared<RenderingComponent>(torusKnotMesh, GenericMaterial);
		go->Add(rTorusKnot);

		SceneObject* obj = new SceneObject("TorusKnot", rTorusKnot.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::RENDERING_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}

	SceneObject* SceneObjects::CreateRenderingModel(GameObject *go, const std::string &path)
	{
		uint32 id = ++_ID;
		std::string finalPath = path;
		/*if (path.substr(path.size() - 5, path.size()).compare(".p3dm") != 0)
		{
			std::string workingPath = ExePath();
			// std::string modelName = workingPath + std::string("\\temp") + path.substr(path.rfind("//"), path.size()); // Save on Temp Folder
			std::string modelName = path.substr(0, path.rfind("//")) + std::string("//") + path.substr(path.rfind("//"), path.size());
			modelName = modelName.substr(0, modelName.rfind("."));
			ostringstream command;
			#ifdef _WIN32
				command << workingPath << "\\tools\\PyrosConvertTool.exe --model " << path << " " << modelName;
			#else
				command << "cd " << "tools/ && ./PyrosConvertTool --model " << path << " " << modelName;
			#endif
			system(command.str().c_str());
			finalPath = modelName + std::string(".p3dm");
		}*/
		
		// Mesh
		std::shared_ptr<Renderable> modelMesh = std::make_shared<Model>(finalPath, true);
		std::shared_ptr<RenderingComponent> rModel = std::make_shared<RenderingComponent>(modelMesh, ShaderUsage::Diffuse | ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow | ShaderUsage::SpotShadow);
		go->Add(rModel);

		SceneObject* obj = new SceneObject("Model", rModel.get(), id, SceneObjectTypes::RENDERING_COMPONENT);
		listObjects[id] = obj;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateDirectionalLight(GameObject *go, const Vec3 &direction, const Vec4 &color)
	{
		uint32 id = ++_ID;
		std::shared_ptr<DirectionalLight> light = std::make_shared<DirectionalLight>(color, direction);
		
		go->Add(light);

		SceneObject* obj = new SceneObject("Directional Light", light.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::DIRECTIONALLIGHT_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreatePointLight(GameObject *go, const f32 radius, const Vec4 &color)
	{
		uint32 id = ++_ID;
		std::shared_ptr<PointLight> light = std::make_shared<PointLight>(color, radius);
		
		go->Add(light);

		SceneObject* obj = new SceneObject("Point Light", light.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::POINTLIGHT_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}
	SceneObject* SceneObjects::CreateSpotLight(GameObject *go, const f32 radius, const Vec3 &direction, const f32 outter, const f32 inner, const Vec4 &color)
	{
		uint32 id = ++_ID;
		std::shared_ptr<SpotLight> light = std::make_shared<SpotLight>(color, radius, direction, outter, inner);
		
		go->Add(light);

		SceneObject* obj = new SceneObject("Spot Light", light.get(), id);
		listObjects[id] = obj;
		obj->Type = SceneObjectTypes::SPOTLIGHT_COMPONENT;

		obj->SetParentID(GetSceneObjectID(go));

		return obj;
	}

	SceneObject* SceneObjects::CreateAudioSource(GameObject *go, const std::string &path, bool stream,
		bool looping, bool spatialized, f32 volume)
	{
		uint32 id = ++_ID;
		std::shared_ptr<AudioSource> source = std::make_shared<AudioSource>(path, stream);
		source->SetLooping(looping);
		source->SetSpatialization(spatialized);
		source->SetVolume(volume);
		source->EnsureLoaded();
		go->Add(source);

		SceneObject* obj = new SceneObject("Sound", source.get(), id, SceneObjectTypes::AUDIO_SOURCE_COMPONENT);
		listObjects[id] = obj;
		obj->SetParentID(GetSceneObjectID(go));
		SetName(id, "Sound");
		return obj;
	}

	SceneObject* SceneObjects::CreateParticleSystem(GameObject *go, const ParticleSystemDesc &desc)
	{
		if (go == NULL) return NULL;

		uint32 id = ++_ID;
		std::shared_ptr<ParticleSystem> particles = std::make_shared<ParticleSystem>(desc);
		go->Add(particles);

		SceneObject* obj = new SceneObject("Particles", particles.get(), id, SceneObjectTypes::PARTICLE_SYSTEM_COMPONENT);
		listObjects[id] = obj;
		obj->SetParentID(GetSceneObjectID(go));
		SetName(id, "Particles");
		return obj;
	}

#ifdef LUA_BINDINGS
	SceneObject* SceneObjects::CreateLuaComponent(GameObject *go, const std::shared_ptr<LuaComponent> &comp)
	{
		if (!go || !comp) return NULL;
		go->Add(comp);
		uint32 id = ++_ID;
		const std::string cname = ScriptStemName(comp->scriptFile);
		SceneObject* obj = new SceneObject(cname, comp.get(), id, SceneObjectTypes::LUA_COMPONENT);
		listObjects[id] = obj;
		obj->SetParentID(GetSceneObjectID(go));
		SetName(id, cname);
		return obj;
	}
#endif
	
	void SceneObjects::DestroySceneObject(const uint32 id)
	{
		uint32 idToRemove = id;
		if (idToRemove>0)
		{
			// find(), not operator[] - the latter default-inserts a NULL
			// SceneObject* for an id that is not in the map and then
			// dereferences it.
			std::map<uint32, SceneObject*>::iterator entry = listObjects.find(idToRemove);
			if (entry == listObjects.end() || entry->second == NULL) return;

			// Children (components) first while the owning GameObject is still
			// alive. Destroying the GO via Scene->Remove() first freed it and
			// left child DestroySceneObject() calling Remove() on a dead
			// owner ("Component Not Found" / Vulkan use-after-free).
			std::vector<uint32> children;
			for (std::map<uint32,SceneObject*>::iterator i=listObjects.begin();i!=listObjects.end();i++)
				if ((*i).second != NULL && (*i).second->GetParentID()==id)
					children.push_back((*i).second->GetID());

			for (std::vector<uint32>::iterator c = children.begin(); c != children.end(); c++)
				DestroySceneObject(*c);

			entry = listObjects.find(idToRemove);
			if (entry == listObjects.end() || entry->second == NULL) return;

			if (listObjects[idToRemove]->GetType()==SceneObjectTypes::GAMEOBJECT)
				Scene->Remove((GameObject*)listObjects[idToRemove]->GetPTR());

			else
			{
				// Every other SceneObject type is a component: detaching it
				// from its GameObject drops the last reference, which frees
				// the component and (with it) its Renderable. Deleting the
				// Renderable by hand here - as this used to - is a double
				// free now that the component owns it through a shared_ptr.
				IComponent* component = (IComponent*)listObjects[idToRemove]->GetPTR();
				if (component != NULL && component->GetOwner() != NULL)
					component->GetOwner()->Remove(component);
			}

			// Remove Helper if Exists
			if (listObjects[idToRemove]->Helper)
			{
				Scene->Remove(listObjects[idToRemove]->Helper);
				listObjects[idToRemove]->Helper.reset();
			}
			delete listObjects[idToRemove];
			listObjects.erase(idToRemove);
		}
	}
    uint32 SceneObjects::GetSceneObjectID(void* go)
    {
        for (std::map<uint32,SceneObject*>::iterator i=listObjects.begin();i!=listObjects.end();i++)
        {
            if ((*i).second->GetPTR() == go) return (*i).second->GetID();
        }
        return 0;
    }

	std::shared_ptr<GameObject> SceneObjects::FindSharedGameObject(SceneGraph* scene, GameObject* go)
	{
		if (!scene || !go) return std::shared_ptr<GameObject>();
		std::vector<std::shared_ptr<GameObject>>& all = scene->GetAllGameObjectList();
		for (std::vector<std::shared_ptr<GameObject>>::iterator i = all.begin(); i != all.end(); ++i)
			if ((*i).get() == go) return *i;
		return std::shared_ptr<GameObject>();
	}

	std::shared_ptr<IComponent> SceneObjects::FindSharedComponent(GameObject* owner, IComponent* comp)
	{
		if (!owner || !comp) return std::shared_ptr<IComponent>();
		const std::vector<std::shared_ptr<IComponent>>& comps = owner->GetComponents();
		for (std::vector<std::shared_ptr<IComponent>>::const_iterator i = comps.begin(); i != comps.end(); ++i)
			if ((*i).get() == comp) return *i;
		return std::shared_ptr<IComponent>();
	}

	bool SceneObjects::IsDescendant(const uint32 ancestorId, const uint32 candidateId) const
	{
		uint32 id = candidateId;
		while (id != 0)
		{
			if (id == ancestorId) return true;
			std::map<uint32, SceneObject*>::const_iterator it = listObjects.find(id);
			if (it == listObjects.end() || it->second == NULL) return false;
			id = it->second->GetParentID();
		}
		return false;
	}

	bool SceneObjects::ReparentGameObject(const uint32 childId, const uint32 newParentId)
	{
		if (childId == 0 || childId == newParentId) return false;
		if (newParentId != 0 && IsDescendant(childId, newParentId)) return false;

		SceneObject* childObj = GetSceneObject(childId);
		if (!childObj || childObj->GetType() != SceneObjectTypes::GAMEOBJECT) return false;

		GameObject* child = (GameObject*)childObj->GetPTR();
		std::shared_ptr<GameObject> childPtr = FindSharedGameObject(Scene, child);
		if (!childPtr) return false;

		if (child->HaveParent())
			child->GetParent()->Remove(child);

		if (newParentId != 0)
		{
			SceneObject* parentObj = GetSceneObject(newParentId);
			if (!parentObj || parentObj->GetType() != SceneObjectTypes::GAMEOBJECT) return false;
			GameObject* newParent = (GameObject*)parentObj->GetPTR();
			std::shared_ptr<GameObject> parentPtr = FindSharedGameObject(Scene, newParent);
			if (!parentPtr) return false;
			parentPtr->Add(childPtr);
			childObj->SetParentID(newParentId);
		}
		else
		{
			childObj->SetParentID(0);
			bool inScene = false;
			std::vector<std::shared_ptr<GameObject>>& all = Scene->GetAllGameObjectList();
			for (std::vector<std::shared_ptr<GameObject>>::iterator i = all.begin(); i != all.end(); ++i)
			{
				if ((*i).get() == child)
				{
					inScene = true;
					break;
				}
			}
			if (!inScene)
				Scene->Add(childPtr);
		}
		return true;
	}

	bool SceneObjects::MoveComponent(const uint32 compId, const uint32 targetGoId)
	{
		SceneObject* compObj = GetSceneObject(compId);
		SceneObject* targetObj = GetSceneObject(targetGoId);
		if (!compObj || !targetObj || targetObj->GetType() != SceneObjectTypes::GAMEOBJECT) return false;
		if (compObj->GetType() == SceneObjectTypes::GAMEOBJECT) return false;

		IComponent* comp = (IComponent*)compObj->GetPTR();
		GameObject* target = (GameObject*)targetObj->GetPTR();
		if (!comp || !target) return false;
		GameObject* owner = comp->GetOwner();
		if (!owner || owner == target) return false;

		std::shared_ptr<IComponent> compPtr = FindSharedComponent(owner, comp);
		if (!compPtr) return false;

		owner->Remove(compPtr);
		target->Add(compPtr);
		compObj->SetParentID(targetGoId);

		if (compObj->Helper)
			((IHelper*)compObj->Helper.get())->owner = target;

		return true;
	 }

	SceneObject* SceneObjects::DuplicateGameObject(const uint32 id, Physics* physicsEngine)
	{
		SceneObject* src = GetSceneObject(id);
		if (!src || src->GetType() != SceneObjectTypes::GAMEOBJECT) return NULL;
		return DuplicateGameObjectUnder(id, src->GetParentID(), physicsEngine);
	}

	SceneObject* SceneObjects::DuplicateGameObjectUnder(const uint32 id, const uint32 newParentId, Physics* physicsEngine)
	{
		SceneObject* srcObj = GetSceneObject(id);
		if (!srcObj || srcObj->GetType() != SceneObjectTypes::GAMEOBJECT) return NULL;
		GameObject* srcGo = (GameObject*)srcObj->GetPTR();
		if (!srcGo) return NULL;

		SceneObject* dupObj = CreateGameObject(srcObj->GetName() + " Copy");
		if (!dupObj) return NULL;
		GameObject* dupGo = (GameObject*)dupObj->GetPTR();

		dupGo->SetPosition(srcGo->GetPosition());
		dupGo->SetRotation(srcGo->GetRotation());
		dupGo->SetScale(srcGo->GetScale());
		dupObj->LocalTransform = srcObj->LocalTransform;
		dupObj->ScaleTransform = srcObj->ScaleTransform;
		dupObj->globalRotation = srcObj->globalRotation;

		const std::map<uint32, std::string>& tags = srcGo->GetTags();
		for (std::map<uint32, std::string>::const_iterator t = tags.begin(); t != tags.end(); ++t)
			dupGo->AddTag(t->second);

		if (newParentId != 0)
			ReparentGameObject(dupObj->GetID(), newParentId);

		const std::vector<std::shared_ptr<IComponent>>& comps = srcGo->GetComponents();
		for (std::vector<std::shared_ptr<IComponent>>::const_iterator c = comps.begin(); c != comps.end(); ++c)
		{
			switch ((*c)->GetComponentType())
			{
			case ComponentType::RenderingComponent:
			{
				RenderingComponent* srcRc = (RenderingComponent*)(*c).get();
				std::shared_ptr<IMaterial> mat = GenericMaterial;
				std::vector<RenderingMesh*>& meshes = srcRc->GetMeshes();
				if (!meshes.empty() && meshes[0]->Material)
					mat = meshes[0]->Material;
				std::shared_ptr<RenderingComponent> newRc = std::make_shared<RenderingComponent>(
					srcRc->GetRenderableShared(), mat);
				dupGo->Add(newRc);
				uint32 cid = ++_ID;
				SceneObject* cobj = new SceneObject("Mesh", newRc.get(), cid, SceneObjectTypes::RENDERING_COMPONENT);
				listObjects[cid] = cobj;
				cobj->SetParentID(dupObj->GetID());
				SetName(cid, "Mesh");
				break;
			}
			case ComponentType::DirectionalLight:
			{
				DirectionalLight* srcLight = (DirectionalLight*)(*c).get();
				SceneObject* lightObj = CreateDirectionalLight(dupGo, srcLight->GetLightDirection(), srcLight->GetLightColor());
				std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(dupGo);
				lightObj->Helper = h;
				Scene->Add(h);
				break;
			}
			case ComponentType::PointLight:
			{
				PointLight* srcLight = (PointLight*)(*c).get();
				SceneObject* lightObj = CreatePointLight(dupGo, srcLight->GetLightRadius(), srcLight->GetLightColor());
				std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(dupGo);
				lightObj->Helper = h;
				Scene->Add(h);
				break;
			}
			case ComponentType::SpotLight:
			{
				SpotLight* srcLight = (SpotLight*)(*c).get();
				SceneObject* lightObj = CreateSpotLight(dupGo, srcLight->GetLightRadius(), srcLight->GetLightDirection(),
					srcLight->GetLightOutterCone(), srcLight->GetLightInnerCone(), srcLight->GetLightColor());
				std::shared_ptr<LightHelper> h = std::make_shared<LightHelper>(dupGo);
				lightObj->Helper = h;
				Scene->Add(h);
				break;
			}
			case ComponentType::AudioSource:
			{
				AudioSource* srcAudio = (AudioSource*)(*c).get();
				SceneObject* soundObj = CreateAudioSource(dupGo, srcAudio->GetFile(), srcAudio->IsStreamed(),
					srcAudio->IsLooping(), srcAudio->IsSpatialized(), srcAudio->GetVolume());
				if (soundObj)
				{
					AudioSource* dst = (AudioSource*)soundObj->GetPTR();
					dst->SetPitch(srcAudio->GetPitch());
					dst->SetPan(srcAudio->GetPan());
					dst->SetAttenuation(srcAudio->GetAttenuationModel(), srcAudio->GetMinDistance(), srcAudio->GetMaxDistance());
					dst->SetDirectionalAttenuation(srcAudio->GetDirectionalAttenuation());
					dst->SetDopplerFactor(srcAudio->GetDopplerFactor());
					if (srcAudio->HasCone())
						dst->SetCone(srcAudio->GetConeInnerAngle(), srcAudio->GetConeOuterAngle(), srcAudio->GetConeOuterGain());
					if (srcAudio->GetFilterType() != AudioFilterType::None)
						dst->SetFilter(srcAudio->GetFilterType(), srcAudio->GetFilterCutoff(), srcAudio->GetFilterOrder());
					if (srcAudio->GetEQType() != AudioEQType::None)
						dst->SetEQ(srcAudio->GetEQType(), srcAudio->GetEQFrequency(), srcAudio->GetEQGain(), srcAudio->GetEQQ());
					if (srcAudio->HasDelay())
						dst->SetDelay(srcAudio->GetDelaySeconds(), srcAudio->GetDelayDecay(), srcAudio->GetDelayWet(), srcAudio->GetDelayDry());
					std::shared_ptr<SoundHelper> h = std::make_shared<SoundHelper>(dupGo);
					soundObj->Helper = h;
					Scene->Add(h);
				}
				break;
			}
			case ComponentType::ParticleSystem:
			{
				// GetDesc() is the whole configuration (that is what it
				// exists for - see ParticleSystem.h), so the copy needs
				// nothing beyond it. The live particles are deliberately not
				// copied: the duplicate starts its own emission.
				ParticleSystem* srcPs = (ParticleSystem*)(*c).get();
				SceneObject* psObj = CreateParticleSystem(dupGo, srcPs->GetDesc());
				if (psObj)
				{
					if (!srcPs->IsPlaying())
						((ParticleSystem*)psObj->GetPTR())->Stop();
					std::shared_ptr<ParticleHelper> h = std::make_shared<ParticleHelper>(dupGo);
					psObj->Helper = h;
					Scene->Add(h);
				}
				break;
			}
#ifdef LUA_BINDINGS
			case ComponentType::LuaComponent:
			{
				LuaComponent* srcLc = (LuaComponent*)(*c).get();
				if (!srcLc || srcLc->scriptFile.empty() || !srcLc->data.valid()) break;
				sol::state_view lua(srcLc->data.lua_state());
				std::shared_ptr<LuaComponent> newLc = LuaComponent_FromFile(lua, srcLc->scriptFile);
				if (newLc)
					CreateLuaComponent(dupGo, newLc);
				break;
			}
#endif
			case ComponentType::Physics:
			{
				if (!physicsEngine) break;
				IPhysicsComponent* srcPhys = (IPhysicsComponent*)(*c).get();
				std::shared_ptr<IPhysicsComponent> newPhys;
				const char* physName = "Physics";
				switch (srcPhys->GetShape())
				{
				case CollisionShapes::Box:
				{
					PhysicsBox* b = (PhysicsBox*)srcPhys;
					newPhys = physicsEngine->CreateBox(b->GetWidth(), b->GetHeight(), b->GetDepth(), b->GetMass(), b->IsGhost());
					physName = "Physics Box";
					break;
				}
				case CollisionShapes::Sphere:
				{
					PhysicsSphere* s = (PhysicsSphere*)srcPhys;
					newPhys = physicsEngine->CreateSphere(s->GetRadius(), s->GetMass(), s->IsGhost());
					physName = "Physics Sphere";
					break;
				}
				case CollisionShapes::Capsule:
				{
					PhysicsCapsule* cap = (PhysicsCapsule*)srcPhys;
					newPhys = physicsEngine->CreateCapsule(cap->GetRadius(), cap->GetHeight(), cap->GetMass(), cap->IsGhost());
					physName = "Physics Capsule";
					break;
				}
				case CollisionShapes::Cone:
				{
					PhysicsCone* cone = (PhysicsCone*)srcPhys;
					newPhys = physicsEngine->CreateCone(cone->GetRadius(), cone->GetHeight(), cone->GetMass(), cone->IsGhost());
					physName = "Physics Cone";
					break;
				}
				case CollisionShapes::Cylinder:
				{
					PhysicsCylinder* cyl = (PhysicsCylinder*)srcPhys;
					newPhys = physicsEngine->CreateCylinder(cyl->GetRadius(), cyl->GetHeight(), cyl->GetMass(), cyl->IsGhost());
					physName = "Physics Cylinder";
					break;
				}
				case CollisionShapes::StaticPlane:
				{
					PhysicsStaticPlane* plane = (PhysicsStaticPlane*)srcPhys;
					newPhys = physicsEngine->CreateStaticPlane(plane->GetNormal(), plane->GetConstant(), plane->GetMass(), plane->IsGhost());
					physName = "Physics Static Plane";
					break;
				}
				default:
					break;
				}
				if (newPhys)
				{
					dupGo->Add(newPhys);
					uint32 cid = ++_ID;
					SceneObject* cobj = new SceneObject(physName, newPhys.get(), cid, SceneObjectTypes::PHYSICS_COMPONENT);
					listObjects[cid] = cobj;
					cobj->SetParentID(dupObj->GetID());
					SetName(cid, physName);
				}
				break;
			}
			default:
				break;
			}
		}

		if (srcObj->Helper)
		{
			std::shared_ptr<GameObjectHelper> h = std::make_shared<GameObjectHelper>(dupGo);
			dupObj->Helper = h;
			Scene->Add(h);
		}

		std::vector<uint32> childIds;
		for (std::map<uint32, SceneObject*>::iterator i = listObjects.begin(); i != listObjects.end(); ++i)
		{
			if (i->second != NULL && i->second->GetParentID() == id && i->second->GetType() == SceneObjectTypes::GAMEOBJECT)
				childIds.push_back(i->second->GetID());
		}
		for (std::vector<uint32>::iterator ci = childIds.begin(); ci != childIds.end(); ++ci)
			DuplicateGameObjectUnder(*ci, dupObj->GetID(), physicsEngine);

		return dupObj;
	}
