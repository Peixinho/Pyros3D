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
#include <Pyros3D/Rendering/Device/IRenderDevice.h>

#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Audio/AudioSource.h>
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
#include <filesystem>

namespace p3d {

	using json = nlohmann::json;
	namespace fs = std::filesystem;

	// Demo JSON files were often saved with absolute host paths
	// (/Users/.../examples/assets/...). Remap those (and plain relative
	// paths) against ASSETS_PATH when the recorded file is missing.
	// Editor projects use the folder that contains scenes/ as the root.
	static std::string g_sceneAssetRoot;

	static std::string NormalizeSlashes(std::string p)
	{
		for (size_t i = 0; i < p.size(); ++i)
			if (p[i] == '\\') p[i] = '/';
		return p;
	}

	static std::string EnsureTrailingSlash(std::string p)
	{
		p = NormalizeSlashes(p);
		if (!p.empty() && p.back() != '/') p.push_back('/');
		return p;
	}

	static std::string InferAssetRootFromScenePath(const std::string &filePath)
	{
		fs::path sp(filePath);
		std::error_code ec;
		sp = fs::weakly_canonical(sp, ec);
		if (ec) sp = fs::path(filePath);
		// <project>/scenes/<file>.json → <project>/
		if (sp.parent_path().filename() == "scenes")
			return EnsureTrailingSlash(sp.parent_path().parent_path().string());
		return std::string();
	}

	static bool SceneAssetFileExists(const std::string &path)
	{
		if (path.empty()) return false;
		std::ifstream in(path.c_str());
		return in.good();
	}

	static std::string RelativizeSceneAssetPath(const std::string &path)
	{
		if (path.empty() || g_sceneAssetRoot.empty()) return path;
		const std::string root = EnsureTrailingSlash(g_sceneAssetRoot);
		const std::string norm = NormalizeSlashes(path);
		const std::string rootNorm = NormalizeSlashes(root);
		if (norm.size() >= rootNorm.size()
			&& norm.compare(0, rootNorm.size(), rootNorm) == 0)
			return norm.substr(rootNorm.size());

		// Not under this project's root, but still a project-shaped path -
		// an asset recorded by another checkout, or by this one before the
		// project folder moved. A literal prefix compare is the only thing
		// this used to do, so such a path was copied straight back out with
		// somebody's home directory baked into it, and the scene stayed
		// pinned to one machine forever. Store it from the marker onward
		// instead - but only once the marker-relative form actually resolves
		// under the current root, because rewriting a path we cannot find
		// would turn a working absolute reference into a broken relative one.
		static const char* kMarkers[] = {
			"assets/models/", "assets/textures/", "assets/sounds/",
			"assets/shaders/", "assets/lua/", "assets/materials/",
			"assets/", "scenes/"
		};
		for (size_t mi = 0; mi < sizeof(kMarkers) / sizeof(kMarkers[0]); ++mi)
		{
			const size_t pos = norm.find(kMarkers[mi]);
			if (pos == std::string::npos) continue;
			const std::string relative = norm.substr(pos);
			if (SceneAssetFileExists(rootNorm + relative))
				return relative;
			break;
		}
		return path;
	}

	static std::string ResolveSceneAssetPath(const std::string &path)
	{
		if (path.empty()) return path;
		if (SceneAssetFileExists(path)) return path;

		std::string relative = NormalizeSlashes(path);

		// Legacy demos: strip host prefix through examples/assets/ so
		// ASSETS_PATH (the assets folder itself) + remainder still works.
		{
			const std::string marker = "examples/assets/";
			const size_t pos = relative.find(marker);
			if (pos != std::string::npos)
			{
				relative = relative.substr(pos + marker.size());
				if (!g_sceneAssetRoot.empty())
				{
					const std::string candidate = EnsureTrailingSlash(g_sceneAssetRoot) + relative;
					if (SceneAssetFileExists(candidate) || relative != NormalizeSlashes(path))
						return candidate;
				}
			}
		}

		// Editor projects: keep paths relative to the project root
		// (assets/models/..., scenes/...).
		const std::string projectMarkers[] = {
			"assets/models/", "assets/textures/", "assets/sounds/",
			"assets/shaders/", "assets/lua/", "assets/materials/",
			"assets/", "scenes/"
		};
		for (size_t mi = 0; mi < sizeof(projectMarkers) / sizeof(projectMarkers[0]); ++mi)
		{
			const size_t pos = relative.find(projectMarkers[mi]);
			if (pos != std::string::npos)
			{
				relative = relative.substr(pos);
				break;
			}
		}

		if (!g_sceneAssetRoot.empty())
		{
			const std::string root = EnsureTrailingSlash(g_sceneAssetRoot);
			const std::string joined = root + relative;
			if (SceneAssetFileExists(joined))
				return joined;

			// ASSETS_PATH may be .../assets/ while the path already starts with
			// assets/ (editor project layout). Try the project root once.
			std::string rootNorm = NormalizeSlashes(root);
			while (!rootNorm.empty() && rootNorm.back() == '/') rootNorm.pop_back();
			const bool rootIsAssetsFolder =
				rootNorm.size() >= 6
				&& (rootNorm.compare(rootNorm.size() - 6, 6, "assets") == 0);
			if (rootIsAssetsFolder && relative.find("assets/") == 0)
			{
				const std::string viaProject = EnsureTrailingSlash(fs::path(rootNorm).parent_path().string()) + relative;
				if (SceneAssetFileExists(viaProject))
					return viaProject;
			}

			if (!fs::path(path).is_absolute())
				return joined;
			if (relative != NormalizeSlashes(path)
				&& (relative.find("assets/") == 0 || relative.find("scenes/") == 0))
				return joined;
		}
		return path;
	}

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
		if (!t->GetFilename().empty()) { parent[key] = RelativizeSceneAssetPath(t->GetFilename()); return; }
		if (!t->GetRawData().empty()) { parent[key + "Data"] = Base64Encode(t->GetRawData()); return; }
	}
	// Mirrors SerializeTextureRef - resolves either key, dedupes
	// path-based loads via textureCache (embedded-data loads always
	// create a fresh Texture, no natural dedup key). `outAssets`, if
	// non-NULL, records every Texture actually constructed (not cache
	// hits) - see LoadedSceneAssets.
	static std::shared_ptr<Texture> DeserializeTextureRef(const json &parent, const std::string &key, std::map<std::string, std::shared_ptr<Texture>> &textureCache, LoadedSceneAssets* outAssets);

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
			m["alphaCutoff"] = gm->GetAlphaCutoff();
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

				// The shader alone is not the whole material: every texture
				// bound through AddSampler() used to be dropped here, so a
				// Material Editor material that samples e.g. uAlbedoTex came
				// back from a saved scene with that sampler unbound. In
				// Forward the per-fragment lighting still varied, which
				// disguised it; in Deferred the object shader only writes
				// albedo/normal to the G-buffer, so a constant albedo made
				// the whole mesh render as one flat colour.
				const std::vector<std::string> &names = cm->GetSamplerNames();
				if (!names.empty())
				{
					json samplers = json::array();
					for (size_t s = 0; s < names.size() && s < cm->textures.size(); s++)
					{
						json entry;
						entry["name"] = names[s];
						SerializeTextureRef(entry, "texture", cm->textures[s].get());
						// Skip a sampler whose texture has neither a file nor
						// raw data - nothing to restore, and an entry with
						// only a name would rebind unit indices on load.
						if (entry.find("texture") == entry.end() && entry.find("textureData") == entry.end()) continue;
						samplers.push_back(entry);
					}
					if (!samplers.empty()) m["samplers"] = samplers;
				}
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
			j["path"] = RelativizeSceneAssetPath(model->GetPath());
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

