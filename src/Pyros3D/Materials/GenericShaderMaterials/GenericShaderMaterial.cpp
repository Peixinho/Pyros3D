//============================================================================
// Name        : GenericShaderMaterials.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Generic Shader Materials
//============================================================================

#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Rendering/ColourSpace.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <algorithm>

namespace p3d
{

	// The engine's own material: what it is (its ShaderUsage options) and its values - the renderer draws it
	GenericShaderMaterial::GenericShaderMaterial(const ShaderOptions options) : IMaterial()
	{
		colorMapID = specularMapID = normalMapID = displacementMapID = envMapID = skyboxMapID = refractMapID = fontMapID = emissionMapID = -1;
		metallicRoughnessMapID = -1;

		displacementHeight = 0.05f;
		// Half is the usual default for a cutout map; only consulted when ShaderUsage::AlphaTest is on.
		AlphaCutoff = 0.5f;
		Wind = Vec4(0.f, 0.f, 0.f, 0.f);
		SSREnabled = 0.0f;
		UseLights = 1.0f;
		Shininess = 50.0f;
		// fully dielectric, mid-rough
		Metallic = 0.0f;
		Roughness = 0.5f;
		Reflectivity = (options & (ShaderUsage::EnvMap | ShaderUsage::Refraction)) ? 1.0f : 0.0f;

		shaderID = options;
		cullFace = CullFace::BackFace;
		SetOpacity(1.0);

		if (options & (ShaderUsage::DirectionalShadow | ShaderUsage::PointShadow | ShaderUsage::SpotShadow))
			isCastingShadows = true;
		if (options & ShaderUsage::TextRendering)
			SetTransparencyFlag(true);
	}

	void GenericShaderMaterial::SetShininess(const f32 shininess)
	{
		this->Shininess = shininess;
	}
	GenericShaderMaterial::~GenericShaderMaterial() {}

	void GenericShaderMaterial::SetDisplacementHeight(const f32 height)
	{
		displacementHeight = height;
	}

	std::shared_ptr<GenericShaderMaterial> GenericShaderMaterial::Clone() const
	{
		std::shared_ptr<GenericShaderMaterial> m = std::make_shared<GenericShaderMaterial>(shaderID);
		m->SetColor(Kd);
		m->SetSpecular(Ks);
		m->SetShininess(Shininess);
		m->SetDisplacementHeight(displacementHeight);
		m->SetReflectivity(Reflectivity);
		m->SetMetallic(Metallic);
		m->SetRoughness(Roughness);
		m->SetSSRStrength(GetSSRStrength());
		m->SetAlphaCutoff(AlphaCutoff);
		if (Subsurface.w > 0.f) m->SetSubsurface(Vec3(Subsurface.x, Subsurface.y, Subsurface.z), Subsurface.w);
		if (Wetness.x > 0.f || Wetness.y > 0.f) m->SetWetness(Wetness.x, Wetness.y, Wetness.z);
		if (EmissionStrength > 0.f) m->SetEmission(EmissionColor, EmissionStrength, EmissionUsesColorMap);
		if (UVTransform != Vec4(1.f, 1.f, 0.f, 0.f)) m->SetUVTransform(GetUVScale(), GetUVOffset());
		m->Wind = Wind;
		m->LightThrough = LightThrough;
		m->Impostor = Impostor;
		m->Ke = Ke;
		m->Ka = Ka;

		auto tex = [this](int32 id) { return id >= 0 && id < (int32)Textures.size() ? Textures[id] : std::shared_ptr<Texture>(); };
		if (std::shared_ptr<Texture> t = tex(colorMapID)) m->SetColorMap(t);
		if (std::shared_ptr<Texture> t = tex(specularMapID)) m->SetSpecularMap(t);
		if (std::shared_ptr<Texture> t = tex(emissionMapID)) m->SetEmissionMap(t);
		if (std::shared_ptr<Texture> t = tex(normalMapID)) m->SetNormalMap(t);
		if (std::shared_ptr<Texture> t = tex(displacementMapID)) m->SetDisplacementMap(t);
		if (std::shared_ptr<Texture> t = tex(envMapID)) m->SetEnvMap(t);
		if (std::shared_ptr<Texture> t = tex(refractMapID)) m->SetRefractMap(t);
		if (std::shared_ptr<Texture> t = tex(skyboxMapID)) m->SetSkyboxMap(t);
		if (std::shared_ptr<Texture> t = tex(metallicRoughnessMapID)) m->SetMetallicRoughnessMap(t);

		// IMaterial render state.
		m->SetOpacity(opacity);
		m->SetTransparencyFlag(isTransparent);
		m->SetCullFace(cullFace);
		m->isWireFrame = isWireFrame;
		m->isCastingShadows = isCastingShadows;
		m->depthTest = depthTest;
		m->depthWrite = depthWrite;
		m->forceDepthWrite = forceDepthWrite;
		m->depthTestMode = depthTestMode;
		m->depthBias = depthBias;
		m->depthFactor = depthFactor;
		m->depthUnits = depthUnits;
		m->blending = blending;
		m->sfactor = sfactor;
		m->dfactor = dfactor;
		m->mode = mode;
		return m;
	}

