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
#include <string>
#include <vector>

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

		// The GPU ring behind an extra-uniform block, one per (program,
		// block): every draw writes its own slot, so all the materials drawn
		// with one program can share it. One ring per material was up to
		// 64 MB each (a dynamic binding's ring) - a streamed terrain with a
		// material per tile spent ~40 MB of GPU memory per cell on them.
		// IRenderer asks for it on first use; released when the last
		// CustomShaderMaterial goes.
		static uint32 SharedExtraBuffer(const uint32 program, const ExtraUniformsBlock &block);
		static bool IsSharedExtraBuffer(const uint32 handle);

		virtual void PreRender();

		virtual void AfterRender();

		// Bind tex as next sampler unit and register uniformName = unit index.
		void AddSampler(const std::string &uniformName, const std::shared_ptr<Texture> &tex);

		// Removes every texture added via AddSampler() and exactly their
		// unit-index uniforms, by name. Every other user uniform stays: it
		// used to clear UserUniforms wholesale, which also freed IMaterial's
		// own uOpacity - leaving IMaterial::opacityHandle dangling, so the
		// next SetOpacity() wrote through freed memory - and dropped every
		// parameter. Used by the Material Editor to reset bindings before
		// re-wiring a node graph whose Texture nodes may have changed
		// count/order since the last Apply.
		void ClearSamplers();

		std::vector<std::shared_ptr<Texture>> textures;

		// The uniform name AddSampler() bound each entry of `textures` to,
		// parallel to it and in the same order (sampler units are handed out
		// sequentially). Without this the name survives only inside
		// UserUniforms as an opaque Int holding a unit index, with no way to
		// map it back to a texture - which is why scene serialization used to
		// drop a custom material's textures entirely: it wrote the shader and
		// nothing else, so a reloaded scene sampled an unbound sampler2D.
		const std::vector<std::string>& GetSamplerNames() const { return samplerNames; }

		// Empty when constructed from a raw Shader* - that path has no
		// recoverable source, callers (e.g. scene serialization) must
		// treat an empty string as "can't be saved/reconstructed".
		const std::string &GetShaderFile() const { return ShaderFilePath; }

		// Records where the shader source lives for a material whose Shader*
		// was compiled by the caller rather than by the file constructor -
		// the Material Editor, which compiles its generated GLSL itself and
		// hands over the linked program. Without this such a material looks
		// path-less, so SceneSerializer embeds a *copy* of the shader text in
		// the scene, and from then on every edit of the .mat stops reaching
		// the objects using it: they keep recompiling their frozen copy.
		// Store it project-relative so the scene stays portable.
		void SetShaderFile(const std::string &path) { ShaderFilePath = path; }

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

		// Program variants. One CustomShaderMaterial is drawn under up to
		// four different conditions - G-buffer pass or a forward pass
		// (ForwardRenderer, or DeferredRenderer's translucent pass), on a
		// static or a skinned mesh - while `shader` is compiled for only
		// one of them. Each other variant is compiled lazily (once, cached)
		// from the same source text with DEFERRED_GBUFFER / SKINNING
		// defined or not, and never touches `shader` itself.
		//
		// Why each exists:
		//  - G-buffer: a scene-loaded material is compiled Forward only
		//    (see the .cpp), and its lighting code must not run inside the
		//    MRT pass.
		//  - Forward from a G-buffer material: a transparent object is drawn
		//    in DeferredRenderer's translucent pass, a single-target forward
		//    pass, so the G-buffer program there wrote FragData_r unlit.
		//  - Skinned: bone attributes exist only on skinned geometry, and a
		//    pipeline whose shader declares an attribute the mesh does not
		//    have fails to build on Vulkan, so the same material needs a
		//    SKINNING and a plain program side by side. Only built for a
		//    source that mentions SKINNING at all.
		//
		// UseVariantForNextDraw() swaps shaderProgram and extraUniforms[]
		// (every variant has its own std140 layout) for exactly one
		// RenderObject(). Returns false when no swap is needed or the
		// variant cannot be built - the caller then draws with the own
		// program as before. Call RestoreOwnProgram() only after true.
		// Scoped to exactly CustomShaderMaterial by the renderers (typeid):
		// subclasses hand-assign extraUniforms[] and are left alone.
		bool UseVariantForNextDraw(bool gbuffer, bool skinned);
		// The shadow pass's variant (SHADOW_DEPTH): only the vertex offset
		// and the alpha cutout, writing depth. Worth using only when
		// HasCustomShadow() - a generated shader that moves vertices or cuts
		// holes, so its shadow must too; anything else is better served by
		// the renderer's shared shadow material. Same pairing rule as above:
		// RestoreOwnProgram() only after true.
		bool HasCustomShadow() const;
		bool UseShadowVariantForNextDraw(bool skinned);
		// True between a successful UseShadowVariantForNextDraw() and
		// RestoreOwnProgram() - what makes IRenderer build this draw's
		// pipeline against the shadow render pass.
		bool IsDrawingShadowVariant() const { return activeVariant >= 0 && (activeVariant & 4) != 0; }
		// The G-buffer, non-skinned variant - kept for existing callers.
		uint32 GetOrBuildGBufferProgram();
		bool UseGBufferProgramForNextDraw() { return UseVariantForNextDraw(true, false); }
		void RestoreOwnProgram();

		// The uniform set every Material Editor shader (MaterialCodegen)
		// is written against: matrices, camera, time, lights, the
		// environment ambient, the forward branch's shadow maps and the
		// bone palette. There is exactly one list, here, because it used
		// to be copied into the editor's Apply path, its recompile path and
		// the scene loader - and the loader's copy stopped at uLights, so
		// every custom material in a loaded scene received no shadows.
		// Idempotent; names the program doesn't declare are skipped.
		void AddGeneratedShaderUniforms();

		// Named material parameters - the Material Editor's Float/Color
		// Parameter nodes. `name` is what the graph shows; the shader
		// uniform is "p_" + name, so a parameter can never collide with an
		// engine uniform. Declare fixes the uniform's GLSL type (float or
		// vec4); Set on an undeclared name declares it from the value given.
		// Values survive SetShader()/ClearSamplers() and round-trip through
		// scene files. Shared by every object using this material.
		void DeclareParameter(const std::string &name, bool isVector, const Vec4 &defaultValue);
		void SetParameter(const std::string &name, const Vec4 &value);
		void SetParameter(const std::string &name, f32 value);
		bool HasParameter(const std::string &name) const { return parameters.find(name) != parameters.end(); }
		Vec4 GetParameter(const std::string &name) const;
		bool IsVectorParameter(const std::string &name) const;
		std::vector<std::string> GetParameterNames() const;
		void RemoveParameter(const std::string &name);
		static std::string ParameterUniformName(const std::string &name) { return "p_" + name; }

	protected:

		std::string ShaderFilePath;

		// Parallel to `textures` - see GetSamplerNames().
		std::vector<std::string> samplerNames;

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
		// Shared: programs compiled from the same source and defines are one
		// program for every material that uses them (see the .cpp).
		std::shared_ptr<Shader> InternalShader;

		bool hasKnownShaderBranch = false;
		bool deferredGBufferBranch = false;

		// Lazily-built variants of `shader`, index = (gbuffer ? 1 : 0) |
		// (skinned ? 2 : 0) | (shadow ? 4 : 0), each with its own
		// extraUniforms layout - see UseVariantForNextDraw(). Dropped
		// whenever SetShader() changes the source they were built from.
		struct ProgramVariant
		{
			std::shared_ptr<Shader> shader;
			bool failed = false;
			ExtraUniformsBlock extraUniforms[2];
		};
		ProgramVariant variants[8];
		int activeVariant = -1;
		// Holds this material's own extraUniforms[] while a variant swap
		// is in effect, so RestoreOwnProgram() can put it back.
		ExtraUniformsBlock ownExtraUniformsBackup[2];
		uint32 GetOrBuildVariant(int index);
		void ResetVariants();
		bool SwapToVariant(int index);

		struct Parameter
		{
			bool isVector = true;
			Vec4 value;
			Uniform* handle = nullptr;
		};
		std::map<std::string, Parameter> parameters;
		// (Re)creates each parameter's uniform - after ClearSamplers() or a
		// type change dropped the old one.
		void RegisterParameterUniform(const std::string &name, Parameter &p);

		// Shared by PopulateAutoExtraUniforms() and
		// GetOrBuildGBufferProgram() - fills outBlocks[VertexShader/
		// FragmentShader] from `program`'s own reflected std140 layout.
		void PopulateExtraUniformsFor(uint32 program, ExtraUniformsBlock outBlocks[2]) const;
	};

}

#endif /* CUSTOMSHADERMATERIAL_H */
