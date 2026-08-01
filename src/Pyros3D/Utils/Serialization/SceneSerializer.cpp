//============================================================================
// Name        : SceneSerializer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Scene save/load to JSON
//============================================================================

#include <Pyros3D/Utils/Serialization/SceneSerializer.h>
#include <Pyros3D/Utils/Json/json.hpp>
#include <Pyros3D/Core/Logs/Log.h>

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>

#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Assets/Texture/Texture.h>

#include <Pyros3D/Assets/Renderable/Primitives/Primitive.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Capsule.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/TorusKnot.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <Pyros3D/Physics/Components/Box/PhysicsBox.h>
#include <Pyros3D/Physics/Components/Sphere/PhysicsSphere.h>
#include <Pyros3D/Physics/Components/Capsule/PhysicsCapsule.h>
#include <Pyros3D/Physics/Components/Cone/PhysicsCone.h>
#include <Pyros3D/Physics/Components/Cylinder/PhysicsCylinder.h>
#include <Pyros3D/Physics/Components/StaticPlane/PhysicsStaticPlane.h>
#include <Pyros3D/Physics/Components/ConvexHull/PhysicsConvexHull.h>
#include <Pyros3D/Physics/Components/ConvexTriangleMesh/PhysicsConvexTriangleMesh.h>
#include <Pyros3D/Physics/Components/TriangleMesh/PhysicsTriangleMesh.h>
#include <Pyros3D/Physics/Components/MultipleSphere/PhysicsMultipleSphere.h>

#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#endif

#include <fstream>
#include <map>

namespace p3d {

	using json = nlohmann::json;

	// ******************************* helpers *******************************