	void GenericShaderMaterial::AddTexture(const std::string &uniformName, const std::shared_ptr<Texture> &texture)
	{
		uint32 id = Textures.size();

		// Save on Textures Lis
		Textures.push_back(texture);
	}

	void GenericShaderMaterial::SetColor(const Vec4& color)
	{
		Kd = color;
	}
	void GenericShaderMaterial::SetSpecular(const Vec4& specularColor)
	{
		Ks = specularColor;
	}

	void GenericShaderMaterial::SetColorMap(const std::shared_ptr<Texture> &colormap)
	{
		if (colorMapID == -1)
			colorMapID = Textures.size();
		else {
			Textures[colorMapID] = colormap;
			return;
		}
		// Save on List
		Textures.push_back(colormap);
	}
	void GenericShaderMaterial::SetSpecularMap(const std::shared_ptr<Texture> &specular)
	{
		if (specularMapID == -1)
			specularMapID = Textures.size();
		else {
			Textures[specularMapID] = specular;
			return;
		}
		// Save on List
		Textures.push_back(specular);
	}
	void GenericShaderMaterial::SetEmissionMap(const std::shared_ptr<Texture> &emission)
	{
		if (emissionMapID == -1)
			emissionMapID = Textures.size();
		else {
			Textures[emissionMapID] = emission;
			return;
		}
		Textures.push_back(emission);
	}
	Texture* GenericShaderMaterial::GetEmissionMap() const
	{
		return (emissionMapID >= 0 && (size_t)emissionMapID < Textures.size()) ? Textures[emissionMapID].get() : NULL;
	}
	void GenericShaderMaterial::SetMetallicRoughnessMap(const std::shared_ptr<Texture> &metallicRoughnessMap)
	{
		if (metallicRoughnessMapID == -1)
			metallicRoughnessMapID = Textures.size();
		else {
			Textures[metallicRoughnessMapID] = metallicRoughnessMap;
			return;
		}
		// Save on List
		Textures.push_back(metallicRoughnessMap);
	}
	void GenericShaderMaterial::SetNormalMap(const std::shared_ptr<Texture> &normalmap)
	{
		if (normalMapID == -1)
			normalMapID = Textures.size();
		else {
			Textures[normalMapID] = normalmap;
			return;
		}
		// Save on List
		Textures.push_back(normalmap);
	}
	void GenericShaderMaterial::SetDisplacementMap(const std::shared_ptr<Texture> &displacementmap)
	{
		if (displacementMapID == -1)
			displacementMapID = Textures.size();
		else {
			Textures[displacementMapID] = displacementmap;
			return;
		}
		// Save on List
		Textures.push_back(displacementmap);
	}
	void GenericShaderMaterial::SetEnvMap(const std::shared_ptr<Texture> &envmap)
	{
		if (envMapID == -1)
			envMapID = Textures.size();
		else {
			Textures[envMapID] = envmap;
			return;
		}
		// Save on List
		Textures.push_back(envmap);
	}
	void GenericShaderMaterial::SetReflectivity(const f32 reflectivity)
	{
		Reflectivity = reflectivity;
	}
	void GenericShaderMaterial::SetMetallic(const f32 metallic)
	{
		Metallic = metallic;
	}
	void GenericShaderMaterial::SetRoughness(const f32 roughness)
	{
		Roughness = roughness;
	}
	void GenericShaderMaterial::SetSSREnabled(const bool enabled) { SetSSRStrength(enabled ? 1.f : 0.f); }
	void GenericShaderMaterial::SetSSRStrength(const f32 strength)
	{
		// (0 off; else 0.5 + half the strength: the pass reads at or above a half as "reflects",
		// and the rest as how much - so 1, all there was before, is full strength)
		const f32 s = strength < 0.f ? 0.f : (strength > 1.f ? 1.f : strength);
		SSREnabled = s > 0.f ? 0.5f + 0.5f * s : 0.0f;
	}

