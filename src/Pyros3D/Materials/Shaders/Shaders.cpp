//============================================================================
// Name        : Shader.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shader
//============================================================================

#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <stdlib.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

	// Every Shader shares whichever backend is currently active (see
	// GetActiveRenderDevice() in IRenderDevice.h) rather than each Shader
	// needing an injected IRenderDevice* - avoids plumbing one through
	// every call site that constructs a Shader or calls its static
	// GetUniformLocation/GetAttributeLocation/SendUniform methods, while
	// still respecting the actual backend in use (GL vs Vulkan), unlike
	// the file-local `static GLRenderDevice instance` this used to
	// hardcode - same pattern as GeometryBuffer.cpp.
	static IRenderDevice& Device()
	{
		return GetActiveRenderDevice();
	}

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
		std::string shaderType;
		uint32 shader;
		switch (type) {
		case ShaderType::VertexShader:
			vertexID = shader = Device().CreateShaderStage(ShaderType::VertexShader);
			shaderType = "Vertex Shader";
			break;
		case ShaderType::FragmentShader:
			fragmentID = shader = Device().CreateShaderStage(ShaderType::FragmentShader);
			shaderType = "Fragment Shader";
			break;
		case ShaderType::GeometryShader:
			//geometryID = shader = Device().CreateShaderStage(ShaderType::GeometryShader);
			shaderType = "Geometry Shader";
			break;
		}

		std::string finalShaderString = Device().BuildShaderSource(definitions, shaderString);

		std::string LOG;
		bool compiled = Device().CompileShaderStage(shader, finalShaderString, LOG);
		if (!compiled)
		{
			echo(std::string(shaderType.c_str() + std::string(" COMPILATION FAILED") + (!LOG.empty() ? (":" + LOG) : "")));

			if (output != NULL)
				*output = LOG;

			echo(finalShaderString);
			return false;
		}
		if (!LOG.empty())
		{
			echo(std::string(shaderType.c_str() + std::string(": " + LOG)));
			if (output != NULL)
				*output = LOG;
		}
		if (shaderProgram == 0)
			shaderProgram = Device().CreateProgram();

		// Attach shader
		Device().AttachShaderStage(shaderProgram, shader);

		return true;
	}
	bool Shader::LinkProgram(std::string *output) const
	{
		std::string LOG;
		bool linked = Device().LinkProgram(shaderProgram, LOG);
		if (!linked)
		{
			echo(std::string(std::string("SHADER PROGRAM LINK ERROR: ") + LOG));

			if (output != NULL)
				*output = LOG;

			return false;
		}

		return true;
	}

	const uint32 &Shader::ShaderProgram() const {
		return shaderProgram;
	}

	void Shader::DeleteShader()
	{
		if (Device().IsProgram(shaderProgram)) {
			if (Device().IsShaderStage(vertexID)) {
				Device().DetachShaderStage(shaderProgram, vertexID);
				Device().DeleteShaderStage(vertexID);
				//            std::cout << "Shader Destroyed: " << shader << std::endl;
			}
			if (Device().IsShaderStage(fragmentID)) {
				Device().DetachShaderStage(shaderProgram, fragmentID);
				Device().DeleteShaderStage(fragmentID);
				//            std::cout << "Shader Destroyed: " << shader << std::endl;
			}
			//if (Device().IsShaderStage(geometryID)) {
			//	Device().DetachShaderStage(shaderProgram, geometryID);
			//	Device().DeleteShaderStage(geometryID);
   // //            std::cout << "Shader Destroyed: " << shader << std::endl;
   //         }
	//        else std::cout << "Shader Not Found: " << shader << std::endl;
		}
		//    else std::cout << "Shader Program Object Not Found: " << shaderProgram << std::endl;

		if (Device().IsProgram(shaderProgram)) {
			Device().DeleteProgram(shaderProgram);
			//      std::cout << "Shader Program Destroyed: " << *shaderProgram << std::endl;
			shaderProgram = 0;
		}
		//    else std::cout << "Shader Program Object Not Found: " << *shaderProgram << std::endl;
	}

	// Get positions
	const int32 Shader::GetUniformLocation(const uint32 program, const std::string &name) {
		return Device().GetUniformLocation(program, name);
	}
	const int32 Shader::GetAttributeLocation(const uint32 program, const std::string &name) {
		return Device().GetAttributeLocation(program, name);
	}

	void Shader::SendUniform(const Uniform &uniform, const int32 Handle)
	{
		if (Handle > -1 && uniform.ElementCount > 0)
			switch (uniform.Type)
			{
			case Uniforms::DataType::Int:
				Device().SendUniformInt(Handle, (const int32*)&uniform.Value[0], uniform.ElementCount);
				break;
			case Uniforms::DataType::Float:
				Device().SendUniformFloat(Handle, (const f32*)&uniform.Value[0], uniform.ElementCount);
				break;
			case Uniforms::DataType::Vec2:
				Device().SendUniformVec2(Handle, (const f32*)&uniform.Value[0], uniform.ElementCount);
				break;
			case Uniforms::DataType::Vec3:
				Device().SendUniformVec3(Handle, (const f32*)&uniform.Value[0], uniform.ElementCount);
				break;
			case Uniforms::DataType::Vec4:
				Device().SendUniformVec4(Handle, (const f32*)&uniform.Value[0], uniform.ElementCount);
				break;
			case Uniforms::DataType::Matrix:
				Device().SendUniformMatrix(Handle, (const f32*)&uniform.Value[0], uniform.ElementCount);
				break;
			}
	}

	void Shader::SendUniform(const Uniform &uniform, void* data, const int32 Handle, const uint32 elementCount)
	{
		if (Handle > -1 && elementCount > 0)
		{
			switch (uniform.Type)
			{
			case Uniforms::DataType::Int:
				Device().SendUniformInt(Handle, (const int32*)data, elementCount);
				break;
			case Uniforms::DataType::Float:
				Device().SendUniformFloat(Handle, (const f32*)data, elementCount);
				break;
			case Uniforms::DataType::Vec2:
				Device().SendUniformVec2(Handle, (const f32*)data, elementCount);
				break;
			case Uniforms::DataType::Vec3:
				Device().SendUniformVec3(Handle, (const f32*)data, elementCount);
				break;
			case Uniforms::DataType::Vec4:
				Device().SendUniformVec4(Handle, (const f32*)data, elementCount);
				break;
			case Uniforms::DataType::Matrix:
				Device().SendUniformMatrix(Handle, (const f32*)data, elementCount);
				break;
			}
		}
	}
}
