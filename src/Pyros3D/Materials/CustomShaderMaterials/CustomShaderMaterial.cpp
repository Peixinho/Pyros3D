#include <vector>
#include <set>
//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

#include <map>
#include <memory>
#include <mutex>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <cassert>

namespace p3d
{

	// Shares whichever backend is currently active - same pattern as
	// Shaders.cpp/GeometryBuffer.cpp's own file-local Device() helper.
	static IRenderDevice& Device()
	{
		return GetActiveRenderDevice();
	}

	namespace {
		// Compiled programs by (defines, source). A scene with a thousand
		// terrain tiles holds a thousand materials built from one shader
		// file, and each compiled it - and each of its variants - again:
		// about a second of glslang per tile, and a hitch every time a
		// streamed cell brought one in. Weak: a program nobody uses is
		// freed as before. Failed compiles are not kept.
		std::mutex &ProgramCacheMutex() { static std::mutex m; return m; }
		std::map<std::string, std::weak_ptr<Shader> > &ProgramCache() { static std::map<std::string, std::weak_ptr<Shader> > c; return c; }

		std::shared_ptr<Shader> SharedProgram(const std::string &sourceText, const std::string &defines)
		{
			const std::string key = defines + '\x1f' + sourceText;
			{
				std::lock_guard<std::mutex> lock(ProgramCacheMutex());
				std::map<std::string, std::weak_ptr<Shader> >::iterator it = ProgramCache().find(key);
				if (it != ProgramCache().end())
				{
					if (std::shared_ptr<Shader> hit = it->second.lock()) return hit;
					ProgramCache().erase(it);
				}
			}
			std::shared_ptr<Shader> s = std::make_shared<Shader>();
			s->LoadShaderText(sourceText);
			s->CompileShader(ShaderType::VertexShader, std::string("#define VERTEX\n") + defines);
			s->CompileShader(ShaderType::FragmentShader, std::string("#define FRAGMENT\n") + defines);
			s->LinkProgram();
			if (s->ShaderProgram() != 0)
			{
				std::lock_guard<std::mutex> lock(ProgramCacheMutex());
				ProgramCache()[key] = s;
			}
			return s;
		}
	}

	namespace {
		struct RingKey
		{
			uint32 program, binding, size;
			std::string name;
			bool operator<(const RingKey &o) const
			{
				if (program != o.program) return program < o.program;
				if (binding != o.binding) return binding < o.binding;
				if (size != o.size) return size < o.size;
				return name < o.name;
			}
		};
		std::map<RingKey, uint32> &SharedRings() { static std::map<RingKey, uint32> r; return r; }
		int &LiveMaterials() { static int n = 0; return n; }
	}

	uint32 CustomShaderMaterial::SharedExtraBuffer(const uint32 program, const ExtraUniformsBlock &block)
	{
		RingKey key;
		key.program = program;
		key.binding = block.binding;
		key.size = block.size;
		key.name = block.blockName;
		std::map<RingKey, uint32>::iterator it = SharedRings().find(key);
		if (it != SharedRings().end()) return it->second;
		const uint32 handle = Device().CreateUniformBuffer(block.size, block.binding);
		if (handle) SharedRings()[key] = handle;
		return handle;
	}

	bool CustomShaderMaterial::IsSharedExtraBuffer(const uint32 handle)
	{
		if (!handle) return false;
		for (std::map<RingKey, uint32>::const_iterator it = SharedRings().begin(); it != SharedRings().end(); ++it)
			if (it->second == handle) return true;
		return false;
	}