	void GenericShaderMaterial::SetWetness(const f32 amount, const f32 puddles, const f32 size)
	{
		Wetness = Vec4(amount < 0.f ? 0.f : (amount > 1.f ? 1.f : amount), puddles < 0.f ? 0.f : (puddles > 1.f ? 1.f : puddles), size > 0.05f ? size : 0.05f, 0.f);
	}

	void GenericShaderMaterial::SetEmission(const Vec3 &color, const f32 strength, const bool usesColorMap)
	{
		EmissionColor = Vec3(std::max(color.x, 0.f), std::max(color.y, 0.f), std::max(color.z, 0.f));
		EmissionStrength = std::max(strength, 0.f);
		EmissionUsesColorMap = usesColorMap;
		const Vec3 e = SrgbToLinear(EmissionColor) * EmissionStrength;     // (a picked colour: ColourSpace.h)
		Emission = Vec4(e.x, e.y, e.z, usesColorMap ? 1.f : 0.f);
	}

	void GenericShaderMaterial::SetUVTransform(const Vec2 &scale, const Vec2 &offset)
	{
		UVTransform = Vec4(scale.x, scale.y, offset.x, offset.y);
	}

	void GenericShaderMaterial::SetSubsurface(const Vec3 &color, const f32 strength)
	{
		Subsurface = Vec4(color.x, color.y, color.z, strength < 0.f ? 0.f : (strength > 1.f ? 1.f : strength));
	}

	void GenericShaderMaterial::SetAlphaCutoff(const f32 cutoff)
	{
		AlphaCutoff = cutoff;
	}
	void GenericShaderMaterial::SetRefractMap(const std::shared_ptr<Texture> &refractmap)
	{
		if (refractMapID == -1)
			refractMapID = Textures.size();
		else {
			Textures[refractMapID] = refractmap;
			return;
		}
		// Save on List
		Textures.push_back(refractmap);
	}
	void GenericShaderMaterial::SetSkyboxMap(const std::shared_ptr<Texture> &skyboxmap)
	{
		if (skyboxMapID == -1)
			skyboxMapID = Textures.size();
		else {
			Textures[skyboxMapID] = skyboxmap;
			return;
		}
		// Save on List
		Textures.push_back(skyboxmap);
	}
	void GenericShaderMaterial::SetTextFont(Font* font)
	{
		const std::shared_ptr<Texture> &fontTex = font->GetTextureShared();
		if (fontMapID == -1)
			fontMapID = Textures.size();
		else {
			Textures[fontMapID] = fontTex;
			return;
		}
		// Save on List
		Textures.push_back(fontTex);
	}
}
