//============================================================================
// Name        : ShaderCompiler.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GLSL -> SPIR-V cross-compilation + reflection
//============================================================================

#include <Pyros3D/Rendering/SPIRV/ShaderCompiler.h>

#ifdef SPIRV_TOOLING

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>

namespace p3d {

	bool SpirvShaderCompiler::Compile(const std::string &source, const uint32 stage, std::vector<uint32> &outSpirv, std::string &errorLog)
	{
		outSpirv.clear();

		shaderc_shader_kind kind = (stage == SpirvShaderStage::Fragment) ? shaderc_glsl_fragment_shader : shaderc_glsl_vertex_shader;

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
		options.SetOptimizationLevel(shaderc_optimization_level_zero);

		shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, kind, "shader", options);

		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			errorLog = result.GetErrorMessage();
			return false;
		}

		outSpirv.assign(result.cbegin(), result.cend());
		return true;
	}

	static uint32 TranslateResourceType(const uint32 spirvCrossResourceType)
	{
		switch (spirvCrossResourceType)
		{
		case spirv_cross::ResourceTypeUniformBuffer:
			return SpirvResourceType::UniformBuffer;
		case spirv_cross::ResourceTypeSampledImage:
			return SpirvResourceType::SampledImage;
		default:
			return SpirvResourceType::Unknown;
		}
	}

	static void ReflectResourceList(const spirv_cross::Compiler &compiler, const spirv_cross::SmallVector<spirv_cross::Resource> &resources, const uint32 resourceType, std::vector<SpirvResourceBinding> &out)
	{
		for (size_t i = 0; i < resources.size(); i++)
		{
			const spirv_cross::Resource &resource = resources[i];
			SpirvResourceBinding binding;
			binding.name = resource.name;
			binding.type = resourceType;
			binding.set = compiler.has_decoration(resource.id, spv::DecorationDescriptorSet) ? compiler.get_decoration(resource.id, spv::DecorationDescriptorSet) : 0;
			binding.binding = compiler.has_decoration(resource.id, spv::DecorationBinding) ? compiler.get_decoration(resource.id, spv::DecorationBinding) : 0;
			out.push_back(binding);
		}
	}

	std::vector<SpirvResourceBinding> SpirvShaderCompiler::Reflect(const std::vector<uint32> &spirv)
	{
		std::vector<SpirvResourceBinding> out;
		if (spirv.empty())
			return out;

		spirv_cross::Compiler compiler(spirv);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		ReflectResourceList(compiler, resources.uniform_buffers, SpirvResourceType::UniformBuffer, out);
		ReflectResourceList(compiler, resources.sampled_images, SpirvResourceType::SampledImage, out);

		return out;
	}

	std::vector<SpirvStageInput> SpirvShaderCompiler::ReflectStageInputs(const std::vector<uint32> &spirv)
	{
		std::vector<SpirvStageInput> out;
		if (spirv.empty())
			return out;

		spirv_cross::Compiler compiler(spirv);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		for (size_t i = 0; i < resources.stage_inputs.size(); i++)
		{
			const spirv_cross::Resource &resource = resources.stage_inputs[i];
			SpirvStageInput input;
			input.name = resource.name;
			input.location = compiler.has_decoration(resource.id, spv::DecorationLocation) ? compiler.get_decoration(resource.id, spv::DecorationLocation) : 0;
			out.push_back(input);
		}

		return out;
	}

};

#endif /* SPIRV_TOOLING */
