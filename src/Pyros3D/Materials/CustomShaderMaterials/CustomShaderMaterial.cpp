//============================================================================
// Name        : CustomShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//=================================================================================

#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

namespace p3d
{

	CustomShaderMaterial::CustomShaderMaterial(const std::string& ShaderFile) : IMaterial()
	{
		StringID number = (MakeStringID(ShaderFile)) + (MakeStringID(ShaderFile));

		{

			std::string define;
#if defined(GLES2)
			define += std::string("#define GLES2\n");
#endif
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
			shader = new Shader();

			shader->LoadShaderFile(ShaderFile.c_str());
			shader->CompileShader(ShaderType::VertexShader, (std::string("#define VERTEX\n") + define).c_str());
			shader->CompileShader(ShaderType::FragmentShader, (std::string("#define FRAGMENT\n") + define).c_str());

			shader->LinkProgram();
		}

		// Get Shader Program
		shaderProgram = shader->ShaderProgram();

		SetOpacity(1.0);

		isInternalShader = true;
	}

	CustomShaderMaterial::CustomShaderMaterial(Shader* shader)
	{
		shaderProgram = shader->ShaderProgram();

		isInternalShader = false;
	}

	void CustomShaderMaterial::SetShader(Shader* shader)
	{
		if (isInternalShader)
			delete this->shader; // delete old program

		isInternalShader = false;

		// Copy shader
		this->shader = shader;
		shaderProgram = shader->ShaderProgram();
	}

	CustomShaderMaterial::~CustomShaderMaterial()
	{
		if (isInternalShader)
			delete shader;
	}

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
