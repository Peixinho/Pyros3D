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
#include <vector>
#include <sstream>

namespace p3d {

	namespace {

		// Pull the offending source line numbers out of a driver info log.
		//
		// Every backend writes the position as "<string index><sep><line>",
		// only the separators differ: ANGLE and WebGL2 say "ERROR: 0:552:",
		// NVIDIA says "0(552) :", Mesa says "0:552(13):". So: find a digit
		// run, and if ':' or '(' follows with more digits, the second number
		// is the line. Anything unrecognised yields nothing, and the caller
		// falls back to printing the whole shader rather than printing less
		// than it used to.
		static std::vector<uint32> ShaderErrorLines(const std::string &log)
		{
			std::vector<uint32> lines;
			for (size_t i = 0; i < log.size();)
			{
				if (!isdigit((unsigned char)log[i])) { ++i; continue; }
				size_t a = i;
				while (a < log.size() && isdigit((unsigned char)log[a])) ++a;
				if (a < log.size() && (log[a] == ':' || log[a] == '('))
				{
					size_t b = a + 1, c = b;
					while (c < log.size() && isdigit((unsigned char)log[c])) ++c;
					if (c > b)
					{
						const uint32 line = (uint32)strtoul(log.substr(b, c - b).c_str(), NULL, 10);
						bool seen = false;
						for (size_t k = 0; k < lines.size(); ++k)
							if (lines[k] == line) { seen = true; break; }
						if (line > 0 && !seen && lines.size() < 10)
							lines.push_back(line);
						i = c;
						continue;
					}
				}
				i = a;
			}
			return lines;
		}

		// The named lines with one line of context either side, numbered, with
		// the offending one marked. A shader is ~1600 lines and the whole dump
		// buries the one line that matters.
		static std::string ShaderSourceExcerpt(const std::string &source, const std::vector<uint32> &badLines)
		{
			std::vector<std::string> src;
			{
				std::istringstream in(source);
				std::string l;
				while (std::getline(in, l)) src.push_back(l);
			}

			std::ostringstream out;
			uint32 lastPrinted = 0;
			for (size_t k = 0; k < badLines.size(); ++k)
			{
				const uint32 hit = badLines[k];
				if (hit > src.size()) continue;
				const uint32 from = hit > 1 ? hit - 1 : 1;
				const uint32 to = (hit + 1 <= src.size()) ? hit + 1 : (uint32)src.size();
				if (lastPrinted && from > lastPrinted + 1) out << "        ...\n";
				for (uint32 n = (lastPrinted && from <= lastPrinted) ? lastPrinted + 1 : from; n <= to; ++n)
				{
					out << (n == hit ? "  >> " : "     ") << n << " | " << src[n - 1] << "\n";
					lastPrinted = n;
				}
			}
			return out.str();
		}

	}

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
			// Name the file, and say what it was resolved against. Shader
			// paths are RELATIVE ("shaders/PyrosShader.glsl"), so the usual
			// cause of this is a working directory without a shaders/ folder
			// next to it - running the binary from the source tree instead of
			// its build directory, say. Without the path in the message the
			// only symptom is that every draw quietly uses a program that
			// failed to build, which in a debug build surfaces as an abort
			// inside glUseProgram with nothing pointing back to here.
			echo(std::string("ERROR: Could not open shader file '") + filename
				+ "' (relative paths resolve against the working directory).");
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

			// The failing line(s) only. The full source is still printed when
			// the log cannot be parsed, so nothing is ever lost - see
			// ShaderErrorLines.
			const std::vector<uint32> badLines = ShaderErrorLines(LOG);
			const std::string excerpt = badLines.empty() ? std::string() : ShaderSourceExcerpt(finalShaderString, badLines);
			if (!excerpt.empty())
				echo(excerpt);
			else
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
