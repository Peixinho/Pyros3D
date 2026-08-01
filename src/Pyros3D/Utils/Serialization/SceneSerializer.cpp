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
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
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
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Font/Font.h>

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
#include <Pyros3D/Physics/Components/Vehicle/PhysicsVehicle.h>

#ifdef LUA_BINDINGS
#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
#endif

#include <fstream>
#include <map>

namespace p3d {

	using json = nlohmann::json;

	// ******************************* helpers *******************************

#ifdef LUA_BINDINGS
	// sol2 has no built-in bridge to nlohmann::json - these convert
	// between a Lua table (as returned by a LuaComponent's serialize()
	// method, or passed to its deserialize()) and plain JSON, handling
	// only primitive values (string/number/bool) and nested
	// tables/arrays, matching the documented serialize()/deserialize()
	// contract (examples using this are expected to return only
	// primitive-valued data, not engine objects).
	static json LuaTableToJson(const sol::table &t);

	static json LuaValueToJson(const sol::object &obj)
	{
		switch (obj.get_type())
		{
		case sol::type::string: return obj.as<std::string>();
		case sol::type::number: return obj.as<double>();
		case sol::type::boolean: return obj.as<bool>();
		case sol::type::table: return LuaTableToJson(obj.as<sol::table>());
		default: return json();
		}
	}

	static json LuaTableToJson(const sol::table &t)
	{
		// Array iff every key is a positive integer 1..N with no gaps.
		size_t count = 0;
		bool isArray = true;
		for (auto &kv : t)
		{
			count++;
			sol::object key = kv.first;
			if (key.get_type() != sol::type::number) { isArray = false; continue; }
			double d = key.as<double>();
			if (d != (double)(int64_t)d || (int64_t)d < 1) isArray = false;
		}
		if (isArray && count > 0)
		{
			json arr = json::array();
			bool contiguous = true;
			for (size_t i = 1; i <= count; i++)
				if (!t[i].valid()) { contiguous = false; break; }
			if (contiguous)
			{
				for (size_t i = 1; i <= count; i++)
					arr.push_back(LuaValueToJson(t[i]));
				return arr;
			}
		}
		json obj = json::object();
		for (auto &kv : t)
		{
			sol::object key = kv.first;
			std::string keyStr;
			if (key.get_type() == sol::type::string) keyStr = key.as<std::string>();
			else if (key.get_type() == sol::type::number) keyStr = std::to_string(key.as<double>());
			else continue;
			obj[keyStr] = LuaValueToJson(kv.second);
		}
		return obj;
	}

	static sol::object JsonToLuaValue(sol::state &lua, const json &j)
	{
		if (j.is_string()) return sol::make_object(lua, j.get<std::string>());
		if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
		if (j.is_number()) return sol::make_object(lua, j.get<double>());
		if (j.is_array())
		{
			sol::table t = lua.create_table();
			int idx = 1;
			for (auto &e : j) t[idx++] = JsonToLuaValue(lua, e);
			return t;
		}
		if (j.is_object())
		{
			sol::table t = lua.create_table();
			for (json::const_iterator it = j.begin(); it != j.end(); ++it) t[it.key()] = JsonToLuaValue(lua, it.value());
			return t;
		}
		return sol::make_object(lua, sol::lua_nil);
	}
	static sol::table JsonToLuaTable(sol::state &lua, const json &j)
	{
		sol::object o = JsonToLuaValue(lua, j);
		if (o.get_type() == sol::type::table) return o.as<sol::table>();
		return lua.create_table();
	}
#endif

