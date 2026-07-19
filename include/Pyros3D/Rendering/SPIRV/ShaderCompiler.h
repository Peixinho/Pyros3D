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
	};

	class PYROS3D_API SpirvShaderCompiler
	{
	public:

		// Compiles GLSL source (already fully assembled - #version prefix,
		// #define definitions, and the shader body, the same way
		// IRenderDevice::BuildShaderSource() assembles it for the GL path)
		// into SPIR-V. Returns false and fills errorLog on failure;
		// outSpirv is left empty in that case.
		static bool Compile(const std::string &source, const uint32 stage, std::vector<uint32> &outSpirv, std::string &errorLog);

		// Enumerates the uniform-buffer and sampler resources a compiled
		// SPIR-V module declares, and the (set, binding) SPIR-V resolved
		// for each. Resources with no explicit `layout(binding = N)` in
		// the source will still be reported here (SPIR-V always assigns
		// something), but that assigned value may not be meaningful/stable
		// across recompiles unless the source pins it explicitly - see the
		// comment on SpirvResourceBinding.
		static std::vector<SpirvResourceBinding> Reflect(const std::vector<uint32> &spirv);

	};

};

#endif /* SPIRV_TOOLING */

#endif /* SHADERCOMPILER_H */
