//============================================================================
// Name        : CustomShaderMaterials.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//============================================================================

#ifndef CUSTOMSHADERMATERIAL_H
#define CUSTOMSHADERMATERIAL_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Other/Export.h>
#include <iostream>
#include <map>
#include <memory>

namespace p3d
{

	class PYROS3D_API CustomShaderMaterial : public IMaterial
	{

	public:

		CustomShaderMaterial(const std::string &ShaderFile);
		CustomShaderMaterial(Shader* shader);
		void SetShader(Shader* shader);
		// Transfers ownership of a Shader previously wired in via SetShader()
		// into this material, so it survives whatever used to own it (e.g. a
		// closed MaterialEditorDocument). ownedShader.get() must equal the
		// currently-active `shader` pointer.
		void AdoptShader(std::unique_ptr<Shader> ownedShader);

		virtual ~CustomShaderMaterial();

		virtual void PreRender();

		virtual void AfterRender();

		// Bind tex as next sampler unit and register uniformName = unit index.
		void AddSampler(const std::string &uniformName, const std::shared_ptr<Texture> &tex);

		// Removes every texture added via AddSampler() and their unit-index
		// uniforms (AddSampler registers those as Usage::Other, same bucket
		// as any other plain user uniform) - only safe on a material whose
		// Usage::Other uniforms are exclusively sampler units it added
		// itself; a subclass that also uses AddUniform(..., Other, ...) for
		// its own non-sampler tunables (e.g. ParticleSystemMaterial) must
		// not call this. Used by the Material Editor to reset bindings
		// before re-wiring a node graph whose Texture nodes may have
		// changed count/order since the last Apply.
		void ClearSamplers();

		std::vector<std::shared_ptr<Texture>> textures;

		// Empty when constructed from a raw Shader* - that path has no
		// recoverable source, callers (e.g. scene serialization) must
		// treat an empty string as "can't be saved/reconstructed".
		const std::string &GetShaderFile() const { return ShaderFilePath; }

		// The underlying Shader* itself - needed so a path-less material
		// (built from a raw Shader*) can still fall back to embedding
		// Shader::GetShaderText()'s real cached source.
		Shader* GetShaderObject() const { return shader; }

		// Which DEFERRED_GBUFFER branch `shader`'s currently-bound program
		// was actually compiled for - unset (HasKnownShaderBranch() ==
		// false) until the first MarkShaderBranch() call. Lets a
		// Forward<->Deferred renderer switch that fails to recompile a
		// material detect "the shader still bound is from the OTHER
		// branch" before letting it run inside the wrong render pass: a
		// Forward branch's real per-fragment lighting code executing
		// during the Deferred G-buffer's MRT pass produces lit-looking
		// output out of thin air (reading whatever's in the shared
		// LightsUBO) plus wrong attachment counts corrupting depth/
		// compositing, rather than a clean error.
		bool HasKnownShaderBranch() const { return hasKnownShaderBranch; }
		bool IsShaderCompiledForDeferredGBuffer() const { return deferredGBufferBranch; }
		void MarkShaderBranch(bool deferredGBuffer) { hasKnownShaderBranch = true; deferredGBufferBranch = deferredGBuffer; }

		// True once this material's own `shader` has been probed and found
		// to already be the DEFERRED_GBUFFER branch - lets a caller skip
		// the swap below when it isn't needed (matches
		// GenericShaderMaterial::IsCompiledForGBuffer()'s role).
		bool IsCompiledForGBuffer() const { return hasKnownShaderBranch && deferredGBufferBranch; }
		// Lazily compiles (once, cached) a DEFERRED_GBUFFER sibling of
		// this material's own shader from the same source text and
		// returns its program handle, without touching `shader` itself.
		// A scene-loaded CustomShaderMaterial (SceneSerializer::
		// BuildMaterial's "custom" kind) is only ever compiled Forward -
		// see the .cpp for why this exists and what it fixes.
		uint32 GetOrBuildGBufferProgram();
		// Paired calls around a single G-buffer RenderObject() -
		// DeferredRenderer::RenderScene() is the only intended caller.
		// Swaps both shaderProgram and extraUniforms[] (the two branches
		// have different std140 layouts - the Forward branch declares
		// uLights[]/uNumberOfLights that DEFERRED_GBUFFER doesn't) to the
		// G-buffer sibling for exactly that one draw. Returns false (no
		// swap performed, nothing to restore) if this material's source
		// has no usable DEFERRED_GBUFFER branch to compile at all - e.g. a
		// hand-written CustomShaderMaterial shader that never declares one
		// - so the caller falls back to drawing with the Forward program
		// as-is, same as before this mechanism existed, instead of
		// binding a failed (program 0) link. Only call RestoreOwnProgram()
		// when this returned true.
		bool UseGBufferProgramForNextDraw();
		void RestoreOwnProgram();

	protected:

		std::string ShaderFilePath;

		// Generic Vulkan auto-fix hookup - see IRenderDevice::
		// GetAutoUniformBlockLayout()'s comment. Called from every place
		// this class finishes wiring up `shader` (both constructors and
		// SetShader()); a no-op on GL and on any shader whose loose
		// uniforms are already hand-wrapped in an explicit UBO (every
		// shader this engine ships today) - only actually populates
		// extraUniforms[] for a shader that genuinely had nothing but
		// plain, unlabeled loose uniforms. Runs before a subclass's own
		// constructor body (base-class constructors always run first), so
		// a subclass that still hand-assigns extraUniforms[] itself
		// (CustomMaterialExample/ParticleMaterial/Lua water_material) simply
		// overwrites this afterward - manual authoring always wins.
		void PopulateAutoExtraUniforms();

		// Shader
		Shader* shader;
		// Owned only when `shader` was built internally (from a file) or
		// handed over by an AdoptShader() call - null when `shader` points
		// at a caller-owned Shader (SetShader() alone never takes ownership).
		std::unique_ptr<Shader> InternalShader;

		bool hasKnownShaderBranch = false;
		bool deferredGBufferBranch = false;

		// Lazily-built DEFERRED_GBUFFER sibling of `shader`, and its own
		// extraUniforms layout - see GetOrBuildGBufferProgram()/
		// UseGBufferProgramForNextDraw(). Null until first requested.
		std::unique_ptr<Shader> gbufferShader;
		bool gbufferCompileFailed = false;
		ExtraUniformsBlock gbufferExtraUniforms[2];
		// Holds this material's own (Forward) extraUniforms[] while
		// UseGBufferProgramForNextDraw()'s swap is in effect, so
		// RestoreOwnProgram() can put it back - see both methods' .cpp.
		ExtraUniformsBlock ownExtraUniformsBackup[2];

		// Shared by PopulateAutoExtraUniforms() and
		// GetOrBuildGBufferProgram() - fills outBlocks[VertexShader/
		// FragmentShader] from `program`'s own reflected std140 layout.
		void PopulateExtraUniformsFor(uint32 program, ExtraUniformsBlock outBlocks[2]) const;
	};

}

#endif /* CUSTOMSHADERMATERIAL_H */