	static json ToJson(const Vec3 &v) { return json::array({ v.x, v.y, v.z }); }
	static json ToJson(const Vec4 &v) { return json::array({ v.x, v.y, v.z, v.w }); }
	static Vec3 Vec3FromJson(const json &j) { return Vec3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>()); }
	static Vec4 Vec4FromJson(const json &j) { return Vec4(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>()); }

	// Self-contained base64 - the vendored nlohmann::json 2.1.1 predates
	// that library's binary/base64 support (added in 3.8+), used to embed
	// raw bytes (Shader source text is already plain text and doesn't
	// need this; Texture::RawData does) for materials/textures with no
	// recoverable file path.
	static const char* kBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static std::string Base64Encode(const std::vector<uchar> &data)
	{
		std::string out;
		out.reserve(((data.size() + 2) / 3) * 4);
		size_t i = 0;
		while (i + 2 < data.size())
		{
			uint32 n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
			out += kBase64Chars[(n >> 18) & 0x3F]; out += kBase64Chars[(n >> 12) & 0x3F];
			out += kBase64Chars[(n >> 6) & 0x3F]; out += kBase64Chars[n & 0x3F];
			i += 3;
		}
		size_t remaining = data.size() - i;
		if (remaining == 1)
		{
			uint32 n = data[i] << 16;
			out += kBase64Chars[(n >> 18) & 0x3F]; out += kBase64Chars[(n >> 12) & 0x3F]; out += "==";
		}
		else if (remaining == 2)
		{
			uint32 n = (data[i] << 16) | (data[i + 1] << 8);
			out += kBase64Chars[(n >> 18) & 0x3F]; out += kBase64Chars[(n >> 12) & 0x3F]; out += kBase64Chars[(n >> 6) & 0x3F]; out += "=";
		}
		return out;
	}
	static std::vector<uchar> Base64Decode(const std::string &in)
	{
		static int8_t table[256];
		static bool tableInit = false;
		if (!tableInit)
		{
			for (int i = 0; i < 256; i++) table[i] = -1;
			for (int i = 0; i < 64; i++) table[(unsigned char)kBase64Chars[i]] = (int8_t)i;
			tableInit = true;
		}
		std::vector<uchar> out;
		out.reserve((in.size() / 4) * 3);
		int32 val = 0, bits = -8;
		for (unsigned char c : in)
		{
			if (table[c] == -1) break; // padding '=' or terminator
			val = (val << 6) + table[c];
			bits += 6;
			if (bits >= 0)
			{
				out.push_back((uchar)((val >> bits) & 0xFF));
				bits -= 8;
			}
		}
		return out;
	}

	// Writes `key` (a path string) when the texture has one, else falls
	// back to `key+"Data"` (base64-embedded RawData) when that's the only
	// recoverable source, else writes nothing (same as before this phase
	// - a texture built from something else entirely, e.g. an FBO render
	// target repurposed as a material input, still can't be saved).
	static void SerializeTextureRef(json &parent, const std::string &key, Texture* t)
	{
		if (!t) return;
		if (!t->GetFilename().empty()) { parent[key] = t->GetFilename(); return; }
		if (!t->GetRawData().empty()) { parent[key + "Data"] = Base64Encode(t->GetRawData()); return; }
	}
	// Mirrors SerializeTextureRef - resolves either key, dedupes
	// path-based loads via textureCache (embedded-data loads always
	// create a fresh Texture, no natural dedup key).
	static Texture* DeserializeTextureRef(const json &parent, const std::string &key, std::map<std::string, Texture*> &textureCache);

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

			SerializeTextureRef(m, "colorMap", gm->GetColorMap());
			SerializeTextureRef(m, "specularMap", gm->GetSpecularMap());
			SerializeTextureRef(m, "normalMap", gm->GetNormalMap());
			SerializeTextureRef(m, "displacementMap", gm->GetDisplacementMap());
			SerializeTextureRef(m, "envMap", gm->GetEnvMap());
			SerializeTextureRef(m, "refractMap", gm->GetRefractMap());
			SerializeTextureRef(m, "skyboxMap", gm->GetSkyboxMap());
			SerializeTextureRef(m, "metallicRoughnessMap", gm->GetMetallicRoughnessMap());
		}
		else if (CustomShaderMaterial* cm = dynamic_cast<CustomShaderMaterial*>(mat))
		{
			if (cm->GetShaderFile().empty() && (!cm->GetShaderObject() || cm->GetShaderObject()->GetShaderText().empty()))
			{
				m["kind"] = "unsupported";
				echo("WARNING: SceneSerializer - skipping a CustomShaderMaterial built from a raw Shader* (no recoverable shader file), saved as an unsupported placeholder");
			}
			else
			{
				m["kind"] = "custom";
				if (!cm->GetShaderFile().empty()) m["shaderFile"] = cm->GetShaderFile();
				else m["shaderSource"] = cm->GetShaderObject()->GetShaderText();
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
		// Decal IS-A Model (Decal : public Model) - must be checked
		// first, or it'd fall into the generic Model branch below and be
		// rejected there (a Decal never has a real GetPath(), it's built
		// straight from an in-memory vertex list, not a file). DecalGeometry
		// (the projector that builds a Decal) has zero getters and
		// consumes a specific target RenderingMesh* with no stable
		// reference to re-find at load time - instead of that fragile
		// path, serialize the already-baked Decal's own vertex data
		// directly (same "inline the final vertices" pattern already
		// used for TriangleMesh/ConvexTriangleMesh physics shapes).
		// haveBones is inferred from whether tBonesID/tBonesWeight were
		// actually populated (Decal::Decal only fills them when its own
		// haveBones ctor arg was true) - no stored flag needed.
		if (Decal* decal = dynamic_cast<Decal*>(r))
		{
			if (decal->Geometries.empty()) { echo("WARNING: SceneSerializer - skipping an empty Decal"); return json(); }
			ModelGeometry* geom = dynamic_cast<ModelGeometry*>(decal->Geometries[0]);
			if (!geom) { echo("WARNING: SceneSerializer - skipping a Decal with an unexpected geometry type"); return json(); }
			bool haveBones = geom->tBonesID.size() == geom->tVertex.size() && !geom->tVertex.empty();
			j["kind"] = "decal";
			json verts = json::array();
			for (size_t i = 0; i < geom->tVertex.size(); i++)
			{
				json vj;
				vj["vertex"] = ToJson(geom->tVertex[i]);
				vj["normal"] = ToJson(geom->tNormal[i]);
				vj["uv"] = json::array({ geom->tTexcoord[i].x, geom->tTexcoord[i].y });
				if (haveBones)
				{
					vj["bonesID"] = ToJson(geom->tBonesID[i]);
					vj["bonesWeight"] = ToJson(geom->tBonesWeight[i]);
				}
				verts.push_back(vj);
			}
			j["vertices"] = verts;
			j["haveBones"] = haveBones;
			return j;
		}
		if (Text* text = dynamic_cast<Text*>(r))
		{
			if (!text->GetFont() || text->GetFont()->GetPath().empty())
			{
				echo("WARNING: SceneSerializer - skipping a Text with no recoverable font path");
				return json();
			}
			j["kind"] = "text";
			j["font"] = text->GetFont()->GetPath();
			j["fontSize"] = text->GetFont()->GetFontSize();
			j["text"] = text->GetText();
			j["charWidth"] = text->GetCharWidth();
			j["charHeight"] = text->GetCharHeight();
			if (!text->GetCharColors().empty())
			{
				json colors = json::array();
				for (size_t i = 0; i < text->GetCharColors().size(); i++) colors.push_back(ToJson(text->GetCharColors()[i]));
				j["charColors"] = colors;
			}
			else
			{
				j["color"] = ToJson(text->GetColor());
			}
			return j;
		}
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

	// Extracted so PhysicsVehicle's chassis (an orphan IPhysicsComponent*,
	// never itself added to a GameObject - see the Vehicle case below) can
	// reuse the exact same shape-dispatch logic a tree-attached Physics
	// component uses, nested under "chassis" instead of written as its
	// own top-level component.
	static json SerializePhysicsShape(IPhysicsComponent* pc)
	{
		json j;
		if (!pc) return json();
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
			// HeightFieldTerrain/Ghost-as-shape - not emitted, see
			// VULKAN_ROADMAP.md's non-goals list (no working
			// HeightFieldTerrain implementation exists anywhere in the
			// engine to serialize in the first place).
			echo("WARNING: SceneSerializer - skipping a physics component with an unsupported shape type");
			return json();
		}
	}

	static json SerializeComponent(IComponent* c, json &materialsArray, std::map<IMaterial*, uint32> &materialIdMap, sol::state* lua)
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

			// Optional skeleton animation, only present if
			// SetActiveSkeletonAnimation() was ever called for this
			// component (automatic, from SkeletonAnimationInstance's own
			// constructor - see RenderingComponent.h's comment). Zero
			// effect on components that don't use skeleton animation.
			if (SkeletonAnimationInstance* inst = static_cast<SkeletonAnimationInstance*>(rc->GetActiveSkeletonAnimation()))
			{
				json sa;
				sa["path"] = inst->GetOwner()->GetPath();
				json playing = json::array();
				for (uint32 order = 0; order < inst->GetNumberPlayingAnimations(); order++)
				{
					json pj;
					pj["id"] = inst->GetAnimationID(order);
					pj["startTimeProgress"] = inst->GetAnimationStartTimeProgress(order);
					pj["currentTime"] = inst->GetAnimationCurrentTime(order);
					pj["speed"] = inst->GetAnimationSpeed(order);
					pj["scale"] = inst->GetAnimationScale(order);
					pj["paused"] = inst->IsPaused(order);
					std::string layer = inst->GetAnimationLayerName(order);
					if (!layer.empty()) pj["layer"] = layer;
					playing.push_back(pj);
				}
				sa["playing"] = playing;
				json layers = json::array();
				for (uint32 li = 0; li < inst->GetNumberLayers(); li++)
				{
					json lj;
					lj["name"] = inst->GetLayerName(li);
					json bones = json::array();
					const std::map<StringID, Bone> &skel = rc->GetSkeleton();
					for (int32 boneID : inst->GetLayerAffectedBoneIDs(li))
					{
						for (std::map<StringID, Bone>::const_iterator bi = skel.begin(); bi != skel.end(); ++bi)
							if (bi->second.self == boneID) { bones.push_back(bi->second.name); break; }
					}
					lj["bones"] = bones;
					layers.push_back(lj);
				}
				sa["layers"] = layers;
				j["skeletonAnimation"] = sa;
			}

			// Optional texture animation - opt-in only (see
			// RenderingComponent.h's SetActiveTextureAnimation comment -
			// unlike skeleton animation there's no automatic engine-level
			// link, a caller must have called this explicitly).
			if (TextureAnimationInstance* tinst = static_cast<TextureAnimationInstance*>(rc->GetActiveTextureAnimation()))
			{
				json ta;
				TextureAnimation* owner = tinst->GetOwner();
				json frames = json::array();
				bool allFramesRecoverable = true;
				for (uint32 i = 0; i < owner->GetNumberFrames(); i++)
				{
					Texture* f = owner->GetFrame(i);
					json fj;
					SerializeTextureRef(fj, "tex", f);
					if (fj.find("tex") == fj.end() && fj.find("texData") == fj.end()) { allFramesRecoverable = false; break; }
					frames.push_back(fj);
				}
				if (allFramesRecoverable && !frames.empty())
				{
					ta["frames"] = frames;
					ta["fps"] = tinst->GetFrameSpeed();
					ta["paused"] = tinst->IsPaused();
					ta["looping"] = tinst->IsLooping();
					ta["yoyo"] = tinst->IsYoyo();
					ta["repeat"] = tinst->GetRepeat();
					ta["reverse"] = tinst->IsReverse();
					ta["currentFrame"] = tinst->GetFrame();
					j["textureAnimation"] = ta;
				}
				else echo("WARNING: SceneSerializer - skipping a RenderingComponent's texture animation, one or more frame textures has no recoverable source");
			}
			return j;
		}
		case ComponentType::ParticleSystem:
		{
			ParticleSystem* ps = dynamic_cast<ParticleSystem*>(c);
			const ParticleSystemDesc &d = ps->GetDesc();
			j["type"] = "ParticleSystem";
			j["maxParticles"] = d.maxParticles;
			SerializeTextureRef(j, "texture", d.texture);
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
			return SerializePhysicsShape(dynamic_cast<IPhysicsComponent*>(c));
		case ComponentType::Vehicle:
		{
			PhysicsVehicle* v = dynamic_cast<PhysicsVehicle*>(c);
			json chassisJson = SerializePhysicsShape(v->GetChassis());
			if (chassisJson.is_null())
			{
				echo("WARNING: SceneSerializer - skipping a Vehicle, its chassis shape couldn't be serialized");
				return json();
			}
			j["type"] = "Vehicle";
			j["ghost"] = v->IsGhost();
			j["chassis"] = chassisJson;
			j["rightIndex"] = v->GetRightIndex(); j["upIndex"] = v->GetUpIndex(); j["forwardIndex"] = v->GetForwardIndex();
			j["maxProxies"] = v->GetMaxProxies(); j["maxOverlap"] = v->GetMaxOverlap();
			j["engineForce"] = v->GetEngineForce(); j["breakingForce"] = v->GetBreakingForce();
			j["maxEngineForce"] = v->GetMaxEngineForce(); j["maxBreakingForce"] = v->GetMaxBreakingForce();
			j["vehicleSteering"] = v->GetVehicleSteering(); j["steeringIncrement"] = v->GetSteeringIncrement(); j["steeringClamp"] = v->GetSteeringClamp();
			j["suspensionStiffness"] = v->GetSuspensionStiffness(); j["suspensionDamping"] = v->GetSuspensionDamping();
			j["suspensionCompression"] = v->GetSuspensionCompression(); j["suspensionRestLength"] = v->GetSuspensionRestLength();
			json wheels = json::array();
			for (size_t i = 0; i < v->GetWheels().size(); i++)
			{
				const VehicleWheel &w = v->GetWheels()[i];
				json wj;
				wj["direction"] = ToJson(w.Direction); wj["axle"] = ToJson(w.Axle);
				wj["radius"] = w.Radius; wj["width"] = w.Width; wj["friction"] = w.Friction; wj["rollInfluence"] = w.RollInfluence;
				wj["position"] = ToJson(w.Position); wj["isFrontWheel"] = w.IsFrontWheel;
				wheels.push_back(wj);
			}
			j["wheels"] = wheels;
			return j;
		}
#ifdef LUA_BINDINGS
		case ComponentType::LuaComponent:
		{
			LuaComponent* lc = dynamic_cast<LuaComponent*>(c);
			j["type"] = "LuaComponent";
			// Real behavior only for components built via
			// GameObject:attachScript()/LuaComponent_fromFile() (non-empty
			// scriptFile) whose class defines a real serialize() method -
			// anything else (ad-hoc on_init/on_update closures) round-trips
			// as existence-only, same as before.
			if (!lc->scriptFile.empty() && lc->data.valid())
			{
				sol::function serializeFn = lc->data["serialize"];
				if (serializeFn.valid())
				{
					sol::protected_function_result result = serializeFn(lc->data);
					if (result.valid() && result.get_type() == sol::type::table)
					{
						j["scriptFile"] = lc->scriptFile;
						j["data"] = LuaTableToJson(result.get<sol::table>());
					}
					else echo("WARNING: SceneSerializer - a LuaComponent's serialize() didn't return a table, saving as existence-only");
				}
				else echo("WARNING: SceneSerializer - a LuaComponent's class has no serialize() method, saving as existence-only");
			}
			return j;
		}
#endif
		default:
			return json();
		}
	}

	static json SerializeGameObject(GameObject* go, json &materialsArray, std::map<IMaterial*, uint32> &materialIdMap, sol::state* lua)
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
			json cj = SerializeComponent(comps[i], materialsArray, materialIdMap, lua);
			if (!cj.is_null()) components.push_back(cj);
		}
		j["components"] = components;

		json children = json::array();
		const std::vector<GameObject*> &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			children.push_back(SerializeGameObject(kids[i], materialsArray, materialIdMap, lua));
		j["children"] = children;

		return j;
	}

	bool SceneSerializer::SaveScene(SceneGraph* scene, const std::string &filePath, sol::state* lua)
	{
		json root;
		root["version"] = 1;

		json materialsArray = json::array();
		std::map<IMaterial*, uint32> materialIdMap;

		json roots = json::array();
		std::vector<GameObject*> &all = scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); i++)
			roots.push_back(SerializeGameObject(all[i], materialsArray, materialIdMap, lua));

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

	static Texture* DeserializeTextureRef(const json &parent, const std::string &key, std::map<std::string, Texture*> &textureCache)
	{
		if (parent.find(key) != parent.end())
			return GetOrLoadTexture(parent[key].get<std::string>(), textureCache);
		std::string dataKey = key + "Data";
		if (parent.find(dataKey) != parent.end())
		{
			// No natural dedup key for embedded data (unlike paths) -
			// always creates a fresh Texture.
			std::vector<uchar> bytes = Base64Decode(parent[dataKey].get<std::string>());
			Texture* tex = new Texture();
			tex->LoadTextureFromMemory(bytes, (uint32)bytes.size(), TextureType::Texture);
			return tex;
		}
		return NULL;
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
			if (Texture* t = DeserializeTextureRef(j, "colorMap", textureCache)) gm->SetColorMap(t);
			if (Texture* t = DeserializeTextureRef(j, "specularMap", textureCache)) gm->SetSpecularMap(t);
			if (Texture* t = DeserializeTextureRef(j, "normalMap", textureCache)) gm->SetNormalMap(t);
			if (Texture* t = DeserializeTextureRef(j, "displacementMap", textureCache)) gm->SetDisplacementMap(t);
			if (Texture* t = DeserializeTextureRef(j, "envMap", textureCache)) gm->SetEnvMap(t);
			if (Texture* t = DeserializeTextureRef(j, "refractMap", textureCache)) gm->SetRefractMap(t);
			if (Texture* t = DeserializeTextureRef(j, "skyboxMap", textureCache)) gm->SetSkyboxMap(t);
			if (Texture* t = DeserializeTextureRef(j, "metallicRoughnessMap", textureCache)) gm->SetMetallicRoughnessMap(t);
			ApplyCommonMaterialFields(gm, j);
			return gm;
		}
		else if (kind == "custom")
		{
			CustomShaderMaterial* cm;
			if (j.find("shaderFile") != j.end())
				cm = new CustomShaderMaterial(j.value("shaderFile", std::string()));
			else if (j.find("shaderSource") != j.end())
			{
				// Mirrors CustomShaderMaterial's real file-based
				// constructor (CustomShaderMaterial.cpp) - same platform
				// #defines, since this compiles into the same target and
				// shares them. Known minor leak: the Shader* constructor
				// overload doesn't take ownership (only the file-path
				// ctor's InternalShader does) - acceptable for this rare
				// fallback path (a material with no recoverable file at
				// all), not worth a deeper CustomShaderMaterial ownership
				// change for.
				Shader* s = new Shader();
				std::string define;
#if defined(GLES3)
				define += std::string("#define GLES3\n");
#endif
#if defined(GLES2_DESKTOP)
				define += std::string("#define GLES2_DESKTOP\n");
#endif
#if defined(GLES3_DESKTOP)
				define += std::string("#define GLES3_DESKTOP\n");
#endif
#if defined(GLLEGACY)
				define += std::string("#define GLLEGACY\n");
#endif
#if defined(EMSCRIPTEN)
				define += std::string("#define EMSCRIPTEN\n");
#endif
				s->LoadShaderText(j["shaderSource"].get<std::string>());
				s->CompileShader(ShaderType::VertexShader, std::string("#define VERTEX\n") + define);
				s->CompileShader(ShaderType::FragmentShader, std::string("#define FRAGMENT\n") + define);
				s->LinkProgram();
				cm = new CustomShaderMaterial(s);
			}
			else
			{
				echo("WARNING: SceneSerializer - skipping a CustomShaderMaterial entry with neither shaderFile nor shaderSource");
				return NULL;
			}
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
		if (kind == "text")
		{
			// No font pooling/dedup - each loaded Text gets its own Font
			// instance, matching what direct C++ construction would do
			// anyway; out of scope to optimize here.
			Font* font = new Font(j.value("font", std::string()), j.value("fontSize", 16.0f));
			std::string text = j.value("text", std::string());
			f32 charWidth = j.value("charWidth", 1.0f);
			f32 charHeight = j.value("charHeight", 1.0f);
			if (j.find("charColors") != j.end())
			{
				std::vector<Vec4> colors;
				for (auto &cj : j["charColors"]) colors.push_back(Vec4FromJson(cj));
				return new Text(font, text, charWidth, charHeight, colors);
			}
			Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
			return new Text(font, text, charWidth, charHeight, color);
		}
		if (kind == "decal")
		{
			bool haveBones = j.value("haveBones", false);
			std::vector<DecalVertex> verts;
			if (j.find("vertices") != j.end())
			{
				for (auto &vj : j["vertices"])
				{
					DecalVertex dv;
					dv.vertex = vj.find("vertex") != vj.end() ? Vec3FromJson(vj["vertex"]) : Vec3();
					dv.normal = vj.find("normal") != vj.end() ? Vec3FromJson(vj["normal"]) : Vec3();
					if (vj.find("uv") != vj.end()) dv.uv = Vec2(vj["uv"][0].get<f32>(), vj["uv"][1].get<f32>());
					if (haveBones)
					{
						if (vj.find("bonesID") != vj.end()) dv.bonesID = Vec4FromJson(vj["bonesID"]);
						if (vj.find("bonesWeight") != vj.end()) dv.bonesWeight = Vec4FromJson(vj["bonesWeight"]);
					}
					verts.push_back(dv);
				}
			}
			return new Decal(verts, haveBones);
		}
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

	// Mirrors SerializePhysicsShape - reused by both the flat "Physics"
	// component case and Vehicle's nested "chassis" (an orphan
	// IPhysicsComponent* the vehicle owns directly, not a tree-attached
	// component).
	static IPhysicsComponent* DeserializePhysicsShape(const json &j, IPhysics* physics)
	{
		if (!physics) { echo("WARNING: SceneSerializer - can't rebuild a physics shape, LoadScene() was called with physics == NULL"); return NULL; }
		std::string shape = j.value("shape", "");
		f32 mass = j.value("mass", 0.0f);
		bool ghost = j.value("ghost", false);
		if (shape == "Box") return physics->CreateBox(j.value("width", 1.0f), j.value("height", 1.0f), j.value("depth", 1.0f), mass, ghost);
		if (shape == "Sphere") return physics->CreateSphere(j.value("radius", 1.0f), mass, ghost);
		if (shape == "Capsule") return physics->CreateCapsule(j.value("radius", 1.0f), j.value("height", 1.0f), mass, ghost);
		if (shape == "Cone") return physics->CreateCone(j.value("radius", 1.0f), j.value("height", 1.0f), mass, ghost);
		if (shape == "Cylinder") return physics->CreateCylinder(j.value("radius", 1.0f), j.value("height", 1.0f), mass, ghost);
		if (shape == "StaticPlane") return physics->CreateStaticPlane((j.find("normal") != j.end()) ? Vec3FromJson(j["normal"]) : Vec3(0, 1, 0), j.value("constant", 0.0f), mass, ghost);
		if (shape == "ConvexHull")
		{
			std::vector<Vec3> pts;
			if (j.find("points") != j.end()) for (auto &p : j["points"]) pts.push_back(Vec3FromJson(p));
			return physics->CreateConvexHull(pts, mass, ghost);
		}
		if (shape == "ConvexTriangleMesh" || shape == "TriangleMesh")
		{
			std::vector<uint32> idx;
			std::vector<Vec3> vtx;
			if (j.find("indices") != j.end()) for (auto &i : j["indices"]) idx.push_back(i.get<uint32>());
			if (j.find("vertices") != j.end()) for (auto &v : j["vertices"]) vtx.push_back(Vec3FromJson(v));
			return (shape == "ConvexTriangleMesh") ? physics->CreateConvexTriangleMesh(idx, vtx, mass, ghost) : physics->CreateTriangleMesh(idx, vtx, mass, ghost);
		}
		if (shape == "MultipleSphere")
		{
			std::vector<Vec3> pos;
			std::vector<f32> rad;
			if (j.find("positions") != j.end()) for (auto &p : j["positions"]) pos.push_back(Vec3FromJson(p));
			if (j.find("radii") != j.end()) for (auto &r : j["radii"]) rad.push_back(r.get<f32>());
			return physics->CreateMultipleSphere(pos, rad, mass, ghost);
		}
		echo("WARNING: SceneSerializer - skipping a physics component with an unsupported shape on load: " + shape);
		return NULL;
	}

	static void DeserializeComponent(GameObject* go, const json &j, const std::vector<IMaterial*> &materialsById, std::map<std::string, Texture*> &textureCache, IPhysics* physics, sol::state* lua)
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

			if (j.find("skeletonAnimation") != j.end())
			{
				const json &sa = j["skeletonAnimation"];
				std::string path = sa.value("path", std::string());
				if (!path.empty())
				{
					SkeletonAnimation* anim = new SkeletonAnimation();
					anim->LoadAnimation(path);
					SkeletonAnimationInstance* inst = anim->CreateInstance(rc);
					if (sa.find("layers") != sa.end())
					{
						for (auto &lj : sa["layers"])
						{
							std::string layerName = lj.value("name", std::string());
							if (layerName.empty()) continue;
							uint32 layerID = inst->CreateLayer(layerName);
							if (lj.find("bones") != lj.end())
								for (auto &bj : lj["bones"]) inst->AddBone(layerID, bj.get<std::string>());
						}
					}
					if (sa.find("playing") != sa.end())
					{
						for (auto &pj : sa["playing"])
						{
							uint32 id = pj.value("id", 0u);
							std::string layer = pj.value("layer", std::string());
							// Play() has no way to seek to an exact mid-playback
							// _currentTime (no setter exists) - restores the
							// same start progress/speed/scale/pause state,
							// not the precise frame it was saved at. Known,
							// minor limitation.
							int32 order = inst->Play(id, pj.value("startTimeProgress", 0.0f), 1.0f, pj.value("speed", 1.0f), pj.value("scale", 1.0f), layer);
							if (order >= 0)
							{
								if (pj.value("paused", false)) inst->PauseAnimation((uint32)order);
							}
						}
					}
				}
				else echo("WARNING: SceneSerializer - skipping a RenderingComponent's skeleton animation, no recoverable asset path");
			}

			if (j.find("textureAnimation") != j.end())
			{
				const json &ta = j["textureAnimation"];
				if (ta.find("frames") != ta.end() && !ta["frames"].empty())
				{
					TextureAnimation* anim = new TextureAnimation();
					for (auto &fj : ta["frames"]) anim->AddFrame(DeserializeTextureRef(fj, "tex", textureCache));
					TextureAnimationInstance* tinst = anim->CreateInstance(ta.value("fps", 30.0f));
					if (ta.value("reverse", false)) tinst->Reverse(true);
					if (ta.value("yoyo", false)) tinst->YoYo(true);
					// No seek/SetFrame API exists - restored playback
					// starts from frame 0, not the exact saved
					// currentFrame. Same class of limitation as skeleton
					// animation's currentTime.
					tinst->Play(ta.value("repeat", 1));
					if (ta.value("paused", false)) tinst->Pause();
					rc->SetActiveTextureAnimation(tinst);
				}
			}
		}
		else if (type == "ParticleSystem")
		{
			ParticleSystemDesc d;
			d.maxParticles = j.value("maxParticles", d.maxParticles);
			if (Texture* t = DeserializeTextureRef(j, "texture", textureCache)) d.texture = t;
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
			IPhysicsComponent* pc = DeserializePhysicsShape(j, physics);
			if (pc) go->AddComponent(pc);
		}
		else if (type == "Vehicle")
		{
			if (!physics) { echo("WARNING: SceneSerializer - skipping a Vehicle, LoadScene() was called with physics == NULL"); return; }
			IPhysicsComponent* chassis = (j.find("chassis") != j.end()) ? DeserializePhysicsShape(j["chassis"], physics) : NULL;
			if (!chassis) { echo("WARNING: SceneSerializer - skipping a Vehicle, its chassis couldn't be rebuilt"); return; }
			IPhysicsComponent* vc = physics->CreateVehicle(chassis, j.value("ghost", false));
			PhysicsVehicle* v = dynamic_cast<PhysicsVehicle*>(vc);
			if (!v) { echo("WARNING: SceneSerializer - CreateVehicle() didn't return a PhysicsVehicle"); return; }
			v->SetMaxProxies(j.value("maxProxies", 1u)); v->SetMaxOverlap(j.value("maxOverlap", 1u));
			v->SetEngineForce(j.value("engineForce", 0.0f)); v->SetBreakingForce(j.value("breakingForce", 0.0f));
			v->SetMaxEngineForce(j.value("maxEngineForce", 1000.0f)); v->SetMaxBreakingForce(j.value("maxBreakingForce", 100.0f));
			v->SetVehicleSteering(j.value("vehicleSteering", 0.0f)); v->SetSteeringIncrement(j.value("steeringIncrement", 0.04f)); v->SetSteeringClamp(j.value("steeringClamp", 0.3f));
			v->SetSuspensionStiffness(j.value("suspensionStiffness", 20.0f)); v->SetSuspensionDamping(j.value("suspensionDamping", 2.3f));
			v->SetSuspensionCompression(j.value("suspensionCompression", 4.4f)); v->SetSuspensionRestLength(j.value("suspensionRestLength", 0.6f));
			if ((j.find("wheels") != j.end()))
			{
				for (auto &wj : j["wheels"])
				{
					v->AddWheel(
						(wj.find("direction") != wj.end()) ? Vec3FromJson(wj["direction"]) : Vec3(0, -1, 0),
						(wj.find("axle") != wj.end()) ? Vec3FromJson(wj["axle"]) : Vec3(-1, 0, 0),
						wj.value("radius", 0.5f), wj.value("width", 0.4f), wj.value("friction", 1.0f), wj.value("rollInfluence", 0.1f),
						(wj.find("position") != wj.end()) ? Vec3FromJson(wj["position"]) : Vec3(0, 0, 0),
						wj.value("isFrontWheel", false));
				}
			}
			go->AddComponent(v);
		}
