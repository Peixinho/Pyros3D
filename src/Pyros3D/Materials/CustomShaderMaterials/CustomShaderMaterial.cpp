//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

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

	CustomShaderMaterial::CustomShaderMaterial(const std::string& ShaderFile) : IMaterial()
	{
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

			shader->LoadShaderFile(ShaderFile.c_str());
			shader->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
			shader->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str());

			shader->LinkProgram();
		}

		// Get Shader Program
		shaderProgram = shader->ShaderProgram();

		PopulateAutoExtraUniforms();

		SetOpacity(1.0);
	}

	CustomShaderMaterial::CustomShaderMaterial(Shader* shader)
	{
		shaderProgram = shader->ShaderProgram();

		this->shader = shader;

		PopulateAutoExtraUniforms();
	}

	void CustomShaderMaterial::SetShader(Shader* shader)
	{
		// Releases the previously internally-owned shader, if any; a no-op
		// if `shader` was caller-owned.
		InternalShader.reset();

		// Copy shader
		this->shader = shader;
		shaderProgram = shader->ShaderProgram();

		PopulateAutoExtraUniforms();
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
				Device().DestroyUniformBuffer(block.bufferHandle);
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
		if (!gbufferShader && !gbufferCompileFailed)
		{
			gbufferShader.reset(new Shader());

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
			const std::string deferredDefine = "#define DEFERRED_GBUFFER\n";

			if (!ShaderFilePath.empty())
				gbufferShader->LoadShaderFile(ShaderFilePath.c_str());
			else
				gbufferShader->LoadShaderText(shader->GetShaderText());

			gbufferShader->CompileShader(ShaderType::VertexShader, std::string("#define VERTEX\n") + deferredDefine + define);
			gbufferShader->CompileShader(ShaderType::FragmentShader, std::string("#define FRAGMENT\n") + deferredDefine + define);
			gbufferShader->LinkProgram();

			if (gbufferShader->ShaderProgram() == 0)
			{
				// This material's source has no usable DEFERRED_GBUFFER
				// branch (e.g. a hand-written shader that never declares
				// one) - give up permanently rather than recompiling a
				// failing variant every draw. Caller falls back to
				// drawing with the Forward program, unchanged from
				// before this mechanism existed.
				gbufferShader.reset();
				gbufferCompileFailed = true;
				return 0;
			}

			PopulateExtraUniformsFor(gbufferShader->ShaderProgram(), gbufferExtraUniforms);
		}
		return gbufferShader ? gbufferShader->ShaderProgram() : 0;
	}

	bool CustomShaderMaterial::UseGBufferProgramForNextDraw()
	{
		const uint32 program = GetOrBuildGBufferProgram();
		if (program == 0)
			return false;
		ownExtraUniformsBackup[0] = extraUniforms[0];
		ownExtraUniformsBackup[1] = extraUniforms[1];
		extraUniforms[0] = gbufferExtraUniforms[0];
		extraUniforms[1] = gbufferExtraUniforms[1];
		shaderProgram = program;
		return true;
	}

	void CustomShaderMaterial::RestoreOwnProgram()
	{
		// Persist any bufferHandle SendExtraUniforms lazily allocated
		// during the G-buffer draw, so the next one reuses it instead of
		// leaking/recreating a GPU buffer every frame.
		gbufferExtraUniforms[0] = extraUniforms[0];
		gbufferExtraUniforms[1] = extraUniforms[1];
		extraUniforms[0] = ownExtraUniformsBackup[0];
		extraUniforms[1] = ownExtraUniformsBackup[1];
		shaderProgram = shader->ShaderProgram();
	}

	CustomShaderMaterial::~CustomShaderMaterial() = default;

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
		AddUniform(Uniform(uniformName, Uniforms::DataType::Int, &imgID));
	}

	void CustomShaderMaterial::ClearSamplers()
	{
		textures.clear();
		UserUniforms.clear();
	}
}
