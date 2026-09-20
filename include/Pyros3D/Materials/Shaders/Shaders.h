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
			GeometryShader,
			// Not a stage of the graphics pipeline, and never linked
			// alongside one: a compute program is a program whose ONLY
			// attached stage is this. Kept in the same enum anyway
			// because every device entry point that takes a stage
			// (CreateShaderStage, BuildShaderSource) is shared with the
			// graphics path and would otherwise need a parallel enum
			// that means the same thing.
			//
			// Only meaningful on a backend whose
			// IRenderDevice::SupportsCompute() returns true - GL41/GL42
			// (compute is GL 4.3), GLES 3.0 and WebGL2 have no such
			// stage at all, and CreateShaderStage returns 0 for it
			// there rather than pretending.
			ComputeShader
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

		// Already cached (LoadShaderFile/LoadShaderText both set
		// shaderString, never cleared after CompileShader() reads it) -
		// just wasn't exposed. Lets a CustomShaderMaterial built from a
		// raw Shader* (no recoverable file path) still be serialized by
		// embedding its actual source text directly.
		const std::string &GetShaderText() const { return shaderString; }

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