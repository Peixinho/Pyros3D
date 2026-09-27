//============================================================================
// Name        : GenericShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Generic Shader Materials
//============================================================================

#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Resources/Resources.h>

namespace p3d
{

	// Shaders List
	std::map<uint32, Shader* > GenericShaderMaterial::ShadersList;

	// Shared by the constructor and GetOrBuildGBufferProgram() - both need
	// the exact same options-bitmask -> #define text, or the two compiled
	// variants of "the same options" (with/without DeferredRenderer_Gbuffer
	// OR'd in) would drift out of sync with each other.
	static std::string BuildShaderUsageDefines(const uint32 options)
	{
		std::string define;
		if (options & ShaderUsage::Color)
			define += std::string("#define COLOR\n");
		if (options & ShaderUsage::DebugRendering)
			define += std::string("#define DEBUGRENDERING\n");
		if (options & ShaderUsage::Texture)
			define += std::string("#define TEXTURE\n");
		if (options & ShaderUsage::TextRendering)
			define += std::string("#define TEXTRENDERING\n");
		if (options & ShaderUsage::TextSDF)
			define += std::string("#define TEXTSDF\n");
		if (options & ShaderUsage::VertexColor)
			define += std::string("#define VERTEXCOLOR\n");
		if (options & ShaderUsage::DirectionalShadow)
			define += std::string("#define DIRECTIONALSHADOW\n");
		if (options & ShaderUsage::PointShadow)
			define += std::string("#define POINTSHADOW\n");
		if (options & ShaderUsage::SpotShadow)
			define += std::string("#define SPOTSHADOW\n");
		if (options & ShaderUsage::CastShadows)
			define += std::string("#define CASTSHADOWS\n");
		if (options & ShaderUsage::BumpMapping)
			define += std::string("#define BUMPMAPPING\n");
		if (options & ShaderUsage::ParallaxMapping)
			define += std::string("#define PARALLAXMAPPING\n");
		if (options & ShaderUsage::Skinning)
			define += std::string("#define SKINNING\n");
		if (options & ShaderUsage::EnvMap)
			define += std::string("#define ENVMAP\n");
		if (options & ShaderUsage::Skybox)
			define += std::string("#define SKYBOX\n");
		if (options & ShaderUsage::Refraction)
			define += std::string("#define REFRACTION\n");
		if (options & ShaderUsage::SpecularColor)
			define += std::string("#define SPECULARCOLOR\n");
		if (options & ShaderUsage::SpecularMap)
			define += std::string("#define SPECULARMAP\n");
		if (options & ShaderUsage::Diffuse)
			define += std::string("#define DIFFUSE\n");
		if (options & ShaderUsage::CellShading)
			define += std::string("#define CELLSHADING\n");
		if (options & ShaderUsage::Lighting2D)
			define += std::string("#define LIGHTING2D\n");
		if (options & ShaderUsage::GlobalIllumination)
			define += std::string("#define GLOBALILLUMINATION\n");
		if (options & ShaderUsage::ClipPlane)
			define += std::string("#define CLIPSPACE\n");
		if (options & ShaderUsage::DeferredRenderer_Gbuffer)
			define += std::string("#define DEFERRED_GBUFFER\n");
		if (options & ShaderUsage::InstancedRendering)
			define += std::string("#define INSTANCED_RENDERING\n");
		if (options & ShaderUsage::VelocityRendering)
			define += std::string("#define VELOCITY_RENDERING\n");
		if (options & ShaderUsage::PBR)
			define += std::string("#define PBR\n");
		if (options & ShaderUsage::PBRMap)
			define += std::string("#define PBRMAP\n");
		if (options & ShaderUsage::AlphaTest)
			define += std::string("#define ALPHATEST\n");
		if (options & ShaderUsage::InstancedColor)
			define += std::string("#define INSTANCED_COLOR\n");
		if (options & ShaderUsage::VertexWind)
			define += std::string("#define VERTEXWIND\n");
		return define;
	}

