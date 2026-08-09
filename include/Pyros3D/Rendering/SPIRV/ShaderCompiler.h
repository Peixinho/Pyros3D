//============================================================================
// Name        : ShaderCompiler.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GLSL -> SPIR-V cross-compilation + reflection. Vulkan
//               roadmap Phase 2 - see VULKAN_ROADMAP.md. Nothing in the
//               engine consumes this yet (there's no VulkanRenderDevice),
//               so this is standalone, testable infrastructure ahead of
//               that work. Only built when CMake finds shaderc +
//               spirv-cross (see CMakeLists.txt's BUILD_SPIRV_TOOLING
//               option); the whole header is compiled out otherwise so a
//               build without the toolchain never sees a declaration it
//               can't link against.
//============================================================================

#ifndef SHADERCOMPILER_H
#define SHADERCOMPILER_H

#ifdef SPIRV_TOOLING

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <string>
#include <vector>

namespace p3d {

	// Mirrors Shaders.h's ShaderType, but SPIR-V compilation is a separate
	// path from the GL runtime one (shaderc wants its own shader-kind enum,
	// and there's no live GL shader object involved) so this isn't reused
	// directly - keeps this header includable without pulling in Shaders.h.
	namespace SpirvShaderStage
	{
		enum {
			Vertex = 0,
			Fragment
		};
	}

	namespace SpirvResourceType
	{
		enum {
			UniformBuffer = 0,
			SampledImage,
			Unknown
		};
	}

	// One reflected shader resource (a UBO block or a sampler) and the
	// descriptor (set, binding) SPIR-V actually assigned it. GLSL written
	// for the GL path relies on glUniformBlockBinding() to assign UBO
	// binding points at *runtime* (see GLRenderDevice::BindUniformBlockIfPresent),
	// which SPIR-V has no equivalent for - Vulkan descriptor bindings must
	// be static in the shader (an explicit `layout(binding = N)`), or
	// shaderc's auto-binding will assign whatever it likes. Reflecting
	// this after compilation is how a future VulkanRenderDevice would
	// build a VkDescriptorSetLayout without hand-authoring one per shader
	// variant - see VULKAN_ROADMAP.md Phase 2 for the "auto-derive
	// descriptor set layouts" goal this exists for.
	struct PYROS3D_API SpirvResourceBinding
	{
		std::string name;
		uint32 type;
		uint32 set;
		uint32 binding;
		// 1 for a plain (non-array) uniform/sampler; N for a
		// shader-declared fixed-size array (e.g. PyrosShader.glsl's
		// `uPointShadowMaps[4]`) - VkDescriptorSetLayoutBinding::
		// descriptorCount must match this exactly
		// (VUID-VkGraphicsPipelineCreateInfo-layout-07991: too small a
		// count here fails pipeline creation outright, and even when it
		// doesn't, whatever's *not* explicitly written by
		// vkUpdateDescriptorSets is left invalid, which fails at draw
		// time instead - VUID-vkCmdDrawIndexed-None-08114 - the moment
		// the shader indexes past what got bound).
		uint32 arraySize;
		// Only meaningful when type == SampledImage. A Vulkan descriptor
		// must be backed by an image whose *view type* matches what the
		// shader declares - a 2D image cannot stand in for a samplerCube,
		// and a non-comparison sampler cannot stand in for a
		// sampler2DShadow/samplerCubeShadow. DeferredRenderer already
		// discovered this the hard way and hand-rolled one dummy per kind
		// (see its dummyShadow2D/dummyShadowCube members); reflecting the
		// kind here is what lets VulkanRenderDevice do that generically,
		// for every sampler a pipeline declares but nothing binds, instead
		// of one call site at a time.
		bool isCube;
		bool isDepthCompare;
	};

	// One reflected vertex-shader input (a vertex attribute) and the
	// location SPIR-V assigned it. GL resolves an attribute's location by
	// name at *runtime* via glGetAttribLocation() (see
	// Shader::GetAttributeLocation(), called from IRenderer::BindMesh()) -
	// Vulkan has no equivalent query; the location is baked into the
	// SPIR-V via an explicit `layout(location = N) in ...` and must be
	// reflected once, ahead of time, the same way UBO/sampler bindings
	// already are above. Only meaningful for the vertex stage (the only
	// stage with externally-named "attribute" inputs in this engine's
	// model - fragment-stage inputs are VS outputs, matched positionally,
	// not by a mesh-provided buffer).
	struct PYROS3D_API SpirvStageInput
	{
		std::string name;
		uint32 location;
	};

	// Describes the one synthesized UBO block (if any) AutoFixForVulkan()
	// wrapped a shader stage's loose uniforms into - see that method's
	// comment. Mirrors IMaterial::ExtraUniformsBlock's shape exactly
	// (binding/blockName/size/offsets) so a caller can populate that
	// struct directly from this one without any translation step.
	struct PYROS3D_API SpirvAutoUboResult
	{
		bool hasBlock;
		uint32 binding;
		std::string blockName;
		uint32 size;
		std::map<std::string, uint32> offsets;
		SpirvAutoUboResult() : hasBlock(false), binding(0), size(0) {}
	};

