#include "SceneObjects.h"
#include <ostream>
#include <vector>

#ifdef _WIN32 // Windows
	#include <Windows.h>
#else
	#include <limits.h>
	#include <unistd.h>
#endif

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
			std::shared_ptr<GameObject> go = std::make_shared<GameObject>();
			Scene->Add(go);

			SceneObject* obj = new SceneObject(Name, go.get(), id, SceneObjectTypes::GAMEOBJECT);
			listObjects[id] = obj;

			SetName(id, Name);

			return obj;
		}
		catch (const std::exception& e) {
			fprintf(stderr, "[ERROR] Failed to create GameObject: %s\n", e.what());
			return NULL;
		}
		catch (...) {
			fprintf(stderr, "[ERROR] Failed to create GameObject: Unknown error\n");
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
			default:
				// Particles, Lua, audio, vehicles: the engine round-trips
				// them, but this editor has no UI for them, so they stay
				// attached to their GameObject and simply aren't listed.
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

			// Collect the children before destroying any of them. The
			// recursive call erases from listObjects, invalidating any
			// iterator into it - the previous version reset the iterator to
			// begin() after each recursion and then let the for-loop's i++
			// run, so it skipped whatever had become the first element.
			// Deleting a parent with more than two children therefore left
			// orphaned SceneObjects behind, still listed in the scene tree
			// but pointing at components that had already been freed.
			std::vector<uint32> children;
			for (std::map<uint32,SceneObject*>::iterator i=listObjects.begin();i!=listObjects.end();i++)
				if ((*i).second != NULL && (*i).second->GetParentID()==id)
					children.push_back((*i).second->GetID());

			for (std::vector<uint32>::iterator c = children.begin(); c != children.end(); c++)
				DestroySceneObject(*c);
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
