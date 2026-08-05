//============================================================================
// Name        : CustomShaderMaterials.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Custom Shader Materials
//============================================================================

#ifndef CUSTOMSHADERMATERIAL_H
#define CUSTOMSHADERMATERIAL_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Other/Export.h>
#include <iostream>
#include <map>
#include <memory>

namespace p3d
{

	class PYROS3D_API CustomShaderMaterial : public IMaterial
	{

	public:

		CustomShaderMaterial(const std::string &ShaderFile);
		CustomShaderMaterial(Shader* shader);
		void SetShader(Shader* shader);

		virtual ~CustomShaderMaterial();

		virtual void PreRender();

		virtual void AfterRender();

		std::vector<std::shared_ptr<Texture>> textures;

		// Empty when constructed from a raw Shader* - that path has no
		// recoverable source, callers (e.g. scene serialization) must
		// treat an empty string as "can't be saved/reconstructed".
		const std::string &GetShaderFile() const { return ShaderFilePath; }

		// The underlying Shader* itself - needed so a path-less material
		// (built from a raw Shader*) can still fall back to embedding
		// Shader::GetShaderText()'s real cached source.
		Shader* GetShaderObject() const { return shader; }

	protected:

		std::string ShaderFilePath;

		// Generic Vulkan auto-fix hookup - see IRenderDevice::
		// GetAutoUniformBlockLayout()'s comment. Called from every place
		// this class finishes wiring up `shader` (both constructors and
		// SetShader()); a no-op on GL and on any shader whose loose
		// uniforms are already hand-wrapped in an explicit UBO (every
		// shader this engine ships today) - only actually populates
		// extraUniforms[] for a shader that genuinely had nothing but
		// plain, unlabeled loose uniforms. Runs before a subclass's own
		// constructor body (base-class constructors always run first), so
		// a subclass that still hand-assigns extraUniforms[] itself
		// (CustomMaterialExample/WaterMaterial/ParticleMaterial) simply
		// overwrites this afterward - manual authoring always wins.
		void PopulateAutoExtraUniforms();

		// Shader
		Shader* shader;
		// Owned only when `shader` was built internally (from a file, or
		// handed off by a previous SetShader call) - null when `shader`
		// points at a caller-owned Shader instead.
		std::unique_ptr<Shader> InternalShader;
	};

}

#endif /* CUSTOMSHADERMATERIAL_H */