	// Variants made ahead of need: see WarmVariants in the header.
	namespace {
		std::set<std::pair<uint64, uint32> > g_wantedVariants;
		void (*g_variantMade)(const uint64, const uint32) = NULL;
		bool g_warmingVariants = false;
		uint64 SourceId(const std::string &text)
		{
			uint64 h = 1469598103934665603ull;
			for (size_t i = 0; i < text.size(); i++) { h ^= (uchar)text[i]; h *= 1099511628211ull; }
			return h;
		}
	}
	void CustomShaderMaterial::WarmVariants(const std::vector<std::pair<uint64, uint32> > &wanted)
	{
		g_wantedVariants.insert(wanted.begin(), wanted.end());
	}
	void CustomShaderMaterial::WhenVariantMade(void (*told)(const uint64 source, const uint32 variant)) { g_variantMade = told; }
	void CustomShaderMaterial::BuildWantedVariants()
	{
		if (g_wantedVariants.empty() || shader == NULL || shader->GetShaderText().empty()) return;
		const uint64 id = SourceId(shader->GetShaderText());
		g_warmingVariants = true;
		for (uint32 index = 1; index < 8; index++)
			if (g_wantedVariants.count(std::make_pair(id, index))) GetOrBuildVariant((int)index);
		g_warmingVariants = false;
	}

	CustomShaderMaterial::CustomShaderMaterial(const std::string& ShaderFile) : IMaterial()
	{
		LiveMaterials()++;
		ShaderFilePath = ShaderFile;

		StringID number = (MakeStringID(ShaderFile)) + (MakeStringID(ShaderFile));

		{

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

			// Not Found, Then Load Shader
			InternalShader.reset(new Shader());
			shader = InternalShader.get();

			// No file is a placeholder the caller fills with SetShader() (the
			// Material Editor builds one per Custom material before compiling
			// the real shader). Compiling nothing put a pair of "Missing
			// entry point" failures in the log on every SPIR-V backend.
			if (!ShaderFile.empty())
			{
				shader->LoadShaderFile(ShaderFile.c_str());
				if (!shader->GetShaderText().empty())
				{
					// The same source as another material's: the same program.
					InternalShader = SharedProgram(shader->GetShaderText(), define);
					shader = InternalShader.get();
				}
				else
				{
					shader->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
					shader->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str());
					shader->LinkProgram();
				}
			}
		}

		// Get Shader Program
		shaderProgram = shader->ShaderProgram();

		PopulateAutoExtraUniforms();
		BuildWantedVariants();

