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
#include <memory>

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
		void SetColorMap(const std::shared_ptr<Texture> &colormap);
		void SetSpecularMap(const std::shared_ptr<Texture> &specular);
		void SetNormalMap(const std::shared_ptr<Texture> &normalmap);
		void SetDisplacementMap(const std::shared_ptr<Texture> &displacementMap);
		void SetDisplacementHeight(const f32 height);
		void SetEnvMap(const std::shared_ptr<Texture> &envmap);
		void SetReflectivity(const f32 reflectivity);
		void SetRefractMap(const std::shared_ptr<Texture> &refractmap);
		void SetSkyboxMap(const std::shared_ptr<Texture> &skyboxmap);
		// Lights
		void SetShininess(const f32 shininess);
		// PBR (metallic/roughness workflow - ShaderUsage::PBR/PBRMap)
		void SetMetallic(const f32 metallic);
		void SetRoughness(const f32 roughness);
		// Packed ORM-style texture: G channel = roughness, B channel = metalness
		// (R unused/free - glTF convention minus AO, not yet supported here).
		void SetMetallicRoughnessMap(const std::shared_ptr<Texture> &metallicRoughnessMap);
		// Real screen-space-reflection opt-in, per material - not to be
		// confused with SetReflectivity() above (an unrelated, older
		// env-map/skybox reflection blend amount). Defaults to false: SSR
		// in DeferredRenderer's lastPass.glsl used to be gated purely by
		// roughness (anything under its cutoff reflected, regardless of
		// what the material author actually wanted), with no way for a
		// material to opt out short of raising its roughness past the
		// cutoff - or, for anything that never asked for it at all, opt
		// in without also being deferred-G-buffer-compatible. This is the
		// material-level control DeferredRenderer::EnableSSR()'s own
		// comment always meant to imply but never actually built. Written
		// into the G-buffer's otherwise-always-0.0 metallicRoughness blue
		// channel (see PyrosShader.glsl's FragData_pbr) - existing
		// materials that never call this keep reading 0.0/"not
		// reflective" there, so this is purely additive, no behavior
		// change for anything that doesn't opt in.
		void SetSSREnabled(const bool enabled);

		// Text
		void SetTextFont(Font* font);

		void AddTexture(const std::string &uniformName, const std::shared_ptr<Texture> &texture);

		// Render
		virtual void PreRender();
		virtual void AfterRender();

		// Bind
		void BindTextures();
		void UnbindTextures();

		// Real getters - this material was previously write-only (every
		// Set* above had no matching Get*, blocking anything that needs
		// to read a material's current state back, e.g. scene
		// serialization or an editor Inspector panel showing the actual
		// current color instead of a disconnected local variable).
		// GetOptions() is the critical one: the ctor's ShaderUsage bitmask
		// selects which shader variant compiles, and without it a saved
		// material can't be reconstructed with the right shader at all.
		const uint32 &GetOptions() const { return shaderID; }
		const Vec4 &GetColor() const { return Kd; }
		const Vec4 &GetSpecular() const { return Ks; }
		const f32 &GetDisplacementHeight() const { return displacementHeight; }
		const f32 &GetReflectivity() const { return Reflectivity; }
		const f32 &GetShininess() const { return Shininess; }
		const f32 &GetMetallic() const { return Metallic; }
		const f32 &GetRoughness() const { return Roughness; }
		bool IsSSREnabled() const { return SSREnabled != 0.0f; }
		// Observing raw pointers - Material owns the shared_ptrs in Textures.
		Texture* GetColorMap() const { return colorMapID >= 0 ? Textures[colorMapID].get() : NULL; }
		Texture* GetSpecularMap() const { return specularMapID >= 0 ? Textures[specularMapID].get() : NULL; }
		Texture* GetNormalMap() const { return normalMapID >= 0 ? Textures[normalMapID].get() : NULL; }
		Texture* GetDisplacementMap() const { return displacementMapID >= 0 ? Textures[displacementMapID].get() : NULL; }
		Texture* GetEnvMap() const { return envMapID >= 0 ? Textures[envMapID].get() : NULL; }
		Texture* GetRefractMap() const { return refractMapID >= 0 ? Textures[refractMapID].get() : NULL; }
		Texture* GetSkyboxMap() const { return skyboxMapID >= 0 ? Textures[skyboxMapID].get() : NULL; }
		Texture* GetMetallicRoughnessMap() const { return metallicRoughnessMapID >= 0 ? Textures[metallicRoughnessMapID].get() : NULL; }

	private:

		// List of Textures
		std::vector<std::shared_ptr<Texture>> Textures;

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
		// See SetSSREnabled()'s comment - stored as 0.0/1.0, same
		// float-as-bool convention as everything else in this UBO.
		f32 SSREnabled;

		// Texture IDs
		int32 colorMapID, specularMapID, normalMapID, displacementMapID, envMapID, skyboxMapID, refractMapID, fontMapID;
		int32 metallicRoughnessMapID;

		// Uniforms Handles
		Uniform *uColor = NULL, *uSpecular = NULL, *uReflectivity = NULL, *uShininess = NULL, *uUseLights = NULL, *uDisplacementHeight = NULL;
		Uniform *uMetallic = NULL, *uRoughness = NULL;
		Uniform *uSSRReflective = NULL;
	};
}

#endif /* GENERICSHADERMATERIAL_H */
