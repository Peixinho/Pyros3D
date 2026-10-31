//============================================================================
// Name        : CustomShaderMaterials.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//============================================================================

#ifndef CUSTOMSHADERMATERIAL_H
#define CUSTOMSHADERMATERIAL_H

#include <utility>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Other/Export.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace p3d
{

	// A material of its own: a surface function the renderer compiles and lights as every other surface (see
	// SetSurfaceSource), the named parameters and the textures it reads.
	class PYROS3D_API CustomShaderMaterial : public IMaterial
	{

	public:

		// `ShaderFile` names the material (a .mat's shader, or an engine one): its surface is read from
		// <ShaderFile>.surface where that lies beside it. Empty: the caller sets the surface itself.
		explicit CustomShaderMaterial(const std::string& ShaderFile = std::string());

		virtual ~CustomShaderMaterial();

		// A texture the surface reads, by the name its `//@texture <name>` line gives
		void AddSampler(const std::string &uniformName, const std::shared_ptr<Texture> &tex);
		// Removes every texture added via AddSampler(); the parameters stay
		void ClearSamplers();

		std::vector<std::shared_ptr<Texture>> textures;

		// The name AddSampler() gave each entry of `textures`, parallel to it and in the same order
		const std::vector<std::string>& GetSamplerNames() const { return samplerNames; }

		// Where the material came from (project-relative, so the scene stays portable): the scene saves it, and
		// a material saved this way is read back from the file, so editing the .mat reaches every object using it.
		// Empty for a material with only its surface - the scene embeds that.
		const std::string &GetShaderFile() const { return ShaderFilePath; }
		void SetShaderFile(const std::string &path) { ShaderFilePath = path; LoadSurfaceBeside(path); }
		// The material as the renderer draws it: a surface function (`void Surface(inout SurfaceData s)`) that fills
		// the surface's colour, normal, roughness, metallic, emission, occlusion and alpha - the renderer lights it,
		// shadows it and so on as every other surface. A `//@param <name> <float|vec4>` line for each parameter it
		// reads (p_<name>), a `//@texture <name>` line for each texture (bound by the AddSampler() name). Made by the
		// Material Editor's graph (MaterialCodegen), saved in the scene as "surface".
		void SetSurfaceSource(const std::string &source) { surfaceSource = source; surfaceRevision++; }
		const std::string &GetSurfaceSource() const { return surfaceSource; }
		uint32 GetSurfaceRevision() const { return surfaceRevision; }
		// Reads <shaderFile>.surface where it lies beside the shader
		void LoadSurfaceBeside(const std::string &shaderFile);

		// Named material parameters - the Material Editor's Float/Color Parameter nodes. `name` is what the graph
		// shows; the surface reads it as "p_" + name, so a parameter can never collide with an engine name. Declare
		// fixes its type (float or vec4); Set on an undeclared name declares it from the value given. Values
		// survive ClearSamplers() and round-trip through scene files. Shared by every object using this material.
		void DeclareParameter(const std::string &name, bool isVector, const Vec4 &defaultValue);
		void SetParameter(const std::string &name, const Vec4 &value);
		void SetParameter(const std::string &name, f32 value);
		bool HasParameter(const std::string &name) const { return parameters.find(name) != parameters.end(); }
		Vec4 GetParameter(const std::string &name) const;
		bool IsVectorParameter(const std::string &name) const;
		std::vector<std::string> GetParameterNames() const;
		void RemoveParameter(const std::string &name);
		static std::string ParameterUniformName(const std::string &name) { return "p_" + name; }

	protected:

		std::string ShaderFilePath;
		std::string surfaceSource;
		uint32 surfaceRevision = 0;

		// Parallel to `textures` - see GetSamplerNames().
		std::vector<std::string> samplerNames;

		struct Parameter
		{
			bool isVector = true;
			Vec4 value;
		};
		std::map<std::string, Parameter> parameters;
	};

}

#endif /* CUSTOMSHADERMATERIAL_H */
