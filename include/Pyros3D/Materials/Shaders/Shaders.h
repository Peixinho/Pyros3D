//============================================================================
// Name        : Shaders.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Shaders
//============================================================================

#ifndef SHADERS_H
#define	SHADERS_H

#include <Pyros3D/Core/Math/Matrix.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Other/Export.h>
#include <string.h>
#include <fstream>
#include <iostream>

namespace p3d {

	namespace ShaderType {
		enum {
			VertexShader = 0,
			FragmentShader,
			GeometryShader
		};
	}

	class PYROS3D_API Shader {
	public:

		Shader();
		virtual ~Shader();

		void LoadShaderFile(const char* filename);
		void LoadShaderText(const std::string &text);

		bool CompileShader(const uint32 type, std::string definitions = std::string(""), std::string *output = NULL);
		void DeleteShader();
		bool LinkProgram(std::string *output = NULL) const;

		const uint32 &ShaderProgram() const;

		static const int32 GetUniformLocation(const uint32 program, const std::string &name);
		static const int32 GetAttributeLocation(const uint32 program, const std::string &name);

		static void SendUniform(const Uniform &uniform, const int32 Handle);
		static void SendUniform(const Uniform &uniform, void* data, const int32 Handle, const uint32 elementCount = 1);

		// Shader Usage Counter
		uint32 currentMaterials;

	private:

		std::string LoadFileSource(const char* file);

		// shader type
		uint32 type;

		//shader text
		std::string shaderString;

		// ids
		uint32 vertexID;
		uint32 fragmentID;
		uint32 geometryID;

		// GL ID
		uint32 shaderProgram;

		// Vertex and Fragment Shaders
		Shader* shader;

	};

}

#endif	/* SHADERS_H */