// Volumetric scattering round-trips for point and spot lights - see
// ILightComponent::SetVolumetricScattering(). Written unconditionally so a
// saved scene always states it; read only when present, so every existing
// scene keeps the 0 default and behaves exactly as before.
static void WriteVolumetric(json &j, ILightComponent *l)
{
	j["volumetricScattering"] = l->GetVolumetricScattering();
	j["volumetricAnisotropy"] = l->GetVolumetricAnisotropy();
	j["volumetricSteps"] = l->GetVolumetricSteps();
}

static void ReadVolumetric(const json &j, ILightComponent *l)
{
	if (j.find("volumetricScattering") != j.end())
		l->SetVolumetricScattering(j.value("volumetricScattering", 0.0f));
	if (j.find("volumetricAnisotropy") != j.end())
		l->SetVolumetricAnisotropy(j.value("volumetricAnisotropy", 0.6f));
	if (j.find("volumetricSteps") != j.end())
		l->SetVolumetricSteps(j.value("volumetricSteps", 32u));
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
			IMaterial* mat = rc->GetMeshes(0)[0]->Material.get();
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
				// Both: "path" stays for older scene files / single-file
				// animations, "paths" is the real one - a SkeletonAnimation
				// concatenates every LoadAnimation() file into one clip list
				// and a saved Play(id) indexes into that concatenation, so
				// dropping all but the last path made those ids meaningless.
				sa["path"] = RelativizeSceneAssetPath(inst->GetOwner()->GetPath());
				json pathsArr = json::array();
				const std::vector<std::string> &paths = inst->GetOwner()->GetPaths();
				for (size_t pi = 0; pi < paths.size(); ++pi)
					pathsArr.push_back(RelativizeSceneAssetPath(paths[pi]));
				sa["paths"] = pathsArr;
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
			SerializeTextureRef(j, "texture", d.texture.get());
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
		case ComponentType::AudioSource:
		{
			AudioSource* a = dynamic_cast<AudioSource*>(c);
			j["type"] = "AudioSource";
			// Same project-relative form every other asset reference is
			// stored in (models, textures, particle sprites). This one field
			// used to be written out as whatever absolute path the file was
			// imported from, which pinned the scene to one machine's home
			// directory and broke as soon as the project moved.
			j["file"] = RelativizeSceneAssetPath(a->GetFile());
			j["stream"] = a->IsStreamed();
			j["playing"] = a->IsPlaying();
			j["looping"] = a->IsLooping();
			j["volume"] = a->GetVolume();
			j["pitch"] = a->GetPitch();
			j["spatialized"] = a->IsSpatialized();
			j["attenuationModel"] = a->GetAttenuationModel();
			j["minDistance"] = a->GetMinDistance();
			j["maxDistance"] = a->GetMaxDistance();
			j["directionalAttenuation"] = a->GetDirectionalAttenuation();
			j["dopplerFactor"] = a->GetDopplerFactor();
			j["pan"] = a->GetPan();
			// Only written when actually set, so a plain omnidirectional
			// source doesn't carry meaningless cone angles around.
			if (a->HasCone())
			{
				j["coneInnerAngle"] = a->GetConeInnerAngle();
				j["coneOuterAngle"] = a->GetConeOuterAngle();
				j["coneOuterGain"] = a->GetConeOuterGain();
			}
			// Same "only if actually set" reasoning for the filter/EQ/delay -
			// most sources have none, and their other fields are meaningless
			// without a type (or, for delay, without being active at all) to
			// go with them.
			if (a->GetFilterType() != AudioFilterType::None)
			{
				j["filterType"] = a->GetFilterType();
				j["filterCutoff"] = a->GetFilterCutoff();
				j["filterOrder"] = a->GetFilterOrder();
			}
			if (a->GetEQType() != AudioEQType::None)
			{
				j["eqType"] = a->GetEQType();
				j["eqFrequency"] = a->GetEQFrequency();
				j["eqGain"] = a->GetEQGain();
				j["eqQ"] = a->GetEQQ();
			}
			if (a->HasDelay())
			{
				j["delaySeconds"] = a->GetDelaySeconds();
				j["delayDecay"] = a->GetDelayDecay();
				j["delayWet"] = a->GetDelayWet();
				j["delayDry"] = a->GetDelayDry();
			}
			// Bus routing (AudioBus) is deliberately NOT serialized - a bus is
			// an app-level submix construct (built once at startup, e.g. a
			// "Music"/"SFX" split), not scene data; a saved source has no
			// portable way to name which bus instance to rejoin.
			return j;
		}
				case ComponentType::DirectionalLight:
		{
			DirectionalLight* l = dynamic_cast<DirectionalLight*>(c);
			j["type"] = "DirectionalLight";
			j["color"] = ToJson(l->GetLightColor());
			j["intensity"] = l->GetLightIntensity();
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
			j["intensity"] = l->GetLightIntensity();
			j["radius"] = l->GetLightRadius();
			j["castingShadows"] = l->IsCastingShadows();
			if (l->IsCastingShadows())
			{
				j["shadowWidth"] = l->GetShadowWidth();
				j["shadowHeight"] = l->GetShadowHeight();
				j["shadowNear"] = l->GetShadowNear();
				// Same two keys DirectionalLight above already round-trips.
				// Point/spot were writing everything about their shadow
				// except its depth bias, so a scene could not carry one at
				// all - it silently reset to ILightComponent's 0/0 on load,
				// and the only shadow acne that made visible belonged to
				// spot lights, since deferred point shadows didn't render.
				j["shadowBiasFactor"] = l->GetShadowBiasFactor();
				j["shadowBiasUnits"] = l->GetShadowBiasUnits();
			}
			// Outside the castingShadows block - volumetric scattering
			// works with or without a shadow map (unshadowed just means the
			// medium glows uniformly inside the light's reach).
			WriteVolumetric(j, l);
			return j;
		}
		case ComponentType::SpotLight:
		{
			SpotLight* l = dynamic_cast<SpotLight*>(c);
			j["type"] = "SpotLight";
			j["color"] = ToJson(l->GetLightColor());
			j["intensity"] = l->GetLightIntensity();
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
				// See the identical pair on PointLight above.
				j["shadowBiasFactor"] = l->GetShadowBiasFactor();
				j["shadowBiasUnits"] = l->GetShadowBiasUnits();
			}
			WriteVolumetric(j, l);
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
			// Always persist scriptFile when set so the editor can round-trip
			// attachments even if serialize() is missing. data defaults to {}.
			if (!lc->scriptFile.empty())
			{
				j["scriptFile"] = RelativizeSceneAssetPath(lc->scriptFile);
				j["data"] = json::object();
				if (lc->data.valid())
				{
					sol::function serializeFn = lc->data["serialize"];
					if (serializeFn.valid())
					{
						sol::protected_function_result result = serializeFn(lc->data);
						if (result.valid() && result.get_type() == sol::type::table)
							j["data"] = LuaTableToJson(result.get<sol::table>());
						else
							echo("WARNING: SceneSerializer - LuaComponent serialize() did not return a table; saving empty data");
					}
				}
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
		const std::vector<std::shared_ptr<IComponent>> &comps = go->GetComponents();
		for (size_t i = 0; i < comps.size(); i++)
		{
			json cj = SerializeComponent(comps[i].get(), materialsArray, materialIdMap, lua);
			if (!cj.is_null()) components.push_back(cj);
		}
		j["components"] = components;

		json children = json::array();
		const std::vector<std::shared_ptr<GameObject>> &kids = go->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
			children.push_back(SerializeGameObject(kids[i].get(), materialsArray, materialIdMap, lua));
		j["children"] = children;

		return j;
	}

	bool SceneSerializer::SaveScene(SceneGraph* scene, const std::string &filePath, sol::state* lua, const SceneMeta* meta)
	{
		g_sceneAssetRoot = InferAssetRootFromScenePath(filePath);

		json root;
		root["version"] = 1;
		if (meta && !meta->mainScript.empty())
			root["mainScript"] = RelativizeSceneAssetPath(meta->mainScript);
		if (meta)
			root["ambientLight"] = json::array({ meta->ambientLight.x, meta->ambientLight.y, meta->ambientLight.z });

		json materialsArray = json::array();
		std::map<IMaterial*, uint32> materialIdMap;

		json roots = json::array();
		std::vector<std::shared_ptr<GameObject>> &all = scene->GetAllGameObjectList();
		for (size_t i = 0; i < all.size(); i++)
			roots.push_back(SerializeGameObject(all[i].get(), materialsArray, materialIdMap, lua));

		root["materials"] = materialsArray;
		root["roots"] = roots;

		std::ofstream out(filePath.c_str());
		if (!out.is_open())
		{
			g_sceneAssetRoot.clear();
			echo("ERROR: SceneSerializer::SaveScene - couldn't open file for writing: " + filePath);
			return false;
		}
		out << root.dump(4);
		out.close();
		g_sceneAssetRoot.clear();
		return true;
	}

	std::string SceneSerializer::SerializeSubtree(GameObject* root, const std::string &scenePathForAssetRoot, sol::state* lua)
	{
		if (!root) return std::string();

		g_sceneAssetRoot = InferAssetRootFromScenePath(scenePathForAssetRoot);

		json materialsArray = json::array();
		std::map<IMaterial*, uint32> materialIdMap;

		json out;
		out["version"] = 1;
		out["materials"] = json::array();
		out["root"] = SerializeGameObject(root, materialsArray, materialIdMap, lua);
		out["materials"] = materialsArray;

		g_sceneAssetRoot.clear();
		return out.dump();
	}

	// ******************************* load *******************************

	static std::shared_ptr<Texture> GetOrLoadTexture(const std::string &path, std::map<std::string, std::shared_ptr<Texture>> &cache, LoadedSceneAssets* outAssets)
	{
		const std::string resolved = ResolveSceneAssetPath(path);
		if (resolved.empty()) return nullptr;
		std::map<std::string, std::shared_ptr<Texture>>::iterator it = cache.find(resolved);
		if (it != cache.end()) return it->second;
		std::shared_ptr<Texture> tex = std::make_shared<Texture>();
		tex->LoadTexture(resolved, TextureType::Texture);
		cache[resolved] = tex;
		if (outAssets) outAssets->textures.push_back(tex);
		return tex;
	}

	// Cubemap as a JSON object with the six face paths (posx/negx/...).
	// Cached under a composite key so repeated skyboxMap refs share one
	// Texture. Face order matches SkyboxTest/RacingGame LoadTexture calls.
	static std::shared_ptr<Texture> GetOrLoadCubemap(const json &faces, std::map<std::string, std::shared_ptr<Texture>> &cache, LoadedSceneAssets* outAssets)
	{
		std::string posx = ResolveSceneAssetPath(faces.value("posx", std::string()));
		std::string negx = ResolveSceneAssetPath(faces.value("negx", std::string()));
		std::string posy = ResolveSceneAssetPath(faces.value("posy", std::string()));
		std::string negy = ResolveSceneAssetPath(faces.value("negy", std::string()));
		std::string posz = ResolveSceneAssetPath(faces.value("posz", std::string()));
		std::string negz = ResolveSceneAssetPath(faces.value("negz", std::string()));
		if (posx.empty() || negx.empty() || posy.empty() || negy.empty() || posz.empty() || negz.empty())
		{
			echo("WARNING: SceneSerializer - cubemap missing one or more faces (need posx/negx/posy/negy/posz/negz)");
			return nullptr;
		}
		std::string cacheKey = "cubemap|" + posx + "|" + negx + "|" + posy + "|" + negy + "|" + posz + "|" + negz;
		std::map<std::string, std::shared_ptr<Texture>>::iterator it = cache.find(cacheKey);
		if (it != cache.end()) return it->second;
		std::shared_ptr<Texture> tex = std::make_shared<Texture>();
		tex->LoadTexture(negx, TextureType::CubemapNegative_X);
		tex->LoadTexture(negy, TextureType::CubemapNegative_Y);
		tex->LoadTexture(negz, TextureType::CubemapNegative_Z);
		tex->LoadTexture(posx, TextureType::CubemapPositive_X);
		tex->LoadTexture(posy, TextureType::CubemapPositive_Y);
		tex->LoadTexture(posz, TextureType::CubemapPositive_Z);
		tex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		cache[cacheKey] = tex;
		if (outAssets) outAssets->textures.push_back(tex);
		return tex;
	}

	static std::shared_ptr<Texture> DeserializeTextureRef(const json &parent, const std::string &key, std::map<std::string, std::shared_ptr<Texture>> &textureCache, LoadedSceneAssets* outAssets)
	{
		if (parent.find(key) != parent.end())
		{
			if (parent[key].is_object())
				return GetOrLoadCubemap(parent[key], textureCache, outAssets);
			return GetOrLoadTexture(parent[key].get<std::string>(), textureCache, outAssets);
		}
		std::string dataKey = key + "Data";
		if (parent.find(dataKey) != parent.end())
		{
			// No natural dedup key for embedded data (unlike paths) -
			// always creates a fresh Texture.
			std::vector<uchar> bytes = Base64Decode(parent[dataKey].get<std::string>());
			std::shared_ptr<Texture> tex = std::make_shared<Texture>();
			tex->LoadTextureFromMemory(bytes, (uint32)bytes.size(), TextureType::Texture);
			if (outAssets) outAssets->textures.push_back(tex);
			return tex;
		}
		return nullptr;
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

	// Material-Editor-generated shaders written before emissive was moved out
	// of the G-buffer's albedo channel. The old form packed `albedo + emissive`
	// into FragData_r.rgb - which the deferred light passes then multiply by
	// N.L - and derived the ambient alphas from that same sum, so a material's
	// *emissive* ended up scaled by the scene's ambient colour. Turning the
	// ambient red made such a material's glow go red under Deferred while
	// Forward, which adds emissive after lighting, did not budge; that
	// mismatch is exactly what this removes.
	//
	// Only reachable for materials with no recoverable .mat - those are the
	// ones a scene bakes a copy of the generated GLSL into (see the
	// shaderFile vs shaderSource branch below), and there is nothing left to
	// regenerate them from. Anything applied since records a shaderFile and
	// is recompiled from that instead. The pattern is text MaterialCodegen
	// emitted verbatim, so this either matches wholesale or does nothing.
	// This used to rewrite the layout below in the OPPOSITE direction,
	// moving emissive out of the albedo channel and into the alphas alone.
	// That packing does not work: measured on a pure-emissive material
	// (Albedo 0, Emissive 0.8), the alpha-only form renders nothing at all
	// in Deferred, while the albedo-channel form matches Forward to within
	// a pixel value (204 vs 204.7). So the rewrite now runs the other way,
	// repairing any scene that was saved while the broken codegen was
	// live - those bakes are otherwise permanently emissive-less, since a
	// baked shaderSource has no .mat left to regenerate it from.
	static std::string UpgradeGeneratedGBufferWrite(const std::string &src)
	{
		static const std::string kBroken =
			"\tvec3 litAlbedo = albedo * occlusion;\n"
			"\tvec3 addTerm = litAlbedo * uAmbientLight.rgb + emissive;\n"
			"\tFragData_r = vec4(litAlbedo, addTerm.x);\n"
			"\tFragData_g = vec4(1.0, 1.0, 1.0, addTerm.y);\n"
			"\tFragData_b = vec4(normalOut, addTerm.z);\n";
		static const std::string kWorking =
			"\tvec3 color = albedo * occlusion + emissive;\n"
			"\tFragData_r = vec4(color, color.x * uAmbientLight.x);\n"
			"\tFragData_g = vec4(1.0, 1.0, 1.0, color.y * uAmbientLight.y);\n"
			"\tFragData_b = vec4(normalOut, color.z * uAmbientLight.z);\n";
		const size_t at = src.find(kBroken);
		if (at == std::string::npos) return src;
		std::string out = src;
		out.replace(at, kBroken.size(), kWorking);
		echo("SceneSerializer: repaired a baked custom shader whose emissive was lost in the Deferred G-buffer");
		return out;
	}

	static std::shared_ptr<IMaterial> BuildMaterial(const json &j, std::map<std::string, std::shared_ptr<Texture>> &textureCache, LoadedSceneAssets* outAssets)
	{
		std::string kind = j.value("kind", "unsupported");
		if (kind == "generic")
		{
			std::shared_ptr<GenericShaderMaterial> gm = std::make_shared<GenericShaderMaterial>(j.value("options", 0u));
			if ((j.find("color") != j.end())) gm->SetColor(Vec4FromJson(j["color"]));
			if ((j.find("specular") != j.end())) gm->SetSpecular(Vec4FromJson(j["specular"]));
			if ((j.find("displacementHeight") != j.end())) gm->SetDisplacementHeight(j["displacementHeight"].get<f32>());
			if ((j.find("reflectivity") != j.end())) gm->SetReflectivity(j["reflectivity"].get<f32>());
			if ((j.find("shininess") != j.end())) gm->SetShininess(j["shininess"].get<f32>());
			if ((j.find("metallic") != j.end())) gm->SetMetallic(j["metallic"].get<f32>());
			if ((j.find("roughness") != j.end())) gm->SetRoughness(j["roughness"].get<f32>());
			if (j.value("ssrEnabled", false)) gm->SetSSREnabled(true);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "colorMap", textureCache, outAssets)) gm->SetColorMap(t);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "specularMap", textureCache, outAssets)) gm->SetSpecularMap(t);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "normalMap", textureCache, outAssets)) gm->SetNormalMap(t);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "displacementMap", textureCache, outAssets)) gm->SetDisplacementMap(t);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "envMap", textureCache, outAssets)) gm->SetEnvMap(t);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "refractMap", textureCache, outAssets)) gm->SetRefractMap(t);
			if (j.find("alphaCutoff") != j.end()) gm->SetAlphaCutoff(j.value("alphaCutoff", 0.5f));
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "skyboxMap", textureCache, outAssets)) gm->SetSkyboxMap(t);
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "metallicRoughnessMap", textureCache, outAssets)) gm->SetMetallicRoughnessMap(t);
			ApplyCommonMaterialFields(gm.get(), j);
			return gm;
		}
		else if (kind == "custom")
		{
			std::shared_ptr<CustomShaderMaterial> cm;
			if (j.find("shaderFile") != j.end())
				cm = std::make_shared<CustomShaderMaterial>(ResolveSceneAssetPath(j.value("shaderFile", std::string())));
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
				s->LoadShaderText(UpgradeGeneratedGBufferWrite(j["shaderSource"].get<std::string>()));
				s->CompileShader(ShaderType::VertexShader, std::string("#define VERTEX\n") + define);
				s->CompileShader(ShaderType::FragmentShader, std::string("#define FRAGMENT\n") + define);
				s->LinkProgram();
				cm = std::make_shared<CustomShaderMaterial>(s);
			}
			else
			{
				echo("WARNING: SceneSerializer - skipping a CustomShaderMaterial entry with neither shaderFile nor shaderSource");
				return nullptr;
			}
			// CustomShaderMaterial's own constructors never call AddUniform -
			// unlike GenericShaderMaterial (whose constructor wires its
			// fixed uniform set internally), a CustomShaderMaterial is
			// meant to be flexible/user-defined, so wiring these is left to
			// the caller. The Material Editor's own compile paths
			// (MaterialEditor::ApplyGraphOrTextToLiveMaterial/
			// RecompileFromDisk) do this - but a material reconstructed
			// straight from a scene file via this function bypasses both of
			// those entirely, and nothing here ever did it either. Result:
			// every CustomShaderMaterial loaded with a scene (as opposed to
			// opened fresh in the Material Editor) silently never received
			// uAmbientLight (or any other standard uniform) at all - not a
			// wrong value, no value ever sent, permanently zero. Verified
			// by dumping the auto-UBO scratch buffer right before upload:
			// (0,0,0,0) for uAmbientLight on every frame. SendUniform/
			// CaptureExtraUniform silently skip any name the compiled
			// shader doesn't declare, so issuing the same fixed set
			// ApplyGraphOrTextToLiveMaterial does is harmless for a shader
			// that only uses some of them.
			cm->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
			cm->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
			cm->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
			cm->AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
			cm->AddUniform(Uniform("uCameraPosition", Uniforms::DataUsage::CameraPosition));
			cm->AddUniform(Uniform("uTime", Uniforms::DataUsage::Timer));
			cm->AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
			cm->AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));

			// Rebind the samplers saved alongside the shader. Order matters:
			// AddSampler hands out unit indices sequentially, so these have
			// to go back in the order they were written.
			if (j.find("samplers") != j.end() && j["samplers"].is_array())
			{
				for (auto &sj : j["samplers"])
				{
					const std::string name = sj.value("name", std::string());
					if (name.empty()) continue;
					if (std::shared_ptr<Texture> t = DeserializeTextureRef(sj, "texture", textureCache, outAssets))
						cm->AddSampler(name, t);
				}
			}

			ApplyCommonMaterialFields(cm.get(), j);
			return cm;
		}
		echo("WARNING: SceneSerializer - skipping an unsupported material entry on load");
		return nullptr;
	}

	static std::shared_ptr<Renderable> DeserializeRenderable(const json &j, LoadedSceneAssets* outAssets)
	{
		if (j.is_null()) return nullptr;
		std::string kind = j.value("kind", "");
		std::shared_ptr<Renderable> r;
		if (kind == "model")
			r = std::make_shared<Model>(ResolveSceneAssetPath(j.value("path", std::string())), j.value("mergeMeshes", true));
		else if (kind == "text")
		{
			// No font pooling/dedup - each loaded Text gets its own Font
			// instance, matching what direct C++ construction would do
			// anyway; out of scope to optimize here. The Font isn't
			// tracked in LoadedSceneAssets - Text doesn't expose a way
			// to retrieve/free it separately, and it's a small,
			// self-contained allocation, not part of the material/
			// texture/renderable pools this manifest targets.
			Font* font = new Font(ResolveSceneAssetPath(j.value("font", std::string())), j.value("fontSize", 16.0f));
			std::string text = j.value("text", std::string());
			f32 charWidth = j.value("charWidth", 1.0f);
			f32 charHeight = j.value("charHeight", 1.0f);
			if (j.find("charColors") != j.end())
			{
				std::vector<Vec4> colors;
				for (auto &cj : j["charColors"]) colors.push_back(Vec4FromJson(cj));
				r = std::make_shared<Text>(font, text, charWidth, charHeight, colors);
			}
			else
			{
				Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
				r = std::make_shared<Text>(font, text, charWidth, charHeight, color);
			}
		}
		else if (kind == "decal")
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
			r = std::make_shared<Decal>(verts, haveBones);
		}
		else if (kind == "primitive")
		{
			bool smooth = j.value("smooth", false);
			bool flip = j.value("flip", false);
			bool tb = j.value("tangentBitangent", false);
			std::string shape = j.value("shape", "");

			if (shape == "Cube") r = std::make_shared<Cube>(j.value("width", 1.0f), j.value("height", 1.0f), j.value("depth", 1.0f), smooth, flip, tb);
			else if (shape == "Sphere") r = std::make_shared<Sphere>(j.value("radius", 1.0f), j.value("segmentsW", 16u), j.value("segmentsH", 16u), smooth, j.value("halfSphere", false), flip, tb);
			else if (shape == "Cone") r = std::make_shared<Cone>(j.value("radius", 1.0f), j.value("height", 1.0f), j.value("segmentsW", 16u), j.value("segmentsH", 16u), j.value("openEnded", false), smooth, flip, tb);
			else if (shape == "Cylinder") r = std::make_shared<Cylinder>(j.value("radius", 1.0f), j.value("height", 1.0f), j.value("segmentsW", 16u), j.value("segmentsH", 16u), j.value("openEnded", false), smooth, flip, tb);
			else if (shape == "Plane") r = std::make_shared<Plane>(j.value("width", 1.0f), j.value("height", 1.0f), smooth, flip, tb);
			else if (shape == "Capsule") r = std::make_shared<Capsule>(j.value("radius", 1.0f), j.value("height", 1.0f), j.value("numRings", 8u), j.value("segmentsW", 16u), j.value("segmentsH", 16u), smooth, flip, tb);
			else if (shape == "Torus") r = std::make_shared<Torus>(j.value("radius", 1.0f), j.value("tube", 0.3f), j.value("segmentsW", 60u), j.value("segmentsH", 6u), smooth, flip, tb);
			else if (shape == "TorusKnot") r = std::make_shared<TorusKnot>(j.value("radius", 1.0f), j.value("tube", 0.3f), j.value("segmentsW", 60u), j.value("segmentsH", 6u), j.value("p", 2.0f), j.value("q", 3.0f), j.value("heightScale", 1u), smooth, flip, tb);
			else echo("WARNING: SceneSerializer - unrecognized primitive shape on load: " + shape);
		}
		if (r && outAssets) outAssets->renderables.push_back(r);
		return r;
	}

	// Mirrors SerializePhysicsShape - reused by both the flat "Physics"
	// component case and Vehicle's nested "chassis" (an orphan
	// IPhysicsComponent* the vehicle owns directly, not a tree-attached
	// component).
	static std::shared_ptr<IPhysicsComponent> DeserializePhysicsShape(const json &j, IPhysics* physics)
	{
		if (!physics) { echo("WARNING: SceneSerializer - can't rebuild a physics shape, LoadScene() was called with physics == NULL"); return nullptr; }
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
		return nullptr;
	}

	static void DeserializeComponent(GameObject* go, const json &j, const std::vector<std::shared_ptr<IMaterial>> &materialsById, std::map<std::string, std::shared_ptr<Texture>> &textureCache, IPhysics* physics, sol::state* lua, LoadedSceneAssets* outAssets)
	{
		std::string type = j.value("type", "");

		if (type == "RenderingComponent")
		{
			std::shared_ptr<Renderable> renderable = DeserializeRenderable(j.value("renderable", json()), outAssets);
			if (!renderable) { echo("WARNING: SceneSerializer - skipping RenderingComponent, couldn't rebuild its renderable"); return; }
			uint32 matId = j.value("material", (uint32)0xFFFFFFFF);
			std::shared_ptr<IMaterial> mat = (matId < materialsById.size()) ? materialsById[matId] : nullptr;

			// Models: rebuild per-submesh materials from the .p3dm (texture
			// paths resolve next to the package). Applying one serialized
			// material to every mesh dropped package textures on reload.
			const bool isModel = (dynamic_cast<Model*>(renderable.get()) != NULL);
			if (!isModel && !mat) { echo("WARNING: SceneSerializer - skipping RenderingComponent, its material couldn't be rebuilt"); return; }

			std::shared_ptr<RenderingComponent> rc;
			if (isModel)
			{
				// Diffuse only here — shadow shader variants sample depth
				// maps that may not exist yet on load (GL: unloadable depth
				// sampler; Vulkan: validation/segfault). Shadows still work
				// via EnableCastShadows + lights; the material picks up
				// Texture/Specular from the .p3dm in BuildMaterials.
				const uint32 opts = ShaderUsage::Diffuse;
#ifdef LUA_BINDINGS
				rc = lua
					? std::static_pointer_cast<RenderingComponent>(std::make_shared<LUA_RenderingComponent>(renderable, opts))
					: std::make_shared<RenderingComponent>(renderable, opts);
#else
				rc = std::make_shared<RenderingComponent>(renderable, opts);
#endif
			}
			else
			{
#ifdef LUA_BINDINGS
				rc = lua
					? std::static_pointer_cast<RenderingComponent>(std::make_shared<LUA_RenderingComponent>(renderable, mat, 0.0f))
					: std::make_shared<RenderingComponent>(renderable, mat, 0.0f);
#else
				rc = std::make_shared<RenderingComponent>(renderable, mat, 0.0f);
#endif
			}
			if (j.value("cullTest", true)) rc->EnableCullTest(); else rc->DisableCullTest();
			if (j.value("castingShadows", true)) rc->EnableCastShadows(); else rc->DisableCastShadows();
			go->AddComponent(rc);

			if (j.find("skeletonAnimation") != j.end())
			{
				const json &sa = j["skeletonAnimation"];
				std::string path = ResolveSceneAssetPath(sa.value("path", std::string()));
				if (!path.empty())
				{
					std::shared_ptr<SkeletonAnimation> anim = std::make_shared<SkeletonAnimation>();
					// Every clip file, in the order that assigned the ids the
					// "playing" entries below refer to (see the save side's
					// comment); "path" alone is the pre-"paths" fallback.
					if (sa.find("paths") != sa.end() && !sa["paths"].empty())
						for (auto &pp : sa["paths"]) anim->LoadAnimation(ResolveSceneAssetPath(pp.get<std::string>()));
					else
						anim->LoadAnimation(path);
					if (outAssets) outAssets->skeletonAnimations.push_back(anim);
					SkeletonAnimationInstance* inst = anim->CreateInstance(rc.get());
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
							// -1 (loop forever) as the default, not 1: every
							// looping animation restored as a single pass
							// played once and then froze.
							int32 order = inst->Play(id, pj.value("startTimeProgress", 0.0f), pj.value("repeat", -1.0f), pj.value("speed", 1.0f), pj.value("scale", 1.0f), layer);
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
					std::shared_ptr<TextureAnimation> anim = std::make_shared<TextureAnimation>();
					if (outAssets) outAssets->textureAnimations.push_back(anim);
					for (auto &fj : ta["frames"]) anim->AddFrame(DeserializeTextureRef(fj, "tex", textureCache, outAssets));
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
			if (std::shared_ptr<Texture> t = DeserializeTextureRef(j, "texture", textureCache, outAssets)) d.texture = t;
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
			go->AddComponent(std::make_shared<ParticleSystem>(d));
		}
		else if (type == "AudioSource")
		{
			// The file path is the one field with no sensible default - without
			// it there is nothing to construct, so skip rather than build a
			// permanently-unloaded source.
			const std::string file = ResolveSceneAssetPath(j.value("file", std::string()));
			if (!file.empty())
			{
				auto a = std::make_shared<AudioSource>(file, j.value("stream", false));
				a->SetSpatialization(j.value("spatialized", true));
				a->SetLooping(j.value("looping", false));
				a->SetVolume(j.value("volume", 1.0f));
				a->SetPitch(j.value("pitch", 1.0f));
				a->SetAttenuation(j.value("attenuationModel", (uint32)AttenuationModel::Linear),
					j.value("minDistance", 1.0f), j.value("maxDistance", 100.0f));
				a->SetDirectionalAttenuation(j.value("directionalAttenuation", 1.0f));
				a->SetDopplerFactor(j.value("dopplerFactor", 1.0f));
				a->SetPan(j.value("pan", 0.0f));
				if (j.find("coneInnerAngle") != j.end())
					a->SetCone(j.value("coneInnerAngle", 6.283185f), j.value("coneOuterAngle", 6.283185f), j.value("coneOuterGain", 1.0f));
				if (j.find("filterType") != j.end())
					a->SetFilter(j.value("filterType", (uint32)AudioFilterType::None), j.value("filterCutoff", 1000.0f), j.value("filterOrder", 2u));
				if (j.find("eqType") != j.end())
					a->SetEQ(j.value("eqType", (uint32)AudioEQType::None), j.value("eqFrequency", 1000.0f), j.value("eqGain", 0.0f), j.value("eqQ", 1.0f));
				if (j.find("delaySeconds") != j.end())
					a->SetDelay(j.value("delaySeconds", 0.2f), j.value("delayDecay", 0.5f), j.value("delayWet", 1.0f), j.value("delayDry", 1.0f));
				a->EnsureLoaded();
				// Resume playback last, so it starts with its final settings
				// rather than briefly sounding with the constructor defaults.
				if (j.value("playing", false)) a->Play();
				go->AddComponent(a);
			}
		}
				else if (type == "DirectionalLight")
		{
			Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
			Vec3 direction = (j.find("direction") != j.end()) ? Vec3FromJson(j["direction"]) : Vec3(0, -1, 0);
			auto l = std::make_shared<DirectionalLight>(color, direction);
			l->SetLightIntensity(j.value("intensity", 1.0f));
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
			auto l = std::make_shared<PointLight>(color, j.value("radius", 1.0f));
			l->SetLightIntensity(j.value("intensity", 1.0f));
			if (j.value("castingShadows", false))
			{
				l->EnableCastShadows(j.value("shadowWidth", 512u), j.value("shadowHeight", 512u), j.value("shadowNear", 0.1f));
				// Matches DirectionalLight's identical block above - see the
				// serialize side's comment on why point/spot were missing it.
				if ((j.find("shadowBiasFactor") != j.end()) || (j.find("shadowBiasUnits") != j.end()))
					l->SetShadowBias(j.value("shadowBiasFactor", 1.0f), j.value("shadowBiasUnits", 1.0f));
			}
			ReadVolumetric(j, l.get());
			go->AddComponent(l);
		}
		else if (type == "SpotLight")
		{
			Vec4 color = (j.find("color") != j.end()) ? Vec4FromJson(j["color"]) : Vec4(1, 1, 1, 1);
			Vec3 direction = (j.find("direction") != j.end()) ? Vec3FromJson(j["direction"]) : Vec3(0, -1, 0);
			auto l = std::make_shared<SpotLight>(color, j.value("radius", 1.0f), direction, j.value("outterCone", 45.0f), j.value("innerCone", 30.0f));
			l->SetLightIntensity(j.value("intensity", 1.0f));
			if (j.value("castingShadows", false))
			{
				l->EnableCastShadows(j.value("shadowWidth", 512u), j.value("shadowHeight", 512u), j.value("shadowNear", 0.1f));
				// See the identical block on PointLight above.
				if ((j.find("shadowBiasFactor") != j.end()) || (j.find("shadowBiasUnits") != j.end()))
					l->SetShadowBias(j.value("shadowBiasFactor", 1.0f), j.value("shadowBiasUnits", 1.0f));
			}
			ReadVolumetric(j, l.get());
			go->AddComponent(l);
		}
		else if (type == "Physics")
		{
			std::shared_ptr<IPhysicsComponent> pc = DeserializePhysicsShape(j, physics);
			if (pc) go->AddComponent(pc);
		}
		else if (type == "Vehicle")
		{
			if (!physics) { echo("WARNING: SceneSerializer - skipping a Vehicle, LoadScene() was called with physics == NULL"); return; }
			std::shared_ptr<IPhysicsComponent> chassis = (j.find("chassis") != j.end()) ? DeserializePhysicsShape(j["chassis"], physics) : nullptr;
			if (!chassis) { echo("WARNING: SceneSerializer - skipping a Vehicle, its chassis couldn't be rebuilt"); return; }
			std::shared_ptr<IPhysicsComponent> vc = physics->CreateVehicle(chassis, j.value("ghost", false));
			PhysicsVehicle* v = dynamic_cast<PhysicsVehicle*>(vc.get());
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
			go->AddComponent(vc);
		}
#ifdef LUA_BINDINGS
		else if (type == "LuaComponent")
		{
			std::string scriptFile = ResolveSceneAssetPath(j.value("scriptFile", std::string()));
			if (!scriptFile.empty() && lua)
			{
				// Always load a fresh chunk (do not use require_file).
				// DemoLauncher does not open sol::lib::package, so touching
				// package.loaded would convert nil→sol::table and abort.
				// load_file also picks up script edits without a process restart.
				sol::object result = sol::lua_nil;
				{
					sol::load_result chunk = lua->load_file(scriptFile);
					if (chunk.valid())
					{
						sol::protected_function_result loaded = chunk();
						if (loaded.valid())
							result = loaded;
						else
						{
							sol::error err = loaded;
							echo(std::string("WARNING: SceneSerializer - Lua load error in ") + scriptFile + ": " + err.what());
						}
					}
					else
					{
						sol::error err = chunk;
						echo(std::string("WARNING: SceneSerializer - couldn't read ") + scriptFile + ": " + err.what());
					}
				}
				if (result.valid() && result.get_type() == sol::type::table)
				{
					sol::table cls = result;
					sol::table instance;
					bool ok = false;
					sol::function deserializeFn = cls["deserialize"];
					if (deserializeFn.valid())
					{
						sol::table dataTable = (j.find("data") != j.end())
							? JsonToLuaTable(*lua, j["data"])
							: lua->create_table();
						sol::protected_function_result instResult = deserializeFn(dataTable);
						if (instResult.valid() && instResult.get_type() == sol::type::table)
						{
							instance = instResult.get<sol::table>();
							ok = true;
						}
						else
							echo("WARNING: SceneSerializer - LuaComponent deserialize() did not return a table");
					}
					if (!ok)
					{
						sol::function newFn = cls["new"];
						if (newFn.valid())
						{
							sol::protected_function_result instResult = newFn(cls);
							if (instResult.valid() && instResult.get_type() == sol::type::table)
							{
								instance = instResult.get<sol::table>();
								ok = true;
							}
						}
					}
					if (ok)
					{
						auto comp = std::make_shared<LuaComponent>();
						comp->scriptFile = scriptFile;
						comp->data = instance;
						WireLuaComponentLifecycle(comp.get());
						go->AddComponent(comp);
					}
					else
					{
						echo("WARNING: SceneSerializer - couldn't instantiate LuaComponent from " + scriptFile);
						go->AddComponent(std::make_shared<LuaComponent>());
					}
				}
				else
				{
					echo("WARNING: SceneSerializer - couldn't load LuaComponent's scriptFile: " + scriptFile + ", adding as existence-only");
					go->AddComponent(std::make_shared<LuaComponent>());
				}
			}
			else
			{
				// Existence only, no behavior - either an ad-hoc
				// component (no scriptFile was ever saved) or LoadScene()
				// was called with lua == NULL.
				go->AddComponent(std::make_shared<LuaComponent>());
			}
		}
#endif
	}

	static std::shared_ptr<GameObject> DeserializeGameObject(const json &j, const std::vector<std::shared_ptr<IMaterial>> &materialsById, std::map<std::string, std::shared_ptr<Texture>> &textureCache, IPhysics* physics, sol::state* lua, LoadedSceneAssets* outAssets)
	{
		// LUA_GameObject when lua != NULL, not plain GameObject - IComponent::
		// GetOwner() (used by WireLuaComponentLifecycle to pass a script its
		// own owner) returns a GameObject*, and GameObject is polymorphic, so
		// sol2's RTTI-based automatic type resolution needs the *actual*
		// object to be an instance of a registered usertype
		// (LUA_GameObject, see PyrosBindings.h/.cpp - plain GameObject
		// itself is never registered) - a real GameObject instance passed
		// into Lua this way fails every method call on it ("attempt to
		// index a sol.p3d::GameObject * value"), found via a real crash,
		// not assumed. LUA_GameObject's default constructor is otherwise
		// fully behavior-compatible (same fields; its Init/Update/Destroy
		// overrides only add no-op-when-unset on_init/on_update/on_destroy
		// hooks on top of the base class's own).
#ifdef LUA_BINDINGS
		std::shared_ptr<GameObject> go = lua
			? std::static_pointer_cast<GameObject>(std::make_shared<LUA_GameObject>(j.value("static", false)))
			: std::make_shared<GameObject>(j.value("static", false));
#else
		std::shared_ptr<GameObject> go = std::make_shared<GameObject>(j.value("static", false));
#endif
		if (outAssets) outAssets->gameObjects.push_back(go);
		go->SetName(j.value("name", std::string()));
		if ((j.find("position") != j.end())) go->SetPosition(Vec3FromJson(j["position"]));
		if ((j.find("rotation") != j.end())) go->SetRotation(Vec3FromJson(j["rotation"]));
		if ((j.find("scale") != j.end())) go->SetScale(Vec3FromJson(j["scale"]));
		if ((j.find("tags") != j.end())) for (auto &t : j["tags"]) go->AddTag(t.get<std::string>());

		if ((j.find("components") != j.end())) for (auto &cj : j["components"]) DeserializeComponent(go.get(), cj, materialsById, textureCache, physics, lua, outAssets);

		if ((j.find("children") != j.end()))
			for (auto &cj : j["children"])
			{
				std::shared_ptr<GameObject> child = DeserializeGameObject(cj, materialsById, textureCache, physics, lua, outAssets);
				go->Add(child);
			}

		return go;
	}

	bool SceneSerializer::LoadScene(SceneGraph* scene, const std::string &filePath, IPhysics* physics, sol::state* lua, LoadedSceneAssets* outAssets, SceneMeta* outMeta)
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

		// Parsed as JSON, but that does not make it a scene. Every lookup
		// below assumes an object - root.value("version", ...) on a JSON
		// array throws type_error.306 straight out of this function, which
		// contradicts the bool return and terminated the caller. Any .json
		// reaches this: a file browser filtered to *.json will happily
		// offer compile_commands.json, which is a top-level array.
		if (!root.is_object())
		{
			echo("ERROR: SceneSerializer::LoadScene - not a scene file (expected a JSON object): " + filePath);
			return false;
		}

		// Remap asset paths. Editor scenes live under <project>/scenes/ and
		// store project-relative paths (assets/models/...). Prefer the project
		// root inferred from the scene file. DemoLauncher sets ASSETS_PATH to
		// the assets/ folder itself and paths are relative to that — only use
		// it when we cannot infer a project root (otherwise we join
		// .../assets/ + assets/models/... → assets/assets/... and the model
		// fails to open, leaving a corrupt empty Model that crashes on select).
		g_sceneAssetRoot.clear();
		g_sceneAssetRoot = InferAssetRootFromScenePath(filePath);
#ifdef LUA_BINDINGS
		if (g_sceneAssetRoot.empty() && lua)
		{
			sol::object assets = (*lua)["ASSETS_PATH"];
			if (assets.valid() && assets.get_type() == sol::type::string)
				g_sceneAssetRoot = assets.as<std::string>();
		}
#endif
		if (g_sceneAssetRoot.empty())
		{
			const std::string marker = "examples/assets/";
			const size_t pos = filePath.find(marker);
			if (pos != std::string::npos)
				g_sceneAssetRoot = filePath.substr(0, pos + marker.size());
		}

		int sceneVersion = 0;
		if (root.contains("version") && root["version"].is_number())
			sceneVersion = (int)root["version"].get<double>();
		if (sceneVersion != 1)
			echo("WARNING: SceneSerializer::LoadScene - unexpected scene file version, attempting to load anyway");

		if (outMeta)
		{
			outMeta->mainScript.clear();
			if (root.contains("mainScript") && root["mainScript"].is_string())
			{
				const std::string raw = root["mainScript"].get<std::string>();
				if (!raw.empty())
					outMeta->mainScript = ResolveSceneAssetPath(raw);
			}
			// Left at SceneMeta's own default (matching IRenderer's
			// constructor default) when the file predates this field.
			if (root.contains("ambientLight") && root["ambientLight"].is_array() && root["ambientLight"].size() >= 3)
			{
				const auto &al = root["ambientLight"];
				outMeta->ambientLight = Vec4(al[0].get<f32>(), al[1].get<f32>(), al[2].get<f32>(), al[0].get<f32>());
			}
		}

		std::map<std::string, std::shared_ptr<Texture>> textureCache;
		std::vector<std::shared_ptr<IMaterial>> materialsById;
		if ((root.find("materials") != root.end()))
			for (auto &mj : root["materials"])
			{
				std::shared_ptr<IMaterial> mat = BuildMaterial(mj, textureCache, outAssets);
				materialsById.push_back(mat);
				if (mat && outAssets) outAssets->materials.push_back(mat);
			}

		if ((root.find("roots") != root.end()))
			for (auto &rj : root["roots"])
			{
				// A root may carry "instances": [[x,y,z], ...], in which case it
				// is a *template* and one GameObject is built per entry, with
				// that entry as its position and everything else (components,
				// material, rotation, scale, children) shared. Purely a file-size
				// measure for scenes made of many identical objects: SimplePhysics
				// is 1000 identical 5x5x5 physics cubes at random positions, and
				// writing each one out in full made that scene 48,140 lines of
				// JSON. Each instance still goes through the ordinary
				// DeserializeGameObject() path, so it lands in LoadedSceneAssets
				// and UnloadScene() frees it exactly like any other object -
				// nothing here is a special case after load.
				//
				// Load-side only: SaveScene() has no idea which objects came from
				// a template, so re-saving such a scene writes every instance out
				// in full. That is fine for authored content (which is loaded, not
				// round-tripped) but worth knowing before saving over one.
				if (rj.find("instances") != rj.end() && rj["instances"].is_array())
				{
					json tmpl = rj;
					tmpl.erase("instances");
					for (auto &inst : rj["instances"])
					{
						if (inst.is_array() && inst.size() >= 3) tmpl["position"] = inst;
						scene->Add(DeserializeGameObject(tmpl, materialsById, textureCache, physics, lua, outAssets));
					}
					continue;
				}
				scene->Add(DeserializeGameObject(rj, materialsById, textureCache, physics, lua, outAssets));
			}

		g_sceneAssetRoot.clear();
		return true;
	}

	std::shared_ptr<GameObject> SceneSerializer::DeserializeSubtree(const std::string &subtreeJson, const std::string &scenePathForAssetRoot,
		IPhysics* physics, sol::state* lua, LoadedSceneAssets* outAssets)
	{
		json subtree;
		try
		{
			subtree = json::parse(subtreeJson);
		}
		catch (const std::exception&)
		{
			echo("ERROR: SceneSerializer::DeserializeSubtree - invalid JSON");
			return nullptr;
		}
		if (!subtree.is_object() || subtree.find("root") == subtree.end())
		{
			echo("ERROR: SceneSerializer::DeserializeSubtree - missing 'root'");
			return nullptr;
		}

		g_sceneAssetRoot = InferAssetRootFromScenePath(scenePathForAssetRoot);

		std::map<std::string, std::shared_ptr<Texture>> textureCache;
		std::vector<std::shared_ptr<IMaterial>> materialsById;
		if (subtree.find("materials") != subtree.end())
			for (auto &mj : subtree["materials"])
			{
				std::shared_ptr<IMaterial> mat = BuildMaterial(mj, textureCache, outAssets);
				materialsById.push_back(mat);
				if (mat && outAssets) outAssets->materials.push_back(mat);
			}

		std::shared_ptr<GameObject> result = DeserializeGameObject(subtree["root"], materialsById, textureCache, physics, lua, outAssets);

		g_sceneAssetRoot.clear();
		return result;
	}

	void SceneSerializer::UnloadScene(SceneGraph* scene, LoadedSceneAssets &assets)
	{
		// Everything freed below owns GPU resources (vertex/index buffers,
		// textures, and on Vulkan the pipelines/descriptor sets cached
		// against them), and the last submitted frame is routinely still
		// executing at this point - the frame fence is only waited on at
		// the top of the *next* BeginFrame(). Freeing them unguarded is a
		// straight use-after-free on whatever that command buffer is still
		// reading. It presents as an intermittent segfault a few demo
		// switches in, and it vanishes entirely under a debugger (which
		// slows the CPU enough that the GPU is always already done) - which
		// is exactly why it reads as "random". Same guard, same reason, as
		// PostEffectsManager::RemoveAllEffects() and ~DeferredRenderer().
		if (IsActiveRenderDeviceSet())
			GetActiveRenderDevice().WaitIdle();
		for (const std::shared_ptr<GameObject> &go : assets.gameObjects)
		{
			// Remove() is a safe no-op (logs, doesn't crash) for a
			// GameObject that was never actually registered with
			// `scene` - true of every recursively-created child here
			// (only top-level roots are ever Scene->Add()'d, see
			// LoadedSceneAssets' comment), so it's simplest and correct
			// to call it unconditionally rather than track root-vs-child
			// separately.
			scene->Remove(go);
		}
		// Drop GameObject/IComponent shared_ptrs first so components release
		// their shared refs to materials/textures/renderables/animations;
		// then clear those vectors (no delete - shared_ptr owns them).
		assets.gameObjects.clear();
		assets.skeletonAnimations.clear();
		assets.textureAnimations.clear();
		assets.materials.clear();
		assets.textures.clear();
		assets.renderables.clear();
	}

}