	static json ToJson(const Vec3 &v) { return json::array({ v.x, v.y, v.z }); }
	static json ToJson(const Vec4 &v) { return json::array({ v.x, v.y, v.z, v.w }); }
	static Vec3 Vec3FromJson(const json &j) { return Vec3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>()); }
	static Vec4 Vec4FromJson(const json &j) { return Vec4(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>()); }

	// ******************************* save *******************************

	static uint32 GetOrAddMaterial(IMaterial* mat, json &materialsArray, std::map<IMaterial*, uint32> &materialIdMap)
	{
		std::map<IMaterial*, uint32>::iterator found = materialIdMap.find(mat);
		if (found != materialIdMap.end()) return found->second;

		uint32 id = (uint32)materialsArray.size();
		json m;
		m["id"] = id;

		if (GenericShaderMaterial* gm = dynamic_cast<GenericShaderMaterial*>(mat))
		{
			m["kind"] = "generic";
			m["options"] = gm->GetOptions();
			m["color"] = ToJson(gm->GetColor());
			m["specular"] = ToJson(gm->GetSpecular());
			m["displacementHeight"] = gm->GetDisplacementHeight();
			m["reflectivity"] = gm->GetReflectivity();
			m["shininess"] = gm->GetShininess();
			m["metallic"] = gm->GetMetallic();
			m["roughness"] = gm->GetRoughness();
			m["ssrEnabled"] = gm->IsSSREnabled();

			Texture* t;
			if ((t = gm->GetColorMap()) && !t->GetFilename().empty()) m["colorMap"] = t->GetFilename();
			if ((t = gm->GetSpecularMap()) && !t->GetFilename().empty()) m["specularMap"] = t->GetFilename();
			if ((t = gm->GetNormalMap()) && !t->GetFilename().empty()) m["normalMap"] = t->GetFilename();
			if ((t = gm->GetDisplacementMap()) && !t->GetFilename().empty()) m["displacementMap"] = t->GetFilename();
			if ((t = gm->GetEnvMap()) && !t->GetFilename().empty()) m["envMap"] = t->GetFilename();
			if ((t = gm->GetRefractMap()) && !t->GetFilename().empty()) m["refractMap"] = t->GetFilename();
			if ((t = gm->GetSkyboxMap()) && !t->GetFilename().empty()) m["skyboxMap"] = t->GetFilename();
			if ((t = gm->GetMetallicRoughnessMap()) && !t->GetFilename().empty()) m["metallicRoughnessMap"] = t->GetFilename();
		}
		else if (CustomShaderMaterial* cm = dynamic_cast<CustomShaderMaterial*>(mat))
		{
			if (cm->GetShaderFile().empty())
			{
				m["kind"] = "unsupported";
				echo("WARNING: SceneSerializer - skipping a CustomShaderMaterial built from a raw Shader* (no recoverable shader file), saved as an unsupported placeholder");
			}
			else
			{
				m["kind"] = "custom";
				m["shaderFile"] = cm->GetShaderFile();
			}
		}
		else
		{
			m["kind"] = "unsupported";
			echo("WARNING: SceneSerializer - skipping an unrecognized material type on save");
		}

		// Shared IMaterial fields - written regardless of kind.
		m["opacity"] = mat->GetOpacity();
		m["transparent"] = mat->IsTransparent();
		m["cullFace"] = mat->GetCullFace();
		m["wireframe"] = mat->IsWireFrame();
		m["castingShadows"] = mat->IsCastingShadows();
		m["depthTest"] = mat->IsDepthTesting();
		m["depthWrite"] = mat->IsDepthWritting();
		m["blending"] = mat->IsBlendingEnabled();
		m["blendSFactor"] = mat->GetBlendingSFactor();
		m["blendDFactor"] = mat->GetBlendingDFactor();
		m["blendEquation"] = mat->GetBlendingEquation();
		m["depthBias"] = mat->IsDepthBiasEnabled();
		m["depthBiasFactor"] = mat->GetDepthBiasFactor();
		m["depthBiasUnits"] = mat->GetDepthBiasUnits();

		materialsArray.push_back(m);
		materialIdMap[mat] = id;
		return id;
	}

	// Only Primitive/Model renderables are supported - anything else
	// (Text, Decal) returns an empty/null json, caller skips.
	static json SerializeRenderable(Renderable* r)
	{
		json j;
		if (Model* model = dynamic_cast<Model*>(r))
		{
			if (model->GetPath().empty())
			{
				echo("WARNING: SceneSerializer - skipping a Model with no recoverable source path");
				return json();
			}
			j["kind"] = "model";
			j["path"] = model->GetPath();
			j["mergeMeshes"] = model->GetMergeMeshes();
			return j;
		}
		if (Primitive* prim = dynamic_cast<Primitive*>(r))
		{
			j["kind"] = "primitive";
			j["smooth"] = prim->IsSmooth();
			j["flip"] = prim->IsFlipped();
			j["tangentBitangent"] = prim->HasTangentBitangent();
			switch (prim->GetPrimitiveType())
			{
			case PrimitiveType::Cube:
			{
				Cube* s = static_cast<Cube*>(prim);
				j["shape"] = "Cube";
				j["width"] = s->GetWidth(); j["height"] = s->GetHeight(); j["depth"] = s->GetDepth();
				return j;
			}
			case PrimitiveType::Sphere:
			{
				Sphere* s = static_cast<Sphere*>(prim);
				j["shape"] = "Sphere";
				j["radius"] = s->GetRadius(); j["segmentsW"] = s->GetSegmentsW(); j["segmentsH"] = s->GetSegmentsH();
				j["halfSphere"] = s->IsHalfSphere();
				return j;
			}
			case PrimitiveType::Cone:
			{
				Cone* s = static_cast<Cone*>(prim);
				j["shape"] = "Cone";
				j["radius"] = s->GetRadius(); j["height"] = s->GetHeight();
				j["segmentsW"] = s->GetSegmentsW(); j["segmentsH"] = s->GetSegmentsH();
				j["openEnded"] = s->IsOpenEnded();
				return j;
			}
			case PrimitiveType::Cylinder:
			{
				Cylinder* s = static_cast<Cylinder*>(prim);
				j["shape"] = "Cylinder";
				j["radius"] = s->GetRadius(); j["height"] = s->GetHeight();
				j["segmentsW"] = s->GetSegmentsW(); j["segmentsH"] = s->GetSegmentsH();
				j["openEnded"] = s->IsOpenEnded();
				return j;
			}
			case PrimitiveType::Plane:
			{
				Plane* s = static_cast<Plane*>(prim);
				j["shape"] = "Plane";
				j["width"] = s->GetWidth(); j["height"] = s->GetHeight();
				return j;
			}
			case PrimitiveType::Capsule:
			{
				Capsule* s = static_cast<Capsule*>(prim);
				j["shape"] = "Capsule";
				j["radius"] = s->GetRadius(); j["height"] = s->GetHeight();
				j["numRings"] = s->GetNumRings(); j["segmentsW"] = s->GetSegmentsW(); j["segmentsH"] = s->GetSegmentsH();
				return j;
			}
			case PrimitiveType::Torus:
			{
				Torus* s = static_cast<Torus*>(prim);
				j["shape"] = "Torus";
				j["radius"] = s->GetRadius(); j["tube"] = s->GetTube();
				j["segmentsW"] = s->GetSegmentsW(); j["segmentsH"] = s->GetSegmentsH();
				return j;
			}
			case PrimitiveType::TorusKnot:
			{
				TorusKnot* s = static_cast<TorusKnot*>(prim);
				j["shape"] = "TorusKnot";
				j["radius"] = s->GetRadius(); j["tube"] = s->GetTube();
				j["segmentsW"] = s->GetSegmentsW(); j["segmentsH"] = s->GetSegmentsH();
				j["p"] = s->GetP(); j["q"] = s->GetQ(); j["heightScale"] = s->GetHeightScale();
				return j;
			}
			default:
				echo("WARNING: SceneSerializer - skipping an unrecognized Primitive subtype on save");
				return json();
			}
		}
		echo("WARNING: SceneSerializer - skipping an unrecognized Renderable type (only Primitive/Model shapes are supported)");
		return json();
	}

	static json SerializeComponent(IComponent* c, json &materialsArray, std::map<IMaterial*, uint32> &materialIdMap)
	{
		json j;
		switch (c->GetComponentType())
		{
		case ComponentType::RenderingComponent:
		{
			RenderingComponent* rc = dynamic_cast<RenderingComponent*>(c);
			// Only LOD 0's renderable is recoverable (AddLOD() never
			// updates the single shared `renderable` field - see
			// VULKAN_ROADMAP.md's Scene serialization section) - a
			// documented v1 simplification, not a bug.
			json renderableJson = SerializeRenderable(rc->GetRenderable());
			if (renderableJson.is_null()) return json();
			if (rc->GetMeshes(0).empty()) return json();
			IMaterial* mat = rc->GetMeshes(0)[0]->Material;
			if (!mat) return json();

			j["type"] = "RenderingComponent";
			j["cullTest"] = rc->IsCullTesting();
			j["castingShadows"] = rc->IsCastingShadows();
			j["material"] = GetOrAddMaterial(mat, materialsArray, materialIdMap);
			j["renderable"] = renderableJson;
			return j;
		}
		case ComponentType::ParticleSystem:
		{
			ParticleSystem* ps = dynamic_cast<ParticleSystem*>(c);
			const ParticleSystemDesc &d = ps->GetDesc();
			j["type"] = "ParticleSystem";
			j["maxParticles"] = d.maxParticles;
			if (d.texture && !d.texture->GetFilename().empty()) j["texture"] = d.texture->GetFilename();
			j["looping"] = d.looping;
			j["emissionRate"] = d.emissionRate;
			j["burstCount"] = d.burstCount;
			j["minLifetime"] = d.minLifetime;
			j["maxLifetime"] = d.maxLifetime;
			j["direction"] = ToJson(d.direction);
			j["spreadAngle"] = d.spreadAngle;
			j["minSpeed"] = d.minSpeed;
			j["maxSpeed"] = d.maxSpeed;
			j["gravity"] = ToJson(d.gravity);
			j["damping"] = d.damping;
			j["startSize"] = d.startSize;
			j["endSize"] = d.endSize;
			j["sizeRandomJitter"] = d.sizeRandomJitter;
			j["startColor"] = ToJson(d.startColor);
			j["endColor"] = ToJson(d.endColor);
			j["fadeInFraction"] = d.fadeInFraction;
			j["fadeOutFraction"] = d.fadeOutFraction;
			j["minRotationSpeed"] = d.minRotationSpeed;
			j["maxRotationSpeed"] = d.maxRotationSpeed;
			j["blendMode"] = d.blendMode;
			j["boundingSphereRadius"] = d.boundingSphereRadius;
			return j;
		}
		case ComponentType::DirectionalLight:
		{
			DirectionalLight* l = dynamic_cast<DirectionalLight*>(c);
			j["type"] = "DirectionalLight";
			j["color"] = ToJson(l->GetLightColor());
			j["direction"] = ToJson(l->GetLightDirection());
			j["castingShadows"] = l->IsCastingShadows();
			if (l->IsCastingShadows())
			{
				j["shadowWidth"] = l->GetShadowWidth();
				j["shadowHeight"] = l->GetShadowHeight();
				j["shadowNear"] = l->GetShadowNear();
				j["shadowFar"] = l->GetShadowFar();
				j["cascades"] = l->GetNumberCascades();
				Cascade c0 = l->GetCascade(0);
				j["fov"] = c0.Fov - CASCADE_FACTOR;
				j["aspect"] = c0.Ratio;
				j["shadowBiasFactor"] = l->GetShadowBiasFactor();
				j["shadowBiasUnits"] = l->GetShadowBiasUnits();
			}
			return j;
		}
		case ComponentType::PointLight:
		{
			PointLight* l = dynamic_cast<PointLight*>(c);
			j["type"] = "PointLight";
			j["color"] = ToJson(l->GetLightColor());
			j["radius"] = l->GetLightRadius();
			j["castingShadows"] = l->IsCastingShadows();
			if (l->IsCastingShadows())
			{
				j["shadowWidth"] = l->GetShadowWidth();
				j["shadowHeight"] = l->GetShadowHeight();
				j["shadowNear"] = l->GetShadowNear();
			}
			return j;
		}
		case ComponentType::SpotLight:
		{
			SpotLight* l = dynamic_cast<SpotLight*>(c);
			j["type"] = "SpotLight";
			j["color"] = ToJson(l->GetLightColor());
			j["radius"] = l->GetLightRadius();
			j["direction"] = ToJson(l->GetLightDirection());
			j["innerCone"] = l->GetLightInnerCone();
			j["outterCone"] = l->GetLightOutterCone();
			j["castingShadows"] = l->IsCastingShadows();
			if (l->IsCastingShadows())
			{
				j["shadowWidth"] = l->GetShadowWidth();
				j["shadowHeight"] = l->GetShadowHeight();
				j["shadowNear"] = l->GetShadowNear();
			}
			return j;
		}
		case ComponentType::Physics:
		{
			IPhysicsComponent* pc = dynamic_cast<IPhysicsComponent*>(c);
			j["mass"] = pc->GetMass();
			j["ghost"] = pc->IsGhost();
			switch (pc->GetShape())
			{
			case CollisionShapes::Box:
			{
				PhysicsBox* s = static_cast<PhysicsBox*>(pc);
				j["type"] = "Physics"; j["shape"] = "Box";
				j["width"] = s->GetWidth(); j["height"] = s->GetHeight(); j["depth"] = s->GetDepth();
				return j;
			}
			case CollisionShapes::Sphere:
			{
				PhysicsSphere* s = static_cast<PhysicsSphere*>(pc);
				j["type"] = "Physics"; j["shape"] = "Sphere";
				j["radius"] = s->GetRadius();
				return j;
			}
			case CollisionShapes::Capsule:
			{
				PhysicsCapsule* s = static_cast<PhysicsCapsule*>(pc);
				j["type"] = "Physics"; j["shape"] = "Capsule";
				j["radius"] = s->GetRadius(); j["height"] = s->GetHeight();
				return j;
			}
			case CollisionShapes::Cone:
			{
				PhysicsCone* s = static_cast<PhysicsCone*>(pc);
				j["type"] = "Physics"; j["shape"] = "Cone";
				j["radius"] = s->GetRadius(); j["height"] = s->GetHeight();
				return j;
			}
			case CollisionShapes::Cylinder:
			{
				PhysicsCylinder* s = static_cast<PhysicsCylinder*>(pc);
				j["type"] = "Physics"; j["shape"] = "Cylinder";
				j["radius"] = s->GetRadius(); j["height"] = s->GetHeight();
				return j;
			}
			case CollisionShapes::StaticPlane:
			{
				PhysicsStaticPlane* s = static_cast<PhysicsStaticPlane*>(pc);
				j["type"] = "Physics"; j["shape"] = "StaticPlane";
				j["normal"] = ToJson(s->GetNormal()); j["constant"] = s->GetConstant();
				return j;
			}
			case CollisionShapes::ConvexHull:
			{
				PhysicsConvexHull* s = static_cast<PhysicsConvexHull*>(pc);
				j["type"] = "Physics"; j["shape"] = "ConvexHull";
				json pts = json::array();
				for (size_t i = 0; i < s->GetPoints().size(); i++) pts.push_back(ToJson(s->GetPoints()[i]));
				j["points"] = pts;
				return j;
			}
			case CollisionShapes::ConvexTriangleMesh:
			case CollisionShapes::TriangleMesh:
			{
				const std::vector<unsigned>* index; const std::vector<Vec3>* vertex;
				if (pc->GetShape() == CollisionShapes::ConvexTriangleMesh)
				{
					PhysicsConvexTriangleMesh* s = static_cast<PhysicsConvexTriangleMesh*>(pc);
					index = &s->GetIndexData(); vertex = &s->GetVertexData();
					j["shape"] = "ConvexTriangleMesh";
				}
				else
				{
					PhysicsTriangleMesh* s = static_cast<PhysicsTriangleMesh*>(pc);
					index = &s->GetIndexData(); vertex = &s->GetVertexData();
					j["shape"] = "TriangleMesh";
				}
				j["type"] = "Physics";
				json idx = json::array();
				for (size_t i = 0; i < index->size(); i++) idx.push_back((*index)[i]);
				json vtx = json::array();
				for (size_t i = 0; i < vertex->size(); i++) vtx.push_back(ToJson((*vertex)[i]));
				j["indices"] = idx; j["vertices"] = vtx;
				return j;
			}
			case CollisionShapes::MultipleSphere:
			{
				PhysicsMultipleSphere* s = static_cast<PhysicsMultipleSphere*>(pc);
				j["type"] = "Physics"; j["shape"] = "MultipleSphere";
				json pos = json::array();
				for (size_t i = 0; i < s->GetPositions().size(); i++) pos.push_back(ToJson(s->GetPositions()[i]));
				json rad = json::array();
				for (size_t i = 0; i < s->GetRadius().size(); i++) rad.push_back(s->GetRadius()[i]);
				j["positions"] = pos; j["radii"] = rad;
				return j;
			}
			default:
				// Vehicle/HeightFieldTerrain/Ghost-as-shape - not
				// emitted, see VULKAN_ROADMAP.md's non-goals list.
				echo("WARNING: SceneSerializer - skipping a physics component with an unsupported shape type");
				return json();
			}
		}
#ifdef LUA_BINDINGS
		case ComponentType::LuaComponent:
			// Existence only - on_init/on_update/on_destroy are live Lua
			// closures, not generically serializable.
			j["type"] = "LuaComponent";
			return j;
#endif
		default:
			return json();
		}
	}

	static json SerializeGameObject(GameObject* go, json &materialsArray, std::map<IMaterial*, uint32> &materialIdMap)
	{
		json j;
		j["name"] = go->GetName();
		j["static"] = go->IsStatic();
		j["position"] = ToJson(go->GetPosition());
		j["rotation"] = ToJson(go->GetRotation());
		j["scale"] = ToJson(go->GetScale());

		json tags = json::array();
		const std::map<uint32, std::string> &tagsMap = go->GetTags();
		for (std::map<uint32, std::string>::const_iterator it = tagsMap.begin(); it != tagsMap.end(); ++it)
			tags.push_back(it->second);
		j["tags"] = tags;

		json components = json::array();
		const std::vector<IComponent*> &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
		{
			json cj = SerializeComponent(comps[i], materialsArray, materialIdMap);
			if (!cj.is_null()) components.push_back(cj);
		}
		j["components"] = components;

		json children = json::array();
		const std::vector<GameObject*> &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			children.push_back(SerializeGameObject(kids[i], materialsArray, materialIdMap));
		j["children"] = children;

		return j;
	}

	bool SceneSerializer::SaveScene(SceneGraph* scene, const std::string &filePath)
	{
		json root;
		root["version"] = 1;

		json materialsArray = json::array();
		std::map<IMaterial*, uint32> materialIdMap;

		json roots = json::array();
		std::vector<GameObject*> &all = scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); i++)
			roots.push_back(SerializeGameObject(all[i], materialsArray, materialIdMap));

		root["materials"] = materialsArray;
		root["roots"] = roots;

		std::ofstream out(filePath.c_str());
		if (!out.is_open())
		{
			echo("ERROR: SceneSerializer::SaveScene - couldn't open file for writing: " + filePath);
			return false;
		}
		out << root.dump(4);
		out.close();
		return true;
	}

	// ******************************* load *******************************

	static Texture* GetOrLoadTexture(const std::string &path, std::map<std::string, Texture*> &cache)
	{
		if (path.empty()) return NULL;
		std::map<std::string, Texture*>::iterator it = cache.find(path);
		if (it != cache.end()) return it->second;
		Texture* tex = new Texture();
		tex->LoadTexture(path, TextureType::Texture);
		cache[path] = tex;
		return tex;
	}

	static void ApplyCommonMaterialFields(IMaterial* mat, const json &j)
	{
		mat->SetOpacity(j.value("opacity", 1.0f));
		mat->SetTransparencyFlag(j.value("transparent", false));
		mat->SetCullFace(j.value("cullFace", (uint32)CullFace::BackFace));
		if (j.value("wireframe", false)) mat->StartRenderWireFrame(); else mat->StopRenderWireFrame();
		if (j.value("castingShadows", true)) mat->EnableCastingShadows(); else mat->DisableCastingShadows();
		if (j.value("depthTest", true)) mat->EnableDepthTest(); else mat->DisableDepthTest();
		if (j.value("depthWrite", true)) mat->EnableDepthWrite(); else mat->DisableDepthWrite();
		if (j.value("blending", false))
		{
			mat->EnableBlending();
			mat->BlendingFunction(j.value("blendSFactor", 0u), j.value("blendDFactor", 0u));
			mat->BlendingEquation(j.value("blendEquation", 0u));
		}
		else mat->DisableBlending();
		if (j.value("depthBias", false)) mat->EnableDethBias(j.value("depthBiasFactor", 0.0f), j.value("depthBiasUnits", 0.0f));
		else mat->DisableDethBias();
	}

	static IMaterial* BuildMaterial(const json &j, std::map<std::string, Texture*> &textureCache)
	{
		std::string kind = j.value("kind", "unsupported");
		if (kind == "generic")
		{
			GenericShaderMaterial* gm = new GenericShaderMaterial(j.value("options", 0u));
			if ((j.find("color") != j.end())) gm->SetColor(Vec4FromJson(j["color"]));
			if ((j.find("specular") != j.end())) gm->SetSpecular(Vec4FromJson(j["specular"]));
			if ((j.find("displacementHeight") != j.end())) gm->SetDisplacementHeight(j["displacementHeight"].get<f32>());
			if ((j.find("reflectivity") != j.end())) gm->SetReflectivity(j["reflectivity"].get<f32>());
			if ((j.find("shininess") != j.end())) gm->SetShininess(j["shininess"].get<f32>());
			if ((j.find("metallic") != j.end())) gm->SetMetallic(j["metallic"].get<f32>());
			if ((j.find("roughness") != j.end())) gm->SetRoughness(j["roughness"].get<f32>());
			if (j.value("ssrEnabled", false)) gm->SetSSREnabled(true);
			if ((j.find("colorMap") != j.end())) gm->SetColorMap(GetOrLoadTexture(j["colorMap"].get<std::string>(), textureCache));
			if ((j.find("specularMap") != j.end())) gm->SetSpecularMap(GetOrLoadTexture(j["specularMap"].get<std::string>(), textureCache));
			if ((j.find("normalMap") != j.end())) gm->SetNormalMap(GetOrLoadTexture(j["normalMap"].get<std::string>(), textureCache));
			if ((j.find("displacementMap") != j.end())) gm->SetDisplacementMap(GetOrLoadTexture(j["displacementMap"].get<std::string>(), textureCache));
			if ((j.find("envMap") != j.end())) gm->SetEnvMap(GetOrLoadTexture(j["envMap"].get<std::string>(), textureCache));
			if ((j.find("refractMap") != j.end())) gm->SetRefractMap(GetOrLoadTexture(j["refractMap"].get<std::string>(), textureCache));
			if ((j.find("skyboxMap") != j.end())) gm->SetSkyboxMap(GetOrLoadTexture(j["skyboxMap"].get<std::string>(), textureCache));
			if ((j.find("metallicRoughnessMap") != j.end())) gm->SetMetallicRoughnessMap(GetOrLoadTexture(j["metallicRoughnessMap"].get<std::string>(), textureCache));
			ApplyCommonMaterialFields(gm, j);
			return gm;
		}
		else if (kind == "custom")
		{
			CustomShaderMaterial* cm = new CustomShaderMaterial(j.value("shaderFile", std::string()));
			ApplyCommonMaterialFields(cm, j);
			return cm;
		}
		echo("WARNING: SceneSerializer - skipping an unsupported material entry on load");
		return NULL;
	}

	static Renderable* DeserializeRenderable(const json &j)
	{
		if (j.is_null()) return NULL;
		std::string kind = j.value("kind", "");
		if (kind == "model")
			return new Model(j.value("path", std::string()), j.value("mergeMeshes", true));
		if (kind != "primitive") return NULL;

		bool smooth = j.value("smooth", false);
		bool flip = j.value("flip", false);
		bool tb = j.value("tangentBitangent", false);
		std::string shape = j.value("shape", "");

		if (shape == "Cube") return new Cube(j.value("width", 1.0f), j.value("height", 1.0f), j.value("depth", 1.0f), smooth, flip, tb);
		if (shape == "Sphere") return new Sphere(j.value("radius", 1.0f), j.value("segmentsW", 16u), j.value("segmentsH", 16u), smooth, j.value("halfSphere", false), flip, tb);
		if (shape == "Cone") return new Cone(j.value("radius", 1.0f), j.value("height", 1.0f), j.value("segmentsW", 16u), j.value("segmentsH", 16u), j.value("openEnded", false), smooth, flip, tb);
		if (shape == "Cylinder") return new Cylinder(j.value("radius", 1.0f), j.value("height", 1.0f), j.value("segmentsW", 16u), j.value("segmentsH", 16u), j.value("openEnded", false), smooth, flip, tb);
		if (shape == "Plane") return new Plane(j.value("width", 1.0f), j.value("height", 1.0f), smooth, flip, tb);
		if (shape == "Capsule") return new Capsule(j.value("radius", 1.0f), j.value("height", 1.0f), j.value("numRings", 8u), j.value("segmentsW", 16u), j.value("segmentsH", 16u), smooth, flip, tb);
		if (shape == "Torus") return new Torus(j.value("radius", 1.0f), j.value("tube", 0.3f), j.value("segmentsW", 60u), j.value("segmentsH", 6u), smooth, flip, tb);
		if (shape == "TorusKnot") return new TorusKnot(j.value("radius", 1.0f), j.value("tube", 0.3f), j.value("segmentsW", 60u), j.value("segmentsH", 6u), j.value("p", 2.0f), j.value("q", 3.0f), j.value("heightScale", 1u), smooth, flip, tb);

		echo("WARNING: SceneSerializer - unrecognized primitive shape on load: " + shape);
		return NULL;
	}

	static void DeserializeComponent(GameObject* go, const json &j, const std::vector<IMaterial*> &materialsById, std::map<std::string, Texture*> &textureCache, IPhysics* physics)
	{
		std::string type = j.value("type", "");

		if (type == "RenderingComponent")
		{
			Renderable* renderable = DeserializeRenderable(j.value("renderable", json()));
			if (!renderable) { echo("WARNING: SceneSerializer - skipping RenderingComponent, couldn't rebuild its renderable"); return; }
			uint32 matId = j.value("material", (uint32)0xFFFFFFFF);
			IMaterial* mat = (matId < materialsById.size()) ? materialsById[matId] : NULL;
			if (!mat) { echo("WARNING: SceneSerializer - skipping RenderingComponent, its material couldn't be rebuilt"); delete renderable; return; }
			RenderingComponent* rc = new RenderingComponent(renderable, mat, 0.0f);
			if (j.value("cullTest", true)) rc->EnableCullTest(); else rc->DisableCullTest();
			if (j.value("castingShadows", true)) rc->EnableCastShadows(); else rc->DisableCastShadows();
			go->AddComponent(rc);
		}
		else if (type == "ParticleSystem")
		{
			ParticleSystemDesc d;
			d.maxParticles = j.value("maxParticles", d.maxParticles);
			if ((j.find("texture") != j.end())) d.texture = GetOrLoadTexture(j["texture"].get<std::string>(), textureCache);
			d.looping = j.value("looping", d.looping);
			d.emissionRate = j.value("emissionRate", d.emissionRate);
			d.burstCount = j.value("burstCount", d.burstCount);
			d.minLifetime = j.value("minLifetime", d.minLifetime);
			d.maxLifetime = j.value("maxLifetime", d.maxLifetime);
			if ((j.find("direction") != j.end())) d.direction = Vec3FromJson(j["direction"]);
			d.spreadAngle = j.value("spreadAngle", d.spreadAngle);
			d.minSpeed = j.value("minSpeed", d.minSpeed);
			d.maxSpeed = j.value("maxSpeed", d.maxSpeed);
			if ((j.find("gravity") != j.end())) d.gravity = Vec3FromJson(j["gravity"]);
			d.damping = j.value("damping", d.damping);
			d.startSize = j.value("startSize", d.startSize);
			d.endSize = j.value("endSize", d.endSize);
			d.sizeRandomJitter = j.value("sizeRandomJitter", d.sizeRandomJitter);
			if ((j.find("startColor") != j.end())) d.startColor = Vec4FromJson(j["startColor"]);
			if ((j.find("endColor") != j.end())) d.endColor = Vec4FromJson(j["endColor"]);
			d.fadeInFraction = j.value("fadeInFraction", d.fadeInFraction);
			d.fadeOutFraction = j.value("fadeOutFraction", d.fadeOutFraction);
			d.minRotationSpeed = j.value("minRotationSpeed", d.minRotationSpeed);
			d.maxRotationSpeed = j.value("maxRotationSpeed", d.maxRotationSpeed);
			d.blendMode = j.value("blendMode", d.blendMode);
			d.boundingSphereRadius = j.value("boundingSphereRadius", d.boundingSphereRadius);
			go->AddComponent(new ParticleSystem(d));
		}
		else if (type == "DirectionalLight")
		{
			Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
			Vec3 direction = (j.find("direction") != j.end()) ? Vec3FromJson(j["direction"]) : Vec3(0, -1, 0);
			DirectionalLight* l = new DirectionalLight(color, direction);
			if (j.value("castingShadows", false))
			{
				Projection proj;
				proj.Perspective(j.value("fov", 70.0f), j.value("aspect", 1.777f), j.value("shadowNear", 1.0f), j.value("shadowFar", 100.0f));
				l->EnableCastShadows(j.value("shadowWidth", 1024u), j.value("shadowHeight", 1024u), proj, j.value("shadowNear", 1.0f), j.value("shadowFar", 100.0f), j.value("cascades", 1u));
				if ((j.find("shadowBiasFactor") != j.end()) || (j.find("shadowBiasUnits") != j.end()))
					l->SetShadowBias(j.value("shadowBiasFactor", 1.0f), j.value("shadowBiasUnits", 1.0f));
			}
			go->AddComponent(l);
		}
		else if (type == "PointLight")
		{
			Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
			PointLight* l = new PointLight(color, j.value("radius", 1.0f));
			if (j.value("castingShadows", false))
				l->EnableCastShadows(j.value("shadowWidth", 512u), j.value("shadowHeight", 512u), j.value("shadowNear", 0.1f));
			go->AddComponent(l);
		}
		else if (type == "SpotLight")
		{
			Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
			Vec3 direction = (j.find("direction") != j.end()) ? Vec3FromJson(j["direction"]) : Vec3(0, -1, 0);
			SpotLight* l = new SpotLight(color, j.value("radius", 1.0f), direction, j.value("outterCone", 45.0f), j.value("innerCone", 30.0f));
			if (j.value("castingShadows", false))
				l->EnableCastShadows(j.value("shadowWidth", 512u), j.value("shadowHeight", 512u), j.value("shadowNear", 0.1f));
			go->AddComponent(l);
		}
		else if (type == "Physics")
		{
			if (!physics) { echo("WARNING: SceneSerializer - skipping a Physics component, LoadScene() was called with physics == NULL"); return; }
			std::string shape = j.value("shape", "");
			f32 mass = j.value("mass", 0.0f);
			bool ghost = j.value("ghost", false);
			IPhysicsComponent* pc = NULL;
			if (shape == "Box") pc = physics->CreateBox(j.value("width", 1.0f), j.value("height", 1.0f), j.value("depth", 1.0f), mass, ghost);
			else if (shape == "Sphere") pc = physics->CreateSphere(j.value("radius", 1.0f), mass, ghost);
			else if (shape == "Capsule") pc = physics->CreateCapsule(j.value("radius", 1.0f), j.value("height", 1.0f), mass, ghost);
			else if (shape == "Cone") pc = physics->CreateCone(j.value("radius", 1.0f), j.value("height", 1.0f), mass, ghost);
			else if (shape == "Cylinder") pc = physics->CreateCylinder(j.value("radius", 1.0f), j.value("height", 1.0f), mass, ghost);
			else if (shape == "StaticPlane") pc = physics->CreateStaticPlane((j.find("normal") != j.end()) ? Vec3FromJson(j["normal"]) : Vec3(0, 1, 0), j.value("constant", 0.0f), mass, ghost);
			else if (shape == "ConvexHull")
			{
				std::vector<Vec3> pts;
				if ((j.find("points") != j.end())) for (auto &p : j["points"]) pts.push_back(Vec3FromJson(p));
				pc = physics->CreateConvexHull(pts, mass, ghost);
			}
			else if (shape == "ConvexTriangleMesh" || shape == "TriangleMesh")
			{
				std::vector<uint32> idx;
				std::vector<Vec3> vtx;
				if ((j.find("indices") != j.end())) for (auto &i : j["indices"]) idx.push_back(i.get<uint32>());
				if ((j.find("vertices") != j.end())) for (auto &v : j["vertices"]) vtx.push_back(Vec3FromJson(v));
				pc = (shape == "ConvexTriangleMesh") ? physics->CreateConvexTriangleMesh(idx, vtx, mass, ghost) : physics->CreateTriangleMesh(idx, vtx, mass, ghost);
			}
			else if (shape == "MultipleSphere")
			{
				std::vector<Vec3> pos;
				std::vector<f32> rad;
				if ((j.find("positions") != j.end())) for (auto &p : j["positions"]) pos.push_back(Vec3FromJson(p));
				if ((j.find("radii") != j.end())) for (auto &r : j["radii"]) rad.push_back(r.get<f32>());
				pc = physics->CreateMultipleSphere(pos, rad, mass, ghost);
			}
			else
			{
				echo("WARNING: SceneSerializer - skipping a physics component with an unsupported shape on load: " + shape);
				return;
			}
			if (pc) go->AddComponent(pc);
		}
#ifdef LUA_BINDINGS
		else if (type == "LuaComponent")
		{
			// Existence only, no behavior - see the class comment.
			go->AddComponent(new LuaComponent());
		}
#endif
	}

	static GameObject* DeserializeGameObject(const json &j, const std::vector<IMaterial*> &materialsById, std::map<std::string, Texture*> &textureCache, IPhysics* physics)
	{
		GameObject* go = new GameObject(j.value("static", false));
		go->SetName(j.value("name", std::string()));
		if ((j.find("position") != j.end())) go->SetPosition(Vec3FromJson(j["position"]));
		if ((j.find("rotation") != j.end())) go->SetRotation(Vec3FromJson(j["rotation"]));
		if ((j.find("scale") != j.end())) go->SetScale(Vec3FromJson(j["scale"]));
		if ((j.find("tags") != j.end())) for (auto &t : j["tags"]) go->AddTag(t.get<std::string>());

		if ((j.find("components") != j.end())) for (auto &cj : j["components"]) DeserializeComponent(go, cj, materialsById, textureCache, physics);

		if ((j.find("children") != j.end()))
			for (auto &cj : j["children"])
			{
				GameObject* child = DeserializeGameObject(cj, materialsById, textureCache, physics);
				go->Add(child);
			}

		return go;
	}

	bool SceneSerializer::LoadScene(SceneGraph* scene, const std::string &filePath, IPhysics* physics)
	{
		std::ifstream in(filePath.c_str());
		if (!in.is_open())
		{
			echo("ERROR: SceneSerializer::LoadScene - couldn't open file: " + filePath);
			return false;
		}
		json root;
		try
		{
			in >> root;
		}
		catch (const std::exception&)
		{
			echo("ERROR: SceneSerializer::LoadScene - invalid JSON in file: " + filePath);
			return false;
		}
		in.close();

		if (root.value("version", 0) != 1)
			echo("WARNING: SceneSerializer::LoadScene - unexpected scene file version, attempting to load anyway");

		std::map<std::string, Texture*> textureCache;
		std::vector<IMaterial*> materialsById;
		if ((root.find("materials") != root.end()))
			for (auto &mj : root["materials"])
				materialsById.push_back(BuildMaterial(mj, textureCache));

		if ((root.find("roots") != root.end()))
			for (auto &rj : root["roots"])
				scene->Add(DeserializeGameObject(rj, materialsById, textureCache, physics));

		return true;
	}

}
