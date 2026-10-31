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
		GenericShaderMaterial(const ShaderOptions options);
		virtual ~GenericShaderMaterial();

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
		// How much it reflects, 0..1 (0 is off): the reflection's weight - Fresnel's and the
		// mirror floor's - scaled by it. SetSSREnabled(true) is 1. Lacquer, glass, a wet road want
		// it whole; a car's dusty paint or a dull metal much less, or they read as mirrors.
		void SetSSRStrength(const f32 strength);
		f32 GetSSRStrength() const { return SSREnabled > 0.f ? (SSREnabled - 0.5f) * 2.f : 0.f; }

		// Light that goes into a surface and comes out somewhere near, the
		// colour of what it went through: skin, wax, a leaf, marble. `color`
		// is what the light is tinted by under the surface (skin: red, most
		// of all) and `strength`, 0 to 1, how far it travels - 0 is off and
		// costs nothing, 1 is about five centimetres.
		// What it does to each light: the edge of the lit side is wider and
		// tinted, the far side of something thin (an ear, a hand against the
		// sun) glows, and a shadow cast from less than that distance away is
		// partly seen through - which is the same thing, light from the lit
		// skin beside it.
		// Forward and deferred both; in the G-buffer it travels in the blue
		// of the roughness/metallic target, below the half that means
		// "reflects the screen", and the colour shares the specular tint's
		// channels (see PyrosShader.glsl) - so a surface cannot be both
		// this and reflecting, and reflecting wins.
		void SetSubsurface(const Vec3 &color, const f32 strength);
		// A wet surface (PBR materials): `amount` 0 dry .. 1 soaked - darker and glossier;
		// `puddles` 0..1 how much of the ground facing up lies under still water (a mirror there, its
		// bumps gone), `size` a puddle's size in metres. Fixed in the world, not the texture.
		void SetWetness(const f32 amount, const f32 puddles = 0.f, const f32 size = 3.f);
		f32 GetWetness() const { return Wetness.x; }
		f32 GetPuddles() const { return Wetness.y; }
		f32 GetPuddleSize() const { return Wetness.z; }
		// Light the surface gives off itself - a lamp's glass, a lit window, a sign: `color` times
		// `strength`, in a light's units (1 is as bright as a white surface lit by a light of
		// intensity 1), added after all lighting. With `usesColorMap` the colour map's texel shapes
		// it (a sign glows in its own letters). Seen by DDGI as light, so it lights what is round it.
		// Strength 0: none.
		void SetEmission(const Vec3 &color, const f32 strength, const bool usesColorMap = false);
		// The maps repeated `scale` times across the mesh's texture coordinates and moved by `offset`: a stone
		// floor tiled every metre or two, not one picture stretched over the whole slab. (1, 1), (0, 0): as made.
		void SetUVTransform(const Vec2 &scale, const Vec2 &offset = Vec2(0.f, 0.f));
		Vec2 GetUVScale() const { return Vec2(UVTransform.x, UVTransform.y); }
		Vec2 GetUVOffset() const { return Vec2(UVTransform.z, UVTransform.w); }
		const Vec3 &GetEmissionColor() const { return EmissionColor; }
		f32 GetEmissionStrength() const { return EmissionStrength; }
		bool GetEmissionUsesColorMap() const { return EmissionUsesColorMap; }
		// colour times strength: what the surface gives off where its colour map is white
		Vec3 GetEmission() const { return EmissionColor * EmissionStrength; }
		// The picture the light comes from (glTF's emissive map): its texel times the emission colour and
		// strength. The material must have been made with ShaderUsage::EmissionMap.
		void SetEmissionMap(const std::shared_ptr<Texture> &emission);
		Texture* GetEmissionMap() const;

		// Alpha cutoff for ShaderUsage::AlphaTest - fragments whose alpha
		// falls below this are discarded outright. Ignored entirely by a
		// material that doesn't compile the flag, so setting it is
		// harmless either way. Defaults to 0.5.
		void SetAlphaCutoff(const f32 cutoff);
		f32 GetAlphaCutoff() const { return AlphaCutoff; }

		// Vertex sway for ShaderUsage::VertexWind materials: strength in
		// world units at the top of the mesh, rate in radians/second,
		// spatial frequency deciding how quickly the phase varies across
		// the scene (so a field ripples instead of moving as one).
		// skyLean: how far the surface's normal leans to the sky for
		// lighting (0 as modelled, 1 straight up) - grass and leaves then
		// take the light the ground takes instead of each card's own.
		// How much of the sun's light comes through (0: none - anything solid; about 0.5: a leaf).
		// For materials drawn with ShaderUsage::VertexWind - leaves and grass. Deferred frames.
		void SetLightThrough(const f32 amount) { LightThrough = amount; }
		f32 GetLightThrough() const { return LightThrough; }
		// An octahedral impostor's numbers (ShaderUsage::Impostor): the grid's side (views a row), the
		// tree's bounding radius and its centre's height above its foot, both at scale 1.
		void SetImpostor(const f32 frames, const f32 radius, const f32 centreHeight) { Impostor = Vec4(frames, radius, centreHeight, 1.f); }
		const Vec4 &GetImpostor() const { return Impostor; }
		void SetWind(const f32 strength, const f32 rate = 1.6f, const f32 spatialFrequency = 0.12f, const f32 skyLean = 0.f) { Wind = Vec4(strength, rate, spatialFrequency, skyLean); }
		const Vec4 &GetWind() const { return Wind; }

		// A new, independent material with the same options, properties
		// and render state. Textures are shared, not reloaded - it is the
		// material's settings that become separate. What cannot be
		// reproduced: a font map (the Font is not kept) and textures added
		// through AddTexture() under a custom uniform name.
		std::shared_ptr<GenericShaderMaterial> Clone() const;

		// Text
		void SetTextFont(Font* font);

		void AddTexture(const std::string &uniformName, const std::shared_ptr<Texture> &texture);

		// Real getters - this material was previously write-only (every
		// Set* above had no matching Get*, blocking anything that needs
		// to read a material's current state back, e.g. scene
		// serialization or an editor Inspector panel showing the actual
		// current color instead of a disconnected local variable).
		// GetOptions() is the critical one: the ctor's ShaderUsage bitmask
		// says what the material is, and without it a saved material
		// can't be reconstructed.
		const ShaderOptions &GetOptions() const { return shaderID; }
		const Vec4 &GetColor() const { return Kd; }
		const Vec4 &GetSpecular() const { return Ks; }
		const f32 &GetDisplacementHeight() const { return displacementHeight; }
		const f32 &GetReflectivity() const { return Reflectivity; }
		const f32 &GetShininess() const { return Shininess; }
		const f32 &GetMetallic() const { return Metallic; }
		const f32 &GetRoughness() const { return Roughness; }
		bool IsSSREnabled() const { return SSREnabled != 0.0f; }
		Vec3 GetSubsurfaceColor() const { return Vec3(Subsurface.x, Subsurface.y, Subsurface.z); }
		f32 GetSubsurfaceStrength() const { return Subsurface.w; }
		// Observing raw pointers - Material owns the shared_ptrs in Textures.
		Texture* GetColorMap() const { return colorMapID >= 0 ? Textures[colorMapID].get() : NULL; }
		Texture* GetFontMap() const { return fontMapID >= 0 ? Textures[fontMapID].get() : NULL; }
		// Shares ownership rather than observing, for handing one material's
		// colormap to another - IRenderer::PickShadowMaterial() lends a
		// cutout caster's map to the shared alpha-test shadow material, and
		// that material must not be left holding a dangling pointer if the
		// caster is destroyed while the shadow material still refers to it.
		std::shared_ptr<Texture> GetColorMapShared() const { return colorMapID >= 0 ? Textures[colorMapID] : std::shared_ptr<Texture>(); }
		Texture* GetSpecularMap() const { return specularMapID >= 0 ? Textures[specularMapID].get() : NULL; }
		Texture* GetNormalMap() const { return normalMapID >= 0 ? Textures[normalMapID].get() : NULL; }
		std::shared_ptr<Texture> GetNormalMapShared() const { return normalMapID >= 0 ? Textures[normalMapID] : std::shared_ptr<Texture>(); }
		Texture* GetDisplacementMap() const { return displacementMapID >= 0 ? Textures[displacementMapID].get() : NULL; }
		Texture* GetEnvMap() const { return envMapID >= 0 ? Textures[envMapID].get() : NULL; }
		Texture* GetRefractMap() const { return refractMapID >= 0 ? Textures[refractMapID].get() : NULL; }
		Texture* GetSkyboxMap() const { return skyboxMapID >= 0 ? Textures[skyboxMapID].get() : NULL; }
		Texture* GetMetallicRoughnessMap() const { return metallicRoughnessMapID >= 0 ? Textures[metallicRoughnessMapID].get() : NULL; }

	private:

		// List of Textures
		std::vector<std::shared_ptr<Texture>> Textures;

	protected:
		// What it is (ShaderUsage)
		ShaderOptions shaderID;

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
		// See SetSSREnabled()'s comment - stored as 0.0 (off) or 0.5..1.0 (on, and how strongly:
		// SetSSRStrength), same float-in-the-UBO convention as everything else in it.
		f32 SSREnabled;
		// See SetAlphaCutoff().
		f32 AlphaCutoff;
		// See SetSubsurface(): xyz the colour, w the strength.
		Vec4 Subsurface = Vec4(1.f, 0.35f, 0.2f, 0.f);
		Vec4 Wetness = Vec4(0.f, 0.f, 3.f, 0.f);
		// See SetEmission(). Emission is the uniform: xyz colour times strength, w uses the colour map.
		Vec3 EmissionColor = Vec3(1.f, 1.f, 1.f);
		f32 EmissionStrength = 0.f;
		bool EmissionUsesColorMap = false;
		Vec4 Emission = Vec4(0.f, 0.f, 0.f, 0.f);
		// See SetUVTransform().
		Vec4 UVTransform = Vec4(1.f, 1.f, 0.f, 0.f);
		// See SetWind().
		Vec4 Wind;
		// See SetLightThrough().
		f32 LightThrough = 0.f;
		// See SetImpostor().
		Vec4 Impostor = Vec4(0.f, 0.f, 0.f, 0.f);

		// Texture IDs
		int32 colorMapID, specularMapID, normalMapID, displacementMapID, envMapID, skyboxMapID, refractMapID, fontMapID;
		int32 metallicRoughnessMapID;
		int32 emissionMapID;

	};
}

#endif /* GENERICSHADERMATERIAL_H */
