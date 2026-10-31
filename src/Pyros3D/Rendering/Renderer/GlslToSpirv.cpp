//============================================================================
// Name        : GlslToSpirv.cpp
// Author      : Duarte Peixinho
// Description : GLSL to SPIR-V through glslang's C interface
//============================================================================

#include <Pyros3D/Rendering/Renderer/GlslToSpirv.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <mutex>

namespace p3d {

	bool GlslToSpirv(const std::string &source, const gfx::Stage stage, std::vector<uint32_t> &spirv, std::string &error)
	{
		static std::once_flag once;
		std::call_once(once, [] { glslang_initialize_process(); });
		glslang_input_t in = {};
		in.language = GLSLANG_SOURCE_GLSL;
		in.stage = stage == gfx::Stage::Vertex ? GLSLANG_STAGE_VERTEX : stage == gfx::Stage::Fragment ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_COMPUTE;
		in.client = GLSLANG_CLIENT_VULKAN; in.client_version = GLSLANG_TARGET_VULKAN_1_1;
		in.target_language = GLSLANG_TARGET_SPV; in.target_language_version = GLSLANG_TARGET_SPV_1_3;
		in.code = source.c_str();
		in.default_version = 450; in.default_profile = GLSLANG_NO_PROFILE;
		in.messages = (glslang_messages_t)(GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT);
		in.resource = glslang_default_resource();
		glslang_shader_t* shader = glslang_shader_create(&in);
		if (!glslang_shader_preprocess(shader, &in) || !glslang_shader_parse(shader, &in))
		{
			error = std::string(glslang_shader_get_info_log(shader)) + glslang_shader_get_info_debug_log(shader);
			glslang_shader_delete(shader);
			return false;
		}
		glslang_program_t* program = glslang_program_create();
		glslang_program_add_shader(program, shader);
		bool ok = glslang_program_link(program, in.messages) != 0;
		if (ok)
		{
			glslang_spv_options_t options = {};
			options.validate = false; options.disable_optimizer = false; options.optimize_size = false;
			glslang_program_SPIRV_generate_with_options(program, in.stage, &options);
			const size_t words = glslang_program_SPIRV_get_size(program);
			spirv.resize(words);
			glslang_program_SPIRV_get(program, spirv.data());
			if (const char* m = glslang_program_SPIRV_get_messages(program)) error = m;
			ok = words > 0;
		}
		else error = std::string(glslang_program_get_info_log(program)) + glslang_program_get_info_debug_log(program);
		glslang_program_delete(program);
		glslang_shader_delete(shader);
		return ok;
	}

}