		SetOpacity(1.0);
	}

	CustomShaderMaterial::CustomShaderMaterial(Shader* shader)
	{
		LiveMaterials()++;
		shaderProgram = shader->ShaderProgram();

		this->shader = shader;

		PopulateAutoExtraUniforms();
		BuildWantedVariants();
	}

	void CustomShaderMaterial::SetShader(Shader* shader)
	{
		// Releases the previously internally-owned shader, if any; a no-op
		// if `shader` was caller-owned.
		InternalShader.reset();

		// Copy shader
		this->shader = shader;
		shaderProgram = shader->ShaderProgram();

		// Every cached variant was compiled from the previous source.
		ResetVariants();

		PopulateAutoExtraUniforms();
		BuildWantedVariants();
	}

	void CustomShaderMaterial::AdoptShader(std::unique_ptr<Shader> ownedShader)
	{
		// caller-error otherwise; keep it a debug assert, not a runtime branch
		assert(ownedShader.get() == shader);
		InternalShader = std::move(ownedShader);
	}

	void CustomShaderMaterial::PopulateExtraUniformsFor(uint32 program, ExtraUniformsBlock outBlocks[2]) const
	{
		uint32 stages[2] = { ShaderType::VertexShader, ShaderType::FragmentShader };
		for (int i = 0; i < 2; i++)
		{
			uint32 binding = 0, size = 0;
			std::string blockName;
			std::map<std::string, uint32> offsets;
			if (!Device().GetAutoUniformBlockLayout(program, stages[i], binding, blockName, size, offsets))
				continue;
			ExtraUniformsBlock &block = outBlocks[i];
			// A previous shader compile (SetShader()/Apply - can fire every
			// ~350ms while live-editing a Custom material, see
			// MaterialEditor::DrawWindow's auto-apply) may have already
			// lazily allocated a GPU buffer for this slot via
			// SendExtraUniforms(). It's about to be replaced (blockName/
			// size/offsets can all differ for the new program) - free it
			// now instead of just dropping the handle, which orphaned one
			// GPU uniform buffer (sized up to 64MB for a dynamic binding -
			// see IsPerObjectDynamicBinding()) per recompile.
			if (block.bufferHandle != 0)
			{
				if (!IsSharedExtraBuffer(block.bufferHandle)) Device().DestroyUniformBuffer(block.bufferHandle);
				block.bufferHandle = 0;
			}
			block.binding = binding;
			block.blockName = blockName;
			block.size = size;
			block.offsets = offsets;
			block.scratch.assign(size, 0);
		}
	}

	void CustomShaderMaterial::PopulateAutoExtraUniforms()
	{
		PopulateExtraUniformsFor(shaderProgram, extraUniforms);
	}

	uint32 CustomShaderMaterial::GetOrBuildGBufferProgram()
	{
		return GetOrBuildVariant(1);
	}

	// The same platform #defines the file constructor compiles with - every
	// variant must match it, or a variant would pick a different
	// precision/profile path than the program it stands in for.
	static std::string PlatformDefines()
	{
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
		return define;
	}

	uint32 CustomShaderMaterial::GetOrBuildVariant(int index)
	{
		// A CustomShaderMaterial loaded fresh from a scene (SceneSerializer::
		// BuildMaterial's "custom" kind) only ever compiles `shader` once,
		// Forward-only (see the constructors above - neither ever defines
		// DEFERRED_GBUFFER, and nothing checks the active renderer type).
		// DeferredRenderer's G-buffer pass binds whatever program this
		// material has without checking that, so a scene-loaded custom
		// material's real per-fragment-lighting FragColor code ran during
		// the G-buffer MRT pass instead of the DEFERRED_GBUFFER branch that
		// bakes ambient into FragData_r/g/b's alpha channels - leaving those
		// alpha channels stale/undefined for secondpassAmbient.glsl to read
		// back out as the ambient factor. Found via a GPU frame capture:
		// the G-buffer pass's Color0-3 attachments matched the shader's
		// DEFERRED_GBUFFER branch exactly for a GenericShaderMaterial grid,
		// but a CustomShaderMaterial sphere in the same scene measured
		// visibly brighter than Forward with no direct-lighting shape to
		// it - the signature of an unset/garbage ambient scalar, not a
		// double-counted one (the composite shader's math has no room for
		// double-counting - see secondpassAmbient.glsl).
		//
		// Fix: lazily compile+cache a DEFERRED_GBUFFER sibling from the
		// same source text (LoadShaderFile if this material has a real
		// file path, LoadShaderText from shader->GetShaderText() otherwise
		// - the Shader* constructor / SetShader() paths have no path), and
		// have the G-buffer pass bind that instead - see
		// DeferredRenderer::RenderScene()'s G-buffer loop.
		//
		// The forward and skinned variants follow the same pattern - see
		// UseVariantForNextDraw()'s comment in the header.
		ProgramVariant &v = variants[index];
		if (!v.shader && !v.failed)
		{
			if (!shader)
			{
				v.failed = true;
				return 0;
			}
			const bool gbuffer = (index & 1) != 0;
			const bool skinned = (index & 2) != 0;
			const bool shadow = (index & 4) != 0;
			std::string defines = PlatformDefines();
			if (gbuffer) defines += "#define DEFERRED_GBUFFER\n";
			if (skinned) defines += "#define SKINNING\n";
			if (shadow) defines += "#define SHADOW_DEPTH\n";
			// The own program's source text first, the file only as a
			// fallback. It is the same source, with includes already inlined
			// - and ShaderFilePath is not always openable from here: the
			// Material Editor records it project-relative (for scene
			// portability), which the editor's working directory does not
			// resolve, so every variant compiled from the path in the editor
			// came up "COULDN'T OPEN/INCLUDE FILE" and silently fell back.
			// Shared with every other material of the same source.
			if (!shader->GetShaderText().empty())
				v.shader = SharedProgram(shader->GetShaderText(), defines);
			else
			{
				v.shader.reset(new Shader());
				if (!ShaderFilePath.empty()) v.shader->LoadShaderFile(ShaderFilePath.c_str());
				v.shader->CompileShader(ShaderType::VertexShader, std::string("#define VERTEX\n") + defines);
				v.shader->CompileShader(ShaderType::FragmentShader, std::string("#define FRAGMENT\n") + defines);
				v.shader->LinkProgram();
			}

			if (v.shader->ShaderProgram() == 0)
			{
				// This material's source has no usable branch for this
				// variant (e.g. a hand-written shader that never declares
				// DEFERRED_GBUFFER) - give up permanently rather than
				// recompiling a failing variant every draw. The caller
				// falls back to drawing with the own program.
				v.shader.reset();
				v.failed = true;
				return 0;
			}

			PopulateExtraUniformsFor(v.shader->ShaderProgram(), v.extraUniforms);
			// (made when first needed, in the middle of things: whoever keeps the list is told)
			if (!g_warmingVariants && g_variantMade != NULL && !shader->GetShaderText().empty())
				g_variantMade(SourceId(shader->GetShaderText()), (uint32)index);
		}
		return v.shader ? v.shader->ShaderProgram() : 0;
	}

	void CustomShaderMaterial::ResetVariants()
	{
		for (int i = 0; i < 8; i++)
		{
			for (int b = 0; b < 2; b++)
				if (variants[i].extraUniforms[b].bufferHandle != 0 && !IsSharedExtraBuffer(variants[i].extraUniforms[b].bufferHandle))
					Device().DestroyUniformBuffer(variants[i].extraUniforms[b].bufferHandle);
			variants[i].shader.reset();
			variants[i].failed = false;
			for (int b = 0; b < 2; b++)
				variants[i].extraUniforms[b] = ExtraUniformsBlock();
		}
	}

	bool CustomShaderMaterial::UseVariantForNextDraw(bool gbuffer, bool skinned)
	{
		const bool ownGBuffer = hasKnownShaderBranch && deferredGBufferBranch;
		// A source that never mentions SKINNING has nothing to compile -
		// every hand-written shader, and generated ones from before the
		// template grew skinning. Draw those with the plain program, as
		// before.
		if (skinned && (!shader || shader->GetShaderText().find("SKINNING") == std::string::npos))
			skinned = false;
		// The own program is always the non-skinned variant of its branch.
		// A material with an unknown branch was built by the file or
		// Shader* constructor, which compile Forward.
		if (gbuffer == ownGBuffer && !skinned)
			return false;

		return SwapToVariant((gbuffer ? 1 : 0) | (skinned ? 2 : 0));
	}

	bool CustomShaderMaterial::SwapToVariant(int index)
	{
		const uint32 program = GetOrBuildVariant(index);
		if (program == 0)
			return false;
		ownExtraUniformsBackup[0] = extraUniforms[0];
		ownExtraUniformsBackup[1] = extraUniforms[1];
		extraUniforms[0] = variants[index].extraUniforms[0];
		extraUniforms[1] = variants[index].extraUniforms[1];
		shaderProgram = program;
		activeVariant = index;
		return true;
	}

	bool CustomShaderMaterial::HasCustomShadow() const
	{
		// The marker MaterialCodegen writes into a shader whose graph uses
		// Vertex Offset or Alpha Clip.
		return shader && shader->GetShaderText().find("P3D_CUSTOM_SHADOW") != std::string::npos;
	}

	bool CustomShaderMaterial::UseShadowVariantForNextDraw(bool skinned)
	{
		if (skinned && shader->GetShaderText().find("SKINNING") == std::string::npos)
			skinned = false;
		return SwapToVariant(4 | (skinned ? 2 : 0));
	}

	void CustomShaderMaterial::RestoreOwnProgram()
	{
		// Persist any bufferHandle SendExtraUniforms lazily allocated
		// during the variant's draw, so the next one reuses it instead of
		// leaking/recreating a GPU buffer every frame.
		if (activeVariant >= 0)
		{
			variants[activeVariant].extraUniforms[0] = extraUniforms[0];
			variants[activeVariant].extraUniforms[1] = extraUniforms[1];
		}
		activeVariant = -1;
		extraUniforms[0] = ownExtraUniformsBackup[0];
		extraUniforms[1] = ownExtraUniformsBackup[1];
		shaderProgram = shader->ShaderProgram();
	}

	void CustomShaderMaterial::AddGeneratedShaderUniforms()
	{
		// AddUniform appends unconditionally, and this runs on every Apply
		// of a live material - skip names already registered.
		auto add = [this](const char* name, uint32 usage) {
			for (const std::list<Uniform>* l : { &GlobalUniforms, &ModelUniforms })
				for (const Uniform &u : *l)
					if (u.Name == name) return;
			AddUniform(Uniform(name, usage));
		};
		add("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix);
		add("uViewMatrix", Uniforms::DataUsage::ViewMatrix);
		add("uModelMatrix", Uniforms::DataUsage::ModelMatrix);
		add("uCameraPosition", Uniforms::DataUsage::CameraPosition);
		add("uTime", Uniforms::DataUsage::Timer);
		add("uAmbientLight", Uniforms::DataUsage::GlobalAmbientLight);
		add("uAmbientSky", Uniforms::DataUsage::AmbientSky);
		add("uAmbientEquator", Uniforms::DataUsage::AmbientEquator);
		add("uAmbientGround", Uniforms::DataUsage::AmbientGround);
		add("uAmbientParams", Uniforms::DataUsage::AmbientParams);
		add("uAmbientSH", Uniforms::DataUsage::AmbientSH);
		// only a shader that reads them (see IRenderer::SetShaderGlobal)
		if (shader && shader->GetShaderText().find("uGlobals") != std::string::npos)
			add("uGlobals", Uniforms::DataUsage::ShaderGlobals);
		add("uLights", Uniforms::DataUsage::Lights);
		add("uNumberOfLights", Uniforms::DataUsage::NumberOfLights);
		// Forward branch only - under Deferred the light passes shadow the
		// G-buffer themselves, and the G-buffer program doesn't declare
		// these, so they are skipped there.
		add("uDirectionalShadowMaps", Uniforms::DataUsage::DirectionalShadowMap);
		add("uDirectionalDepthsMVP", Uniforms::DataUsage::DirectionalShadowMatrix);
		add("uDirectionalShadowFar", Uniforms::DataUsage::DirectionalShadowFar);
		add("uNumberOfDirectionalShadows", Uniforms::DataUsage::NumberOfDirectionalShadows);
		add("uPointShadowMaps", Uniforms::DataUsage::PointShadowMap);
		add("uPointDepthsMVP", Uniforms::DataUsage::PointShadowMatrix);
		add("uNumberOfPointShadows", Uniforms::DataUsage::NumberOfPointShadows);
		add("uSpotShadowMaps", Uniforms::DataUsage::SpotShadowMap);
		add("uSpotDepthsMVP", Uniforms::DataUsage::SpotShadowMatrix);
		add("uNumberOfSpotShadows", Uniforms::DataUsage::NumberOfSpotShadows);
		add("uBoneMatrix", Uniforms::DataUsage::Skinning);
		// Misleadingly named: this is what gates IRenderer::BindShadowMaps(),
		// i.e. whether a draw gets the shadow maps bound at all - without it
		// the samplers above never get a unit. Casting is decided per object
		// by RenderingComponent::EnableCastShadows(), not here.
		EnableCastingShadows();
	}

	void CustomShaderMaterial::RegisterParameterUniform(const std::string &name, Parameter &p)
	{
		const std::string uniformName = ParameterUniformName(name);
		for (std::list<Uniform>::iterator i = UserUniforms.begin(); i != UserUniforms.end(); ++i)
			if (i->Name == uniformName) { UserUniforms.erase(i); break; }
		if (p.isVector)
			p.handle = AddUniform(Uniform(uniformName, Uniforms::DataType::Vec4, &p.value));
		else
			p.handle = AddUniform(Uniform(uniformName, Uniforms::DataType::Float, &p.value.x));
	}

	void CustomShaderMaterial::DeclareParameter(const std::string &name, bool isVector, const Vec4 &defaultValue)
	{
		Parameter &p = parameters[name];
		p.isVector = isVector;
		p.value = defaultValue;
		RegisterParameterUniform(name, p);
	}

	void CustomShaderMaterial::SetParameter(const std::string &name, const Vec4 &value)
	{
		std::map<std::string, Parameter>::iterator it = parameters.find(name);
		if (it == parameters.end()) { DeclareParameter(name, true, value); return; }
		it->second.value = value;
		if (it->second.handle)
			it->second.handle->SetValue(it->second.isVector ? (void*)&it->second.value : (void*)&it->second.value.x);
	}

	void CustomShaderMaterial::SetParameter(const std::string &name, f32 value)
	{
		std::map<std::string, Parameter>::iterator it = parameters.find(name);
		if (it == parameters.end()) { DeclareParameter(name, false, Vec4(value, value, value, value)); return; }
		SetParameter(name, it->second.isVector ? Vec4(value, value, value, value) : Vec4(value, 0.f, 0.f, 0.f));
	}

	Vec4 CustomShaderMaterial::GetParameter(const std::string &name) const
	{
		std::map<std::string, Parameter>::const_iterator it = parameters.find(name);
		return it == parameters.end() ? Vec4() : it->second.value;
	}

	bool CustomShaderMaterial::IsVectorParameter(const std::string &name) const
	{
		std::map<std::string, Parameter>::const_iterator it = parameters.find(name);
		return it != parameters.end() && it->second.isVector;
	}

	std::vector<std::string> CustomShaderMaterial::GetParameterNames() const
	{
		std::vector<std::string> names;
		for (std::map<std::string, Parameter>::const_iterator it = parameters.begin(); it != parameters.end(); ++it)
			names.push_back(it->first);
		return names;
	}

	void CustomShaderMaterial::RemoveParameter(const std::string &name)
	{
		std::map<std::string, Parameter>::iterator it = parameters.find(name);
		if (it == parameters.end()) return;
		const std::string uniformName = ParameterUniformName(name);
		for (std::list<Uniform>::iterator i = UserUniforms.begin(); i != UserUniforms.end(); ++i)
			if (i->Name == uniformName) { UserUniforms.erase(i); break; }
		parameters.erase(it);
	}

	CustomShaderMaterial::~CustomShaderMaterial()
	{
		ResetVariants();
		// IMaterial's destructor frees extraUniforms[]' buffers: not the
		// shared ones.
		for (int i = 0; i < 2; i++)
			if (IsSharedExtraBuffer(extraUniforms[i].bufferHandle)) extraUniforms[i].bufferHandle = 0;
		if (--LiveMaterials() == 0 && IsActiveRenderDeviceSet())
		{
			for (std::map<RingKey, uint32>::iterator it = SharedRings().begin(); it != SharedRings().end(); ++it)
				Device().DestroyUniformBuffer(it->second);
			SharedRings().clear();
		}
	}

	void CustomShaderMaterial::PreRender()
	{
		for (std::vector<std::shared_ptr<Texture>>::iterator i = textures.begin(); i != textures.end(); i++)
			(*i)->Bind();
	}

	void CustomShaderMaterial::AfterRender()
	{
		for (std::vector<std::shared_ptr<Texture>>::reverse_iterator i = textures.rbegin(); i != textures.rend(); i++)
			(*i)->Unbind();
	}

	void CustomShaderMaterial::AddSampler(const std::string &uniformName, const std::shared_ptr<Texture> &tex)
	{
		int32 imgID = (int32)textures.size();
		textures.push_back(tex);
		samplerNames.push_back(uniformName);
		AddUniform(Uniform(uniformName, Uniforms::DataType::Int, &imgID));
	}

	void CustomShaderMaterial::ClearSamplers()
	{
		for (const std::string &name : samplerNames)
			for (std::list<Uniform>::iterator i = UserUniforms.begin(); i != UserUniforms.end(); ++i)
				if (i->Name == name) { UserUniforms.erase(i); break; }
		textures.clear();
		samplerNames.clear();
	}
}
