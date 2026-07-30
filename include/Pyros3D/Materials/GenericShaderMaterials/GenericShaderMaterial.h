//============================================================================
// Name        : ShaderLib.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ShaderLib
//============================================================================

#ifndef GENERICSHADERMATERIAL_H
#define GENERICSHADERMATERIAL_H

#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/Other/Export.h>
#include <iostream>
#include <map>

namespace p3d
{
	class PYROS3D_API GenericShaderMaterial : public IMaterial
	{
	public:

		GenericShaderMaterial() {}
		GenericShaderMaterial(const uint32 options);
		virtual ~GenericShaderMaterial();

		// PyrosShader.glsl is guaranteed to declare the fixed-binding UBOs
		// these materials' uniforms map into - see IMaterial::SupportsUniformBlocks().
		virtual bool SupportsUniformBlocks() const { return true; }
		// Set Colors
		void SetColor(const Vec4 &color);
		void SetSpecular(const Vec4 &specularColor);
		// Set Textures
		void SetColorMap(Texture* colormap);
		void SetSpecularMap(Texture* specular);
		void SetNormalMap(Texture* normalmap);
		void SetDisplacementMap(Texture* displacementMap);
		void SetDisplacementHeight(const f32 height);
		void SetEnvMap(Texture* envmap);
		void SetReflectivity(const f32 reflectivity);
		void SetRefractMap(Texture* refractmap);
		void SetSkyboxMap(Texture* skyboxmap);
		// Lights
		void SetShininess(const f32 shininess);
		// PBR (metallic/roughness workflow - ShaderUsage::PBR/PBRMap)
		void SetMetallic(const f32 metallic);
		void SetRoughness(const f32 roughness);
		// Packed ORM-style texture: G channel = roughness, B channel = metalness
		// (R unused/free - glTF convention minus AO, not yet supported here).
		void SetMetallicRoughnessMap(Texture* metallicRoughnessMap);

		// Text
		void SetTextFont(Font* font);

		void AddTexture(const std::string &uniformName, Texture* texture);

		// Render
		virtual void PreRender();
		virtual void AfterRender();

		// Bind
		void BindTextures();
		void UnbindTextures();

	private:

		// List of Tetxures
		std::vector<Texture*> Textures;

	protected:
		// Shaders List
		static std::map<uint32, Shader* > ShadersList;
		// Save Shader Location on Shaders List
		uint32 shaderID;

		// Lighting Properties
		Vec4 Ke;
		Vec4 Ka;
		Vec4 Kd;
		Vec4 Ks;
		f32 Shininess, UseLights, displacementHeight;

		// Environment Cube
		f32 Reflectivity;

		// PBR (metallic/roughness workflow)
		f32 Metallic, Roughness;

		// Texture IDs
		int32 colorMapID, specularMapID, normalMapID, displacementMapID, envMapID, skyboxMapID, refractMapID, fontMapID;
		int32 metallicRoughnessMapID;

		// Uniforms Handles
		Uniform *uColor, *uSpecular, *uReflectivity, *uShininess, *uUseLights, *uDisplacementHeight;
		Uniform *uMetallic, *uRoughness;
	};
}

#endif /* GENERICSHADERMATERIAL_H */
