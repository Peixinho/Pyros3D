//============================================================================
// Name        : Shader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shader
//============================================================================

#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <stdlib.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	Shader::Shader()
	{
		vertexID = fragmentID = geometryID = shaderProgram = currentMaterials = 0;
	}

	Shader::~Shader() {}

	std::string Shader::LoadFileSource(const char *filename)
	{
		std::string shaderSource;

		std::ifstream t(filename);
		std::string str;

		if (t.fail()) {
			echo("ERROR: Shader File does not exist or you don't have permission to open it.");
			return std::string("\n\n/*\n * SHADER ERROR\n * COULDN'T OPEN/INCLUDE FILE ")+filename+std::string("\n *\n */\n\n");
		}

		std::string line;
		std::string pragma_include_ppc("#pragma include");
		std::string include_ppc("#include");
		while (std::getline(t, line))
		{
			std::istringstream iss(line);

			size_t foundInclude = line.find(pragma_include_ppc);
			uint32 includeSentenceSize = pragma_include_ppc.size();
			if (foundInclude == std::string::npos)
			{
				foundInclude = line.find(include_ppc);
				includeSentenceSize = include_ppc.size();
			}

			if (foundInclude != std::string::npos)
			{
				if (line.find_first_of("'")!=std::string::npos)
				{
					std::string fileToInclude = line.substr(line.find_first_of("'")+1, line.find_last_of("'")-(line.find_first_of("'")+1));
					shaderSource+=LoadFileSource(fileToInclude.c_str());
					continue;
				} 
				if (line.find_first_of("\"")!=std::string::npos)
				{
					std::string fileToInclude = line.substr(line.find_first_of("\"")+1, line.find_last_of("\"")-(line.find_first_of("\"")+1));
					shaderSource+=LoadFileSource(fileToInclude.c_str());
					continue;
				}
			}

			shaderSource+=line;
			shaderSource+="\n";
		}

		return shaderSource;
	}

	void Shader::LoadShaderFile(const char* filename)
	{
		shaderString = LoadFileSource(filename);
	}

	void Shader::LoadShaderText(const std::string &text)
	{
		shaderString = text;
	}

	bool Shader::CompileShader(const uint32 type, std::string definitions, std::string *output)
	{
		std::string LOG;
		std::string shaderType;
		uint32 shader;
		switch (type) {
		case ShaderType::VertexShader:
			vertexID = shader = glCreateShader(GL_VERTEX_SHADER);
			shaderType = "Vertex Shader";
			break;
		case ShaderType::FragmentShader:
			fragmentID = shader = glCreateShader(GL_FRAGMENT_SHADER);
			shaderType = "Fragment Shader";
			break;
		case ShaderType::GeometryShader:
			//geometryID = shader = glCreateShader(GL_GEOMETRY_SHADER);
			shaderType = "Geometry Shader";
			break;
		}

		#if defined(GLES2)
			std::string finalShaderString = (std::string("#version 100\n#define GLES2\n") + definitions + std::string(" ") + shaderString);
		#elif defined(GLES3)
			std::string finalShaderString = (std::string("#version 300 es\n#define GLES3\n") + definitions + std::string(" ") + shaderString);
		#elif defined(GL42)
			std::string finalShaderString = (std::string("#version 420\n") + definitions + std::string(" ") + shaderString);
		#elif defined(GL41)
			std::string finalShaderString = (std::string("#version 410\n") + definitions + std::string(" ") + shaderString);
		#else
			std::string finalShaderString = (std::string("#version 450\n") + definitions + std::string(" ") + shaderString);
		#endif

		uint32 len = finalShaderString.length();
		
		// batatas because is a good example :P
		const char *batatas = finalShaderString.c_str();

		GLCHECKER(glShaderSource(shader, 1, (const GLchar**)&batatas, (const GLint *)&len));
		GLCHECKER(glCompileShader(shader));

		GLint result, length = 0;

		GLCHECKER(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));
		if (length > 1)
		{
			char* log = (char*)malloc(length);
			GLCHECKER(glGetShaderInfoLog(shader, length, &result, log));
			GLCHECKER(glGetShaderiv(shader, GL_COMPILE_STATUS, &result));
			LOG = std::string(log);
			echo(std::string(shaderType.c_str() + std::string((result == GL_FALSE ? " COMPILATION ERROR:" + LOG : ": " + LOG))));

			if (output != NULL)
				*output = LOG;

			if (result == GL_FALSE) {
				echo(finalShaderString);
				return false;
			}
		}
		if (shaderProgram == 0)
			shaderProgram = (uint32)glCreateProgram();

		// Attach shader
		GLCHECKER(glAttachShader(shaderProgram, shader));

		return true;
	}
	bool Shader::LinkProgram(std::string *output) const
	{
		// Link Program
		GLCHECKER(glLinkProgram(shaderProgram));

		GLint result, length = 0;
		std::string LOG;

		// Get Linkage error
		GLCHECKER(glGetProgramiv(shaderProgram, GL_LINK_STATUS, &result));
		if (result == GL_FALSE)
		{
			GLCHECKER(glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &length));
			char* log = (char*)malloc(length);
			GLCHECKER(glGetProgramInfoLog(shaderProgram, length, &result, log));
			LOG = std::string(log);
			echo(std::string(std::string("SHADER PROGRAM LINK ERROR: ") + LOG));

			if (output != NULL)
				*output = LOG;

			free(log);
			return false;
		}

		return true;
	}

	const uint32 &Shader::ShaderProgram() const {
		return shaderProgram;
	}

	void Shader::DeleteShader()
	{
		if (glIsProgram(shaderProgram)) {
			if (glIsShader(vertexID)) {
				GLCHECKER(glDetachShader(shaderProgram, vertexID));
				GLCHECKER(glDeleteShader(vertexID));
				//            std::cout << "Shader Destroyed: " << shader << std::endl;
			}
			if (glIsShader(fragmentID)) {
				GLCHECKER(glDetachShader(shaderProgram, fragmentID));
				GLCHECKER(glDeleteShader(fragmentID));
				//            std::cout << "Shader Destroyed: " << shader << std::endl;
			}
			//if (glIsShader(geometryID)) {
			//	glDetachShader(shaderProgram, geometryID);
			//	glDeleteShader(geometryID);
   // //            std::cout << "Shader Destroyed: " << shader << std::endl;
   //         }
	//        else std::cout << "Shader Not Found: " << shader << std::endl;
		}
		//    else std::cout << "Shader Program Object Not Found: " << shaderProgram << std::endl;

		if (glIsProgram(shaderProgram)) {
			GLCHECKER(glDeleteProgram(shaderProgram));
			//      std::cout << "Shader Program Destroyed: " << *shaderProgram << std::endl;
			shaderProgram = 0;
		}
		//    else std::cout << "Shader Program Object Not Found: " << *shaderProgram << std::endl;
	}

	// Get positions
	const int32 Shader::GetUniformLocation(const uint32 program, const std::string &name) {
		return glGetUniformLocation(program, name.c_str());
	}
	const int32 Shader::GetAttributeLocation(const uint32 program, const std::string &name) {
		return glGetAttribLocation(program, name.c_str());
	}

	void Shader::SendUniform(const Uniform &uniform, const int32 Handle)
	{
		if (Handle > -1 && uniform.ElementCount > 0)
			switch (uniform.Type)
			{
			case Uniforms::DataType::Int:
			{
				GLCHECKER(glUniform1iv(Handle, uniform.ElementCount, (GLint*)&uniform.Value[0]));
				break;
			}
			case Uniforms::DataType::Float:
			{
				GLCHECKER(glUniform1fv(Handle, uniform.ElementCount, (f32*)&uniform.Value[0]));
				break;
			}
			case Uniforms::DataType::Vec2:
			{
				GLCHECKER(glUniform2fv(Handle, uniform.ElementCount, (f32*)&uniform.Value[0]));
				break;
			}
			case Uniforms::DataType::Vec3:
			{
				GLCHECKER(glUniform3fv(Handle, uniform.ElementCount, (f32*)&uniform.Value[0]));
				break;
			}
			case Uniforms::DataType::Vec4:
			{
				GLCHECKER(glUniform4fv(Handle, uniform.ElementCount, (f32*)&uniform.Value[0]));
				break;
			}
			case Uniforms::DataType::Matrix:
			{
				GLCHECKER(glUniformMatrix4fv(Handle, uniform.ElementCount, false, (f32*)&uniform.Value[0]));
				break;
			}
			}
	}

	void Shader::SendUniform(const Uniform &uniform, void* data, const int32 Handle, const uint32 elementCount)
	{
		if (Handle > -1 && elementCount > 0)
		{
			switch (uniform.Type)
			{
			case Uniforms::DataType::Int:
			{
				GLCHECKER(glUniform1iv(Handle, elementCount, (GLint*)((int32*)data)));
				break;
			}
			case Uniforms::DataType::Float:
			{
				GLCHECKER(glUniform1fv(Handle, elementCount, (f32*)data));
				break;
			}
			case Uniforms::DataType::Vec2:
			{
				GLCHECKER(glUniform2fv(Handle, elementCount, (f32*)data));
				break;
			}
			case Uniforms::DataType::Vec3:
			{
				GLCHECKER(glUniform3fv(Handle, elementCount, (f32*)data));
				break;
			}
			case Uniforms::DataType::Vec4:
			{
				GLCHECKER(glUniform4fv(Handle, elementCount, (f32*)data));
				break;
			}
			case Uniforms::DataType::Matrix:
			{
				GLCHECKER(glUniformMatrix4fv(Handle, elementCount, false, (f32*)data));
				break;
			}
			}
		}
	}
}