	GenericShaderMaterial::GenericShaderMaterial(const uint32 options) : IMaterial()
	{
		// Default
		colorMapID = specularMapID = normalMapID = displacementMapID = envMapID = skyboxMapID = refractMapID = fontMapID = -1;
		metallicRoughnessMapID = -1;

		displacementHeight = 0.05f;

		uColor = uSpecular = uReflectivity = NULL;
		uMetallic = uRoughness = NULL;
		uSSRReflective = NULL;
		uAlphaCutoffUniform = NULL;
		// Half is the usual default for a cutout map; only consulted
		// when ShaderUsage::AlphaTest is on.
		AlphaCutoff = 0.5f;
		Wind = Vec4(0.f, 0.f, 0.f, 0.f);
		uShininess = uUseLights = uDisplacementHeight = NULL;
		SSREnabled = 0.0f;

		// Find if Shader exists, if not, creates a new one
		if (ShadersList.find(options) == ShadersList.end())
		{
			ShadersList[options] = new Shader();
			ShadersList[options]->currentMaterials = 0;
			//ShaderLib::BuildShader(options, ShadersList[options]);

			ShadersList[options]->LoadShaderFile("shaders/PyrosShader.glsl");

			const std::string define = BuildShaderUsageDefines(options);
			ShadersList[options]->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
			ShadersList[options]->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str());

			ShadersList[options]->LinkProgram();
		}

		// Save Shader Location
		shaderID = options;

		// Add Counter
		ShadersList[options]->currentMaterials++;

		// Get Shader Program
		shaderProgram = ShadersList[options]->ShaderProgram();

		// Back Face Culling
		cullFace = CullFace::BackFace;