#ifdef LUA_BINDINGS
		else if (type == "LuaComponent")
		{
			std::string scriptFile = j.value("scriptFile", std::string());
			if (!scriptFile.empty() && lua && j.find("data") != j.end())
			{
				sol::object result = lua->require_file(scriptFile, scriptFile);
				if (result.valid() && result.get_type() == sol::type::table)
				{
					sol::table cls = result;
					sol::function deserializeFn = cls["deserialize"];
					if (deserializeFn.valid())
					{
						sol::table dataTable = JsonToLuaTable(*lua, j["data"]);
						sol::protected_function_result instResult = deserializeFn(dataTable);
						if (instResult.valid() && instResult.get_type() == sol::type::table)
						{
							LuaComponent* comp = new LuaComponent();
							comp->scriptFile = scriptFile;
							comp->data = instResult.get<sol::table>();
							go->AddComponent(comp);
						}
						else echo("WARNING: SceneSerializer - a LuaComponent's deserialize() didn't return a table, adding as existence-only");
					}
					else
					{
						echo("WARNING: SceneSerializer - a LuaComponent's class has no deserialize() method, adding as existence-only");
						go->AddComponent(new LuaComponent());
					}
				}
				else
				{
					echo("WARNING: SceneSerializer - couldn't load LuaComponent's scriptFile: " + scriptFile + ", adding as existence-only");
					go->AddComponent(new LuaComponent());
				}
			}
			else
			{
				// Existence only, no behavior - either an ad-hoc
				// component (no scriptFile was ever saved) or LoadScene()
				// was called with lua == NULL.
				go->AddComponent(new LuaComponent());
			}
		}