	class PYROS3D_API SpirvShaderCompiler
	{
	public:

		// Compiles GLSL source (already fully assembled - #version prefix,
		// #define definitions, and the shader body, the same way
		// IRenderDevice::BuildShaderSource() assembles it for the GL path)
		// into SPIR-V. Returns false and fills errorLog on failure;
		// outSpirv is left empty in that case. Results are cached in
		// memory and under ~/.cache/pyros3d/spirv/ keyed by source hash.
		static bool Compile(const std::string &source, const uint32 stage, std::vector<uint32> &outSpirv, std::string &errorLog);

		// True when `source` still has loose in/out/uniform declarations
		// that need AutoFixForVulkan() (no `layout(...)` prefix). Engine
		// shaders that already ship with explicit layouts return false so
		// callers can skip the expensive shaderc preprocess AutoFix always
		// runs; CustomShaderMaterial's plain GL GLSL returns true.
		static bool NeedsAutoFixForVulkan(const std::string &source);

		// Generic Vulkan-portability auto-fix for arbitrary, plain
		// GL-style GLSL (CustomShaderMaterial's user-authored shaders,
		// which - unlike PyrosShader.glsl/the engine's own effect shaders -
		// were never written with Vulkan's stricter rules in mind - see
		// VULKAN_ROADMAP.md's CustomShaderMaterial gap). Preprocesses
		// `source` first (resolves this stage's #ifdef VERTEX/FRAGMENT
		// branch and expands the shader's own macros, e.g. a shader-local
		// `#define varying_in in`), then rewrites the flattened result so
		// it's valid Vulkan GLSL even with zero manual authoring:
		//   - every vertex attribute/varying `in`/`out` lacking an explicit
		//     `layout(location=N)` gets one, numbered by sorting the names
		//     found *within this stage* alphabetically - deterministic and
		//     stage-independent, so a vertex `out vTexcoord` and the
		//     matching fragment `in vTexcoord` always land on the same
		//     location without this method ever seeing both stages at
		//     once (Vulkan matches VS-out/FS-in by location number, not
		//     name, unlike GL).
		//   - every sampler-type uniform lacking an explicit
		//     `layout(set=1,binding=N)` gets one, numbered the same way
		//     (local to this stage/program - see LinkProgram()'s reflection-
		//     built samplerBindings map, nothing outside this program ever
		//     needs these numbers to agree with anything else).
		//   - every other (non-opaque) loose `uniform TYPE name, name2...;`
		//     declaration is collected, removed, and replaced with one
		//     synthesized `layout(std140,binding=uboBinding) uniform
		//     blockName { ... };`, offsets computed by std140 rules - same
		//     shape as IMaterial::ExtraUniformsBlock, reported back via
		//     outResult so a caller can populate that struct without
		//     hand-authoring it. Lines that already start with `layout(`
		//     are left untouched (already-correct/hand-authored shaders,
		//     including every shader this engine ships, are a no-op).
		// `uboBinding` must already be a globally-reserved value (see
		// IMaterial::ExtraUniformsBlock's comment on the binding
		// registry) - only actually consumed if the stage turns out to
		// have loose uniforms to wrap (outResult.hasBlock tells the
		// caller whether it was used). Returns false only on a genuine
		// preprocessing failure (errorLog filled) - a stage with nothing
		// to fix still returns true with outResult.hasBlock == false and
		// `source` essentially unchanged.
		static bool AutoFixForVulkan(std::string &source, const uint32 stage, const uint32 uboBinding, const std::string &blockName, SpirvAutoUboResult &outResult, std::string &errorLog);

		// Enumerates the uniform-buffer and sampler resources a compiled
		// SPIR-V module declares, and the (set, binding) SPIR-V resolved
		// for each. Resources with no explicit `layout(binding = N)` in
		// the source will still be reported here (SPIR-V always assigns
		// something), but that assigned value may not be meaningful/stable
		// across recompiles unless the source pins it explicitly - see the
		// comment on SpirvResourceBinding.
		static std::vector<SpirvResourceBinding> Reflect(const std::vector<uint32> &spirv);

		// Enumerates a compiled SPIR-V module's stage inputs (vertex
		// attributes, when called on a compiled vertex shader) and the
		// location each was assigned - see the comment on SpirvStageInput
		// for why this exists. Meaningless (returns whatever the module's
		// actual stage inputs are) if called on a non-vertex-shader module;
		// callers only ever pass a vertex shader's SPIR-V today.
		static std::vector<SpirvStageInput> ReflectStageInputs(const std::vector<uint32> &spirv);

	};

};

#endif /* SPIRV_TOOLING */

#endif /* SHADERCOMPILER_H */