		// Always used Uniforms
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));

		// Default Opacity
		SetOpacity(1.0);

		if (options & ShaderUsage::Diffuse)
		{
			UseLights = 1.0;
			Shininess = 50.0;
			AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
			AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));
			AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
			uUseLights = AddUniform(Uniform("uUseLights", Uniforms::DataType::Float, &UseLights));
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			uShininess = AddUniform(Uniform("uShininess", Uniforms::DataType::Float, &Shininess));
		}

		if (options & ShaderUsage::CellShading)
		{
			UseLights = 1.0;
			Shininess = 50.0;
			AddUniform(Uniform("uLights", Uniforms::DataUsage::Lights));
			AddUniform(Uniform("uNumberOfLights", Uniforms::DataUsage::NumberOfLights));
			AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
			uUseLights = AddUniform(Uniform("uUseLights", Uniforms::DataType::Float, &UseLights));
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			uShininess = AddUniform(Uniform("uShininess", Uniforms::DataType::Float, &Shininess));
		}

		if (options & ShaderUsage::PBR)
		{
			// Sensible defaults: fully dielectric, mid-rough
			Metallic = 0.0f;
			Roughness = 0.5f;
			uMetallic = AddUniform(Uniform("uMetallic", Uniforms::DataType::Float, &Metallic));
			uRoughness = AddUniform(Uniform("uRoughness", Uniforms::DataType::Float, &Roughness));
		}

		if (options & ShaderUsage::GlobalIllumination)
		{
			// Engine-owned, like the shadow maps below: the volume
			// belongs to the scene. Declared only under the flag,
			// because a sampler a shader declares and nothing binds is
			// a dropped draw on WebGL2.
			AddUniform(Uniform("uDDGIIrradiance", Uniforms::DataUsage::DDGIIrradianceMap));
			AddUniform(Uniform("uDDGIVisibility", Uniforms::DataUsage::DDGIVisibilityMap));
			AddUniform(Uniform("uDDGIRadiance", Uniforms::DataUsage::DDGIRadianceMap));
			AddUniform(Uniform("uBRDFLut", Uniforms::DataUsage::BRDFLutMap));
			AddUniform(Uniform("uDDGIProbeData", Uniforms::DataUsage::DDGIProbeDataMap));
		}

		if (options & ShaderUsage::DirectionalShadow)
		{
			// Shadows
			AddUniform(Uniform("uDirectionalShadowMaps", Uniforms::DataUsage::DirectionalShadowMap));
			AddUniform(Uniform("uDirectionalDepthsMVP", Uniforms::DataUsage::DirectionalShadowMatrix));
			AddUniform(Uniform("uDirectionalShadowFar", Uniforms::DataUsage::DirectionalShadowFar));
			AddUniform(Uniform("uNumberOfDirectionalShadows", Uniforms::DataUsage::NumberOfDirectionalShadows));
			isCastingShadows = true;
		}

		if (options & ShaderUsage::PointShadow)
		{
			// Shadows
			AddUniform(Uniform("uPointShadowMaps", Uniforms::DataUsage::PointShadowMap));
			AddUniform(Uniform("uPointDepthsMVP", Uniforms::DataUsage::PointShadowMatrix));
			AddUniform(Uniform("uNumberOfPointShadows", Uniforms::DataUsage::NumberOfPointShadows));
			isCastingShadows = true;
		}

		if (options & ShaderUsage::SpotShadow)
		{
			// Shadows
			AddUniform(Uniform("uSpotShadowMaps", Uniforms::DataUsage::SpotShadowMap));
			AddUniform(Uniform("uSpotDepthsMVP", Uniforms::DataUsage::SpotShadowMatrix));
			AddUniform(Uniform("uNumberOfSpotShadows", Uniforms::DataUsage::NumberOfSpotShadows));
			isCastingShadows = true;
		}

		if (options & ShaderUsage::EnvMap)
		{
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			// Set Default Reflectivity
			Reflectivity = 1.0;
			uReflectivity = AddUniform(Uniform("uReflectivity", Uniforms::DataType::Float, &Reflectivity));
		}

		if (options & ShaderUsage::Refraction)
		{
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			// Set Default Reflectivity
			Reflectivity = 1.0;
			uReflectivity = AddUniform(Uniform("uReflectivity", Uniforms::DataType::Float, &Reflectivity));
		}

		if (options & ShaderUsage::Skinning)
		{
			AddUniform(Uniform("uBoneMatrix", Uniforms::DataUsage::Skinning));
		}

		if (options & ShaderUsage::ClipPlane)
		{
			AddUniform(Uniform("uClipPlanes", Uniforms::DataUsage::ClipPlanes));
		}

		if (options & ShaderUsage::TextRendering)
		{
			SetTransparencyFlag(true);
		}

		if (options & ShaderUsage::ParallaxMapping)
		{
			AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
			uDisplacementHeight = AddUniform(Uniform("uDisplacementHeight", Uniforms::DataType::Float, &displacementHeight));
		}

		if (options & ShaderUsage::DeferredRenderer_Gbuffer)
		{
			AddUniform(Uniform("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight));
		}

		if (options & ShaderUsage::VelocityRendering)
		{
			AddUniform(Uniform("uPrvModelMatrix", Uniforms::DataUsage::PrvModelMatrix));
			AddUniform(Uniform("uPrvViewMatrix", Uniforms::DataUsage::PrvViewMatrix));
			AddUniform(Uniform("uPrvProjectionMatrix", Uniforms::DataUsage::PrvProjectionMatrix));
		}
	}

	namespace {
		struct FingerprintHasher
		{
			uint64 h = 1469598103934665603ull;
			void Bytes(const void* p, size_t n)
			{
				const uchar* b = static_cast<const uchar*>(p);
				for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ull;
			}
			template <typename T> void Value(const T &v) { Bytes(&v, sizeof(T)); }
			void Vec(const Vec4 &v) { Value(v.x); Value(v.y); Value(v.z); Value(v.w); }
			void Uniforms(const std::list<Uniform> &list)
			{
				for (std::list<Uniform>::const_iterator u = list.begin(); u != list.end(); u++)
				{
					Value(u->NameID); Value(u->Usage); Value(u->Type);
					const size_t n = u->Value.size();
					Value(n);
					if (n) Bytes(&u->Value[0], n);
				}
			}
		};
	}

	uint64 GenericShaderMaterial::RenderFingerprint() const
	{
		FingerprintHasher f;
		f.Value(shaderID);
		const size_t textureCount = Textures.size();
		f.Value(textureCount);
		for (size_t i = 0; i < textureCount; i++) f.Value(Textures[i].get());
		f.Value(colorMapID); f.Value(specularMapID); f.Value(normalMapID); f.Value(displacementMapID);
		f.Value(envMapID); f.Value(skyboxMapID); f.Value(refractMapID); f.Value(fontMapID); f.Value(metallicRoughnessMapID);
		f.Vec(Ke); f.Vec(Ka); f.Vec(Kd); f.Vec(Ks); f.Vec(Wind);
		f.Value(Shininess); f.Value(UseLights); f.Value(displacementHeight); f.Value(Reflectivity);
		f.Value(Metallic); f.Value(Roughness); f.Value(SSREnabled); f.Value(AlphaCutoff);
		// IMaterial's render state.
		f.Value(isWireFrame); f.Value(forceDepthWrite); f.Value(depthTest); f.Value(depthWrite); f.Value(depthTestMode);
		f.Value(isCastingShadows); f.Value(cullFace); f.Value(isTransparent); f.Value(opacity);
		f.Value(depthFactor); f.Value(depthUnits); f.Value(depthBias);
		f.Value(blending); f.Value(sfactor); f.Value(dfactor); f.Value(mode);
		// Whatever the fields above miss, the uniforms carry: every value this
		// material sends, by name.
		f.Uniforms(GlobalUniforms); f.Uniforms(ModelUniforms); f.Uniforms(UserUniforms);
		return f.h;
	}

	uint32 GenericShaderMaterial::GetOrBuildGBufferProgram()
	{
		return GetOrBuildVariantProgram(ShaderUsage::DeferredRenderer_Gbuffer);
	}

	uint32 GenericShaderMaterial::GetOrBuildVariantProgram(const uint32 extraOptions)
	{
		// shaderID (this material's own options, chosen once at
		// construction - by SceneObjects::GenericMaterial for editor-added
		// primitives, or straight from a scene's serialized "options" field
		// for anything loaded from disk) never has DeferredRenderer_Gbuffer
		// OR'd in - nothing upstream of GenericShaderMaterial's constructor
		// ever sets it for ordinary scene content, only the editor's own
		// grid special-cases it. So this material's own shaderProgram is
		// Forward-only: PyrosShader.glsl's `void main()` for it never had
        // DEFERRED_GBUFFER defined, so it only declares/writes the single
		// `FragColor` output (already fully lit - ambient and any real
		// lights baked straight into diffuse.xyz by the DIFFUSE branch's
		// `diffuse = _diffuse*diffuse + ...`), never FragData_r/g/b/pbr.
		//
		// DeferredRenderer's G-buffer pass binds *whatever* Material->
		// GetShader() returns without checking that - so drawing this mesh
		// there wrote that already-lit FragColor into the G-buffer's first
		// attachment as if it were raw unlit albedo, and left the other
		// three MRT attachments as whatever the render pass's own default
		// load left them. The ambient composite pass then read that
		// attachment's alpha (meant to carry "diffuse.x*uAmbientLight.x")
		// back out as an ambient scalar - but this program's FragColor.w is
		// just diffuse.w*uOpacity (~1.0 for an opaque material, regardless
		// of the real scene ambient) - producing a uniform overbrightness
		// with no directional shape, on every single built-in-material mesh
		// in every Deferred scene, independent of light count. Found by
		// measuring pixel-exact Forward (204,204,204) vs Deferred
		// (255,255,255) on a zero-light scene with ambient=0.8 - Forward
		// matched ambient*255 exactly, Deferred didn't, and editing every
		// line inside PyrosShader.glsl's `#ifdef DEFERRED_GBUFFER` block
		// (including an unconditional solid-magenta override at the very
		// top of main()) had zero visible effect, proving this material's
		// bound program never had that branch compiled in at all.
		//
		// Fix: lazily compile+cache a sibling program for shaderID |
		// DeferredRenderer_Gbuffer (same ShadersList cache CustomShaderMaterial-
		// style branch selection already uses elsewhere in the engine), and
		// have the G-buffer pass bind *that* instead of this material's own
		// program - see DeferredRenderer::RenderScene()'s G-buffer loop.
		const uint32 gbufferOptions = shaderID | extraOptions;
		if (ShadersList.find(gbufferOptions) == ShadersList.end())
		{
			ShadersList[gbufferOptions] = new Shader();
			ShadersList[gbufferOptions]->currentMaterials = 0;
			ShadersList[gbufferOptions]->LoadShaderFile("shaders/PyrosShader.glsl");

			const std::string define = BuildShaderUsageDefines(gbufferOptions);
			// Say so when this fails. The G-buffer sibling is the ONLY way an
			// ordinary material's geometry reaches the G-buffer, so a failure
			// here does not degrade - the mesh writes depth (occluding what is
			// behind it) and then never appears, because there is nothing in
			// the G-buffer for the lighting pass to light. Returning the
			// handle regardless made that indistinguishable from "the object
			// is simply unlit", which is a bad hour to spend.
			bool ok = ShadersList[gbufferOptions]->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
			ok = ShadersList[gbufferOptions]->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str()) && ok;
			ok = ShadersList[gbufferOptions]->LinkProgram() && ok;
			if (!ok)
				echo("ERROR: shader variant for material options " + std::to_string(gbufferOptions)
					+ " failed to build - a G-buffer variant's meshes occlude but never light, an instanced one's do not draw.");
			else
				echo("TRACE: GBUFFER built sibling program for options " + std::to_string(gbufferOptions)
					+ " (program " + std::to_string(ShadersList[gbufferOptions]->ShaderProgram()) + ")");
		}
		return ShadersList[gbufferOptions]->ShaderProgram();
	}

	void GenericShaderMaterial::SetShininess(const f32 shininess)
	{
		this->Shininess = shininess;
		//SetUniformValue("uShininess", &this->Shininess);
	}
	GenericShaderMaterial::~GenericShaderMaterial()
	{
		// Delete Shaders
		if (ShadersList.find(shaderID) != ShadersList.end())
		{
			ShadersList[shaderID]->currentMaterials--;
			// Keep the compiled Shader* alive when the last material of
			// this options bitset goes away - demo switches tear down
			// every GenericShaderMaterial, and recreating PyrosShader
			// variants (shaderc + LinkProgram reflect) dominated Vulkan
			// reload time. GL's glCompileShader is cheap enough that
			// deleting was fine; Vulkan is not. Leaks one Shader per
			// distinct options mask for the process lifetime - tiny.
			(void)0;
		}
	}

	void GenericShaderMaterial::SetDisplacementHeight(const f32 height)
	{
		displacementHeight = height;
		// Unlike every sibling Set*() here, this unconditionally
		// dereferenced uDisplacementHeight - only ever non-null if the
		// material was constructed with ShaderUsage::ParallaxMapping (see
		// the constructor above), so calling this on any other material
		// (the common case) crashed. Real bug, found via
		// SceneSerializer round-tripping a plain Texture+Diffuse material
		// through this setter. Same lazy-create-if-null pattern as
		// SetReflectivity/SetMetallic/SetRoughness/SetSSREnabled.
		if (!uDisplacementHeight)
			uDisplacementHeight = AddUniform(Uniform("uDisplacementHeight", Uniforms::DataType::Float, &displacementHeight));
		else
			uDisplacementHeight->SetValue(&displacementHeight);
	}

	std::shared_ptr<GenericShaderMaterial> GenericShaderMaterial::Clone() const
	{
		std::shared_ptr<GenericShaderMaterial> m = std::make_shared<GenericShaderMaterial>(shaderID);
		m->SetColor(Kd);
		m->SetSpecular(Ks);
		m->SetShininess(Shininess);
		m->SetDisplacementHeight(displacementHeight);
		m->SetReflectivity(Reflectivity);
		m->SetMetallic(Metallic);
		m->SetRoughness(Roughness);
		m->SetSSREnabled(SSREnabled != 0.0f);
		m->SetAlphaCutoff(AlphaCutoff);
		m->Wind = Wind;
		m->Ke = Ke;
		m->Ka = Ka;

		auto tex = [this](int32 id) { return id >= 0 && id < (int32)Textures.size() ? Textures[id] : std::shared_ptr<Texture>(); };
		if (std::shared_ptr<Texture> t = tex(colorMapID)) m->SetColorMap(t);
		if (std::shared_ptr<Texture> t = tex(specularMapID)) m->SetSpecularMap(t);
		if (std::shared_ptr<Texture> t = tex(normalMapID)) m->SetNormalMap(t);
		if (std::shared_ptr<Texture> t = tex(displacementMapID)) m->SetDisplacementMap(t);
		if (std::shared_ptr<Texture> t = tex(envMapID)) m->SetEnvMap(t);
		if (std::shared_ptr<Texture> t = tex(refractMapID)) m->SetRefractMap(t);
		if (std::shared_ptr<Texture> t = tex(skyboxMapID)) m->SetSkyboxMap(t);
		if (std::shared_ptr<Texture> t = tex(metallicRoughnessMapID)) m->SetMetallicRoughnessMap(t);

		// IMaterial render state.
		m->SetOpacity(opacity);
		m->SetTransparencyFlag(isTransparent);
		m->SetCullFace(cullFace);
		m->isWireFrame = isWireFrame;
		m->isCastingShadows = isCastingShadows;
		m->depthTest = depthTest;
		m->depthWrite = depthWrite;
		m->forceDepthWrite = forceDepthWrite;
		m->depthTestMode = depthTestMode;
		m->depthBias = depthBias;
		m->depthFactor = depthFactor;
		m->depthUnits = depthUnits;
		m->blending = blending;
		m->sfactor = sfactor;
		m->dfactor = dfactor;
		m->mode = mode;
		return m;
	}

	void GenericShaderMaterial::AddTexture(const std::string &uniformName, const std::shared_ptr<Texture> &texture)
	{
		uint32 id = Textures.size();

		// Save on Textures Lis
		Textures.push_back(texture);
		// Set Uniform
		AddUniform(Uniform(uniformName.c_str(), Uniforms::DataType::Int, &id));
	}

	void GenericShaderMaterial::BindTextures()
	{
		for (std::vector<std::shared_ptr<Texture>>::iterator i = Textures.begin(); i != Textures.end(); i++)
		{
			(*i)->Bind();
		}
	}
	void GenericShaderMaterial::UnbindTextures()
	{
		for (std::vector<std::shared_ptr<Texture>>::reverse_iterator i = Textures.rbegin(); i != Textures.rend(); i++)
		{
			(*i)->Unbind();
		}
	}

	void GenericShaderMaterial::SetColor(const Vec4& color)
	{
		// Kd was declared but never actually written here - unlike every
		// other property on this class (Reflectivity/Metallic/Shininess/
		// etc all cache into their own member alongside the uniform),
		// this only ever touched the uniform's byte buffer, leaving Kd
		// permanently stale/default. Real bug, found via GetColor()
		// (added for scene serialization) reading it back as always
		// (0,0,0,0). Kd is otherwise unread anywhere in the engine, so
		// this is a pure fix, not a behavior change to rendering.
		Kd = color;
		if (!uColor)
			uColor = AddUniform(Uniform("uColor", Uniforms::DataType::Vec4, &Kd));
		else
			uColor->SetValue(&Kd);
	}
	void GenericShaderMaterial::SetSpecular(const Vec4& specularColor)
	{
		// See SetColor()'s identical comment - Ks was the same dead field.
		Ks = specularColor;
		if (!uSpecular)
			uSpecular = AddUniform(Uniform("uSpecular", Uniforms::DataType::Vec4, &Ks));
		else uSpecular->SetValue(&Ks);
	}

	void GenericShaderMaterial::SetColorMap(const std::shared_ptr<Texture> &colormap)
	{
		if (colorMapID == -1)
			colorMapID = Textures.size();
		else {
			Textures[colorMapID] = colormap;
			return;
		}
		// Save on List
		Textures.push_back(colormap);
		// Set Uniform
		AddUniform(Uniform("uColormap", Uniforms::DataType::Int, &colorMapID));
	}
	void GenericShaderMaterial::SetSpecularMap(const std::shared_ptr<Texture> &specular)
	{
		if (specularMapID == -1)
			specularMapID = Textures.size();
		else {
			Textures[specularMapID] = specular;
			return;
		}
		// Save on List
		Textures.push_back(specular);
		// Set Uniform
		AddUniform(Uniform("uSpecularmap", Uniforms::DataType::Int, &specularMapID));
	}
	void GenericShaderMaterial::SetMetallicRoughnessMap(const std::shared_ptr<Texture> &metallicRoughnessMap)
	{
		if (metallicRoughnessMapID == -1)
			metallicRoughnessMapID = Textures.size();
		else {
			Textures[metallicRoughnessMapID] = metallicRoughnessMap;
			return;
		}
		// Save on List
		Textures.push_back(metallicRoughnessMap);
		// Set Uniform
		AddUniform(Uniform("uMetallicRoughnessmap", Uniforms::DataType::Int, &metallicRoughnessMapID));
	}
	void GenericShaderMaterial::SetNormalMap(const std::shared_ptr<Texture> &normalmap)
	{
		if (normalMapID == -1)
			normalMapID = Textures.size();
		else {
			Textures[normalMapID] = normalmap;
			return;
		}
		// Save on List
		Textures.push_back(normalmap);
		// Set Uniform
		AddUniform(Uniform("uNormalmap", Uniforms::DataType::Int, &normalMapID));
	}
	void GenericShaderMaterial::SetDisplacementMap(const std::shared_ptr<Texture> &displacementmap)
	{
		if (displacementMapID == -1)
			displacementMapID = Textures.size();
		else {
			Textures[displacementMapID] = displacementmap;
			return;
		}
		// Save on List
		Textures.push_back(displacementmap);
		// Set Uniform
		AddUniform(Uniform("uDisplacementmap", Uniforms::DataType::Int, &displacementMapID));
	}
	void GenericShaderMaterial::SetEnvMap(const std::shared_ptr<Texture> &envmap)
	{
		if (envMapID == -1)
			envMapID = Textures.size();
		else {
			Textures[envMapID] = envmap;
			return;
		}
		// Save on List
		Textures.push_back(envmap);
		// Set Uniform
		AddUniform(Uniform("uEnvmap", Uniforms::DataType::Int, &envMapID));
	}
	void GenericShaderMaterial::SetReflectivity(const f32 reflectivity)
	{
		Reflectivity = reflectivity;
		if (!uReflectivity)
			uReflectivity = AddUniform(Uniform("uReflectivity", Uniforms::DataType::Float, &Reflectivity));
		else
			uReflectivity->SetValue(&Reflectivity);
	}
	void GenericShaderMaterial::SetMetallic(const f32 metallic)
	{
		Metallic = metallic;
		if (!uMetallic)
			uMetallic = AddUniform(Uniform("uMetallic", Uniforms::DataType::Float, &Metallic));
		else
			uMetallic->SetValue(&Metallic);
	}
	void GenericShaderMaterial::SetRoughness(const f32 roughness)
	{
		Roughness = roughness;
		if (!uRoughness)
			uRoughness = AddUniform(Uniform("uRoughness", Uniforms::DataType::Float, &Roughness));
		else
			uRoughness->SetValue(&Roughness);
	}
	void GenericShaderMaterial::SetSSREnabled(const bool enabled)
	{
		SSREnabled = enabled ? 1.0f : 0.0f;
		if (!uSSRReflective)
			uSSRReflective = AddUniform(Uniform("uSSRReflective", Uniforms::DataType::Float, &SSREnabled));
		else
			uSSRReflective->SetValue(&SSREnabled);
	}

	void GenericShaderMaterial::SetAlphaCutoff(const f32 cutoff)
	{
		AlphaCutoff = cutoff;
		if (!uAlphaCutoffUniform)
			uAlphaCutoffUniform = AddUniform(Uniform("uAlphaCutoff", Uniforms::DataType::Float, &AlphaCutoff));
		else
			uAlphaCutoffUniform->SetValue(&AlphaCutoff);
	}
	void GenericShaderMaterial::SetRefractMap(const std::shared_ptr<Texture> &refractmap)
	{
		if (refractMapID == -1)
			refractMapID = Textures.size();
		else {
			Textures[refractMapID] = refractmap;
			return;
		}
		// Save on List
		Textures.push_back(refractmap);
		// Set Uniform
		AddUniform(Uniform("uRefractmap", Uniforms::DataType::Int, &refractMapID));
	}
	void GenericShaderMaterial::SetSkyboxMap(const std::shared_ptr<Texture> &skyboxmap)
	{
		if (skyboxMapID == -1)
			skyboxMapID = Textures.size();
		else {
			Textures[skyboxMapID] = skyboxmap;
			return;
		}
		// Save on List
		Textures.push_back(skyboxmap);
		// Set Uniform
		AddUniform(Uniform("uSkyboxmap", Uniforms::DataType::Int, &skyboxMapID));
	}
	void GenericShaderMaterial::SetTextFont(Font* font)
	{
		const std::shared_ptr<Texture> &fontTex = font->GetTextureShared();
		if (fontMapID == -1)
			fontMapID = Textures.size();
		else {
			Textures[fontMapID] = fontTex;
			return;
		}
		// Save on List
		Textures.push_back(fontTex);
		// Set Uniform
		AddUniform(Uniform("uFontmap", Uniforms::DataType::Int, &fontMapID));
	}
	void GenericShaderMaterial::PreRender()
	{
		BindTextures();
	}
	void GenericShaderMaterial::AfterRender()
	{
		UnbindTextures();
	}
}
