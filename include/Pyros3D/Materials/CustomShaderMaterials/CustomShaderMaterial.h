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

		std::vector<Texture*> textures;

	protected:
		// Shader
		Shader* shader;
		// Owned only when `shader` was built internally (from a file, or
		// handed off by a previous SetShader call) - null when `shader`
		// points at a caller-owned Shader instead.
		std::unique_ptr<Shader> InternalShader;
	};

}

#endif /* CUSTOMSHADERMATERIAL_H */