#endif
	}

	static GameObject* DeserializeGameObject(const json &j, const std::vector<IMaterial*> &materialsById, std::map<std::string, Texture*> &textureCache, IPhysics* physics, sol::state* lua)
	{
		GameObject* go = new GameObject(j.value("static", false));
		go->SetName(j.value("name", std::string()));
		if ((j.find("position") != j.end())) go->SetPosition(Vec3FromJson(j["position"]));
		if ((j.find("rotation") != j.end())) go->SetRotation(Vec3FromJson(j["rotation"]));
		if ((j.find("scale") != j.end())) go->SetScale(Vec3FromJson(j["scale"]));
		if ((j.find("tags") != j.end())) for (auto &t : j["tags"]) go->AddTag(t.get<std::string>());

		if ((j.find("components") != j.end())) for (auto &cj : j["components"]) DeserializeComponent(go, cj, materialsById, textureCache, physics, lua);

		if ((j.find("children") != j.end()))
			for (auto &cj : j["children"])
			{
				GameObject* child = DeserializeGameObject(cj, materialsById, textureCache, physics, lua);
				go->Add(child);
			}

		return go;
	}

	bool SceneSerializer::LoadScene(SceneGraph* scene, const std::string &filePath, IPhysics* physics, sol::state* lua)
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
				scene->Add(DeserializeGameObject(rj, materialsById, textureCache, physics, lua));

		return true;
	}

}
