//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

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

	void CustomShaderMaterial::PopulateAutoExtraUniforms()
	{
		uint32 stages[2] = { ShaderType::VertexShader, ShaderType::FragmentShader };
		for (int i = 0; i < 2; i++)
		{
			uint32 binding = 0, size = 0;
			std::string blockName;
			std::map<std::string, uint32> offsets;
			if (!Device().GetAutoUniformBlockLayout(shaderProgram, stages[i], binding, blockName, size, offsets))
				continue;
			ExtraUniformsBlock &block = extraUniforms[i];
			block.binding = binding;
			block.blockName = blockName;
			block.size = size;
			block.offsets = offsets;
			block.scratch.assign(size, 0);
			block.bufferHandle = 0;
		}
	}

	CustomShaderMaterial::~CustomShaderMaterial() = default;

	void CustomShaderMaterial::PreRender()
	{
		for (std::vector<Texture*>::iterator i = textures.begin(); i != textures.end(); i++)
			(*i)->Bind();
	}

	void CustomShaderMaterial::AfterRender()
	{
		for (std::vector<Texture*>::reverse_iterator i = textures.rbegin(); i != textures.rend(); i++)
			(*i)->Unbind();
	}
}
