//============================================================================
// Name        : PyrosLuaRender.cpp
// Description : Renderers, FBO, materials, lights, rendering components.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Rendering/Water/OceanSim.h>
#include <Pyros3D/Rendering/Renderer/SceneView.h>
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Rendering/ColourSpace.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaRenderEarly(sol::state* lua)
	{
		// Base usertypes for mesh/material hierarchies.
		lua->new_usertype<Renderable>("Renderable");

		{
			// IMaterial - shared_ptr via sol::factories
			lua->new_usertype<IMaterial>("IMaterial",
				sol::factories(
					[]() { return std::make_shared<IMaterial>(); }
				),
				"setCullFace", &IMaterial::SetCullFace,
				"getCullFace", &IMaterial::GetCullFace,
				"isWireFrame", &IMaterial::IsWireFrame,
				"getOpacity", &IMaterial::GetOpacity,
				"isTransparent", &IMaterial::IsTransparent,
				"setOpacity", &IMaterial::SetOpacity,
				"setTransparencyFlag", &IMaterial::SetTransparencyFlag,
				"enableDepthBias", &IMaterial::EnableDethBias,
				"disableDepthBias", &IMaterial::DisableDethBias,
				"enableBlending", &IMaterial::EnableBlending,
				"disableBlending", &IMaterial::DisableBlending,
				"blendingFunction", &IMaterial::BlendingFunction,
				"blendingEquation", &IMaterial::BlendingEquation,
				"startRenderWireFrame", &IMaterial::StartRenderWireFrame,
				"stopRenderWireFrame", &IMaterial::StopRenderWireFrame,
				"enableCastingShadows", &IMaterial::EnableCastingShadows,
				"disableCastingShadows", &IMaterial::DisableCastingShadows,
				"isCastingShadows", &IMaterial::IsCastingShadows,
				"destroy", &IMaterial::Destroy,
				"getInternalID", &IMaterial::GetInternalID,
				"enableDepthTest", &IMaterial::EnableDepthTest,
				"disableDepthTest", &IMaterial::DisableDepthTest,
				"enableDepthWrite", &IMaterial::EnableDepthWrite,
				"disableDepthWrite", &IMaterial::DisableDepthWrite,
				"isDepthWritting", &IMaterial::IsDepthWritting,
				"isDepthTesting", &IMaterial::IsDepthTesting
				);
		}



		{
			sol::constructors<sol::types<int>, sol::types<>> con;
			lua->new_usertype<RenderingMesh>("RenderingMesh",
				con,
				"getDrawingType", &RenderingMesh::GetDrawingType,
				"geometry", &RenderingMesh::Geometry,
				"material", &RenderingMesh::Material,
				"getGenericMaterial", &RenderingMesh_GetGenericMaterial,
				"getCustomMaterial", &RenderingMesh_GetCustomMaterial,
				"drawingType", &RenderingMesh::drawingType,
				"renderingComponent", &RenderingMesh::renderingComponent,
				"cullingGeometry", &RenderingMesh::CullingGeometry,
				"active", &RenderingMesh::Active,
				"clickable", &RenderingMesh::Clickable,
				// (glass, a cut-out: what screenPick's last value says of the submesh hit)
				"isTransparent", [](RenderingMesh &m) { return m.Material && m.Material->IsTransparent(); },
				// (the model file's name for the submesh's material: how a script finds a part - a pane,
				// a light - on a model whose submeshes are merged by material)
				"getMaterialName", [](RenderingMesh &m) { return m.Geometry ? m.Geometry->materialProperties.Name : std::string(); }
				);
		}

		lua->new_usertype<IComponent>("IComponent",
			"getOwner", &IComponent::GetOwner,
			"enable", &IComponent::Enable,
			"disable", &IComponent::Disable,
			"isActive", &IComponent::IsActive
			);
		// Colour and intensity on the base type, so a script that got a
		// light out of getComponent() can dim it without caring which of
		// the three kinds it is.
		lua->new_usertype<ILightComponent>("IlightComponent",
			"getLightColor", &ILightComponent::GetLightColor,
			"setLightColor", &ILightComponent::SetLightColor,
			"getLightIntensity", &ILightComponent::GetLightIntensity,
			"setLightIntensity", &ILightComponent::SetLightIntensity,
			"getLightType", &ILightComponent::GetLightType,
			sol::base_classes, sol::bases<IComponent>()
			);

		// The lights a SCENE contains, as opposed to the LUA_* ones a script
		// constructs. SceneSerializer builds plain PointLight/SpotLight/
		// DirectionalLight, and sol picks a metatable from the pointer's
		// static type - so without these three, a light that came out of a
		// scene file pushed as a method-less userdata and every call on it
		// threw. Same base-type registration GameObjectBase exists for, and
		// the reason getComponent() can return a light at all.
		lua->new_usertype<PointLight>("PointLightBase",
			"getLightRadius", &PointLight::GetLightRadius,
			"setLightRadius", &PointLight::SetLightRadius,
			"getShadowFar", &PointLight::GetShadowFar,
			"setShadowBiasScale", &PointLight::SetShadowBiasScale,
			sol::base_classes, sol::bases<ILightComponent, IComponent>()
			);
		lua->new_usertype<SpotLight>("SpotLightBase",
			"getLightRadius", &SpotLight::GetLightRadius,
			"setLightRadius", &SpotLight::SetLightRadius,
			"getLightDirection", &SpotLight::GetLightDirection,
			"setLightDirection", &SpotLight::SetLightDirection,
			"getLightInnerCone", &SpotLight::GetLightInnerCone,
			"setLightInnerCone", &SpotLight::SetLightInnerCone,
			"getLightOutterCone", &SpotLight::GetLightOutterCone,
			"setLightOutterCone", &SpotLight::SetLightOutterCone,
			sol::base_classes, sol::bases<ILightComponent, IComponent>()
			);
		lua->new_usertype<DirectionalLight>("DirectionalLightBase",
			"setContactShadows", &DirectionalLight::SetContactShadows,
			"getContactShadows", &DirectionalLight::GetContactShadows,
			"getLightDirection", &DirectionalLight::GetLightDirection,
			// (on a light a scene already has - what getComponent("DirectionalLight")
			// hands back - as well as on one a script makes)
			"setShadowCascades", [](DirectionalLight &l, const uint32 cascades, const f32 reach) { l.SetShadowCascades(cascades, reach); },
			"getNumberCascades", [](DirectionalLight &l) { return l.GetNumberCascades(); },
			// The shadow cache (DirectionalLight::SetShadowCache): what stands still drawn once and kept.
			// setShadowCacheWindFrom(1): what sways in the wind kept too from the second cascade on (its
			// shadow still there); -1, the default, never. setShadowCacheTurn(radians): how far the sun turns
			// before a kept cascade is drawn again.
			"setShadowCache", [](DirectionalLight &l, const bool on) { l.SetShadowCache(on); },
			"setShadowCacheWindFrom", [](DirectionalLight &l, const int32 cascade) { l.SetShadowCacheWindFrom(cascade); },
			"setShadowCacheTurn", [](DirectionalLight &l, const f32 radians) { l.SetShadowCacheTurn(radians); },
			"setSun", [](DirectionalLight &l, const bool on) { l.SetSun(on); },
			// (a sun by the clock: 0..24, up at six, down at eighteen - see DirectionalLight::SetTimeOfDay)
			"setTimeOfDay", [](DirectionalLight &l, const f32 hour) { l.SetTimeOfDay(hour); },
			"getTimeOfDay", [](DirectionalLight &l) { return l.GetTimeOfDay(); },
			"setSunPath", [](DirectionalLight &l, const f32 azimuth, const f32 tilt) { l.SetSunPath(azimuth, tilt); },
			"setLightDirection", &DirectionalLight::SetLightDirection,
			sol::base_classes, sol::bases<ILightComponent, IComponent>()
			);
	}

	void RegisterLuaRenderMid(sol::state* lua)
	{
		{
			// Real, generic Lua-side "attach arbitrary behavior to any
			// GameObject" component - see PyrosBindings.h's LuaComponent
			// class comment. Attach via the already-bound
			// GameObject::addComponent(), same as any other component.
			lua->new_usertype<LuaComponent>("LuaComponent",
				sol::factories(
					[]() { return std::make_shared<LuaComponent>(); }
				),
				"init", &LuaComponent::Init,
				"update", &LuaComponent::Update,
				"destroy", &LuaComponent::Destroy,
				"onUpdate", &LuaComponent::on_update,
				"onInit", &LuaComponent::on_init,
				"onDestroy", &LuaComponent::on_destroy,
				"scriptFile", &LuaComponent::scriptFile,
				"data", &LuaComponent::data,
				sol::base_classes, sol::bases<IComponent>()
				);
			// Real, serializable behavior - see PyrosBindings.h's
			// LuaComponent_FromFile comment. Lower-level building block;
			// GameObject:attachScript() (below, in the GameObject
			// usertype) is the convenient one-call version most scripts
			// want.
			lua->set_function("LuaComponent_fromFile", [lua](const std::string &scriptFile) {
				return LuaComponent_FromFile(*lua, scriptFile);
			});
		}


		{
			// Directional Light - shared_ptr via sol::factories
			lua->new_usertype<LUA_DirectionalLight>("DirectionalLight",
				sol::factories(
					[]() { return std::make_shared<LUA_DirectionalLight>(); },
					[](const Vec4 &color) { return std::make_shared<LUA_DirectionalLight>(color); },
					[](const Vec4 &color, const Vec3 &direction) { return std::make_shared<LUA_DirectionalLight>(color, direction); }
				),
				"start", &LUA_DirectionalLight::Start,
				"update", &LUA_DirectionalLight::Update,
				"destroy", &LUA_DirectionalLight::Destroy,
				"enableShadows", &LUA_DirectionalLight::EnableCastShadows,
				// light:setShadowCascades(2, 60): how many cascades and how far the
				// shadow reaches, on a light already casting.
				"setShadowCascades", [](LUA_DirectionalLight &l, const uint32 cascades, const f32 reach) { l.SetShadowCascades(cascades, reach); },
				"setShadowCache", [](LUA_DirectionalLight &l, const bool on) { l.SetShadowCache(on); },
				"setShadowCacheWindFrom", [](LUA_DirectionalLight &l, const int32 cascade) { l.SetShadowCacheWindFrom(cascade); },
				"setShadowCacheTurn", [](LUA_DirectionalLight &l, const f32 radians) { l.SetShadowCacheTurn(radians); },
				"getLightProjection", &LUA_DirectionalLight::GetLightProjection,
				"updateCascadeFrustumPoints", &LUA_DirectionalLight::UpdateCascadeFrustumPoints,
				"getNumberCascades", &LUA_DirectionalLight::GetNumberCascades,
				"getCascade", &LUA_DirectionalLight::GetCascade,
				"getLightDirection", &LUA_DirectionalLight::GetLightDirection,
				"setLightDirection", &LUA_DirectionalLight::SetLightDirection,
				"getLightColor", &LUA_DirectionalLight::GetLightColor,
				"setLightColor", &LUA_DirectionalLight::SetLightColor,
				"getLightIntensity", &LUA_DirectionalLight::GetLightIntensity,
				"setLightIntensity", &LUA_DirectionalLight::SetLightIntensity,
				"onUpdate", &LUA_DirectionalLight::on_update,
				"onInit", &LUA_DirectionalLight::on_init,
				"onDestroy", &LUA_DirectionalLight::on_destroy,
				"setShadowPCFTexelSize", &LUA_DirectionalLight::SetShadowPCFTexelSize,
				// Filter radius and normal-offset bias, both in shadow-map
				// texels - see ILightComponent::SetShadowSoftness() and
				// SetShadowNormalBias(). setShadowPCFTexelSize is the old,
				// deprecated spelling of the first.
				"setShadowSoftness", &LUA_DirectionalLight::SetShadowSoftness,
				"getShadowSoftness", &LUA_DirectionalLight::GetShadowSoftness,
				// metres of soft edge, the same in every cascade - see ILightComponent::SetShadowPenumbra()
				"setShadowPenumbra", &LUA_DirectionalLight::SetShadowPenumbra,
				"getShadowPenumbra", &LUA_DirectionalLight::GetShadowPenumbra,
				"setShadowNormalBias", &LUA_DirectionalLight::SetShadowNormalBias,
				"getShadowNormalBias", &LUA_DirectionalLight::GetShadowNormalBias,
				// Shadow depth bias. Lights built from Lua defaulted to
				// 0/0 (ILightComponent's constructor), which is shadow
				// acne on every one of them, and there was no way to set
				// it without going through the scene serializer - so a
				// Lua-authored shadow-casting light could only ever be
				// biased by editing C++. Same factor/units pair
				// SceneSerializer reads.
				"setShadowBias", &LUA_DirectionalLight::SetShadowBias,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Point Light - shared_ptr via sol::factories
			lua->new_usertype<LUA_PointLight>("PointLight",
				sol::factories(
					[]() { return std::make_shared<LUA_PointLight>(); },
					[](const Vec4 &color, float radius) { return std::make_shared<LUA_PointLight>(color, radius); }
				),
				"start", &LUA_PointLight::Start,
				"update", &LUA_PointLight::Update,
				"destroy", &LUA_PointLight::Destroy,
				"enableShadows", &LUA_PointLight::EnableCastShadows,
				"getShadowFar", &LUA_PointLight::GetShadowFar,
				"getLightRadius", &LUA_PointLight::GetLightRadius,
				"setLightRadius", &LUA_PointLight::SetLightRadius,
				"getLightColor", &LUA_PointLight::GetLightColor,
				"setLightColor", &LUA_PointLight::SetLightColor,
				"getLightIntensity", &LUA_PointLight::GetLightIntensity,
				"setLightIntensity", &LUA_PointLight::SetLightIntensity,
				"onUpdate", &LUA_PointLight::on_update,
				"onInit", &LUA_PointLight::on_init,
				"onDestroy", &LUA_PointLight::on_destroy,
				"setShadowPCFTexelSize", &LUA_PointLight::SetShadowPCFTexelSize,
				// Filter radius and normal-offset bias, both in shadow-map
				// texels - see ILightComponent::SetShadowSoftness() and
				// SetShadowNormalBias(). setShadowPCFTexelSize is the old,
				// deprecated spelling of the first.
				"setShadowSoftness", &LUA_PointLight::SetShadowSoftness,
				"getShadowSoftness", &LUA_PointLight::GetShadowSoftness,
				"setShadowNormalBias", &LUA_PointLight::SetShadowNormalBias,
				"getShadowNormalBias", &LUA_PointLight::GetShadowNormalBias,
				// Shadow depth bias. Lights built from Lua defaulted to
				// 0/0 (ILightComponent's constructor), which is shadow
				// acne on every one of them, and there was no way to set
				// it without going through the scene serializer - so a
				// Lua-authored shadow-casting light could only ever be
				// biased by editing C++. Same factor/units pair
				// SceneSerializer reads.
				"setShadowBias", &LUA_PointLight::SetShadowBias,
				// Point lights only. setShadowBias above configures polygon
				// offset, which does nothing for a point light - its cube map
				// is an R32F colour attachment. This is the knob that works.
				"setShadowBiasScale", &LUA_PointLight::SetShadowBiasScale,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Spot Light - was previously two separate registrations
			// under the same Lua name "SpotLight" (the second silently
			// clobbering the first's method table in sol2's per-type
			// metatable, leaving onUpdate/onInit as effectively the only
			// reachable methods); merged into one, matching the
			// DirectionalLight/PointLight pattern. Also fixes a
			// copy-paste bug: "getLightProjection"/"setLightProjection"
			// were mapped to GetLightDirection/SetLightDirection (no
			// light-projection accessor exists on SpotLight) - renamed
			// to their real names. shared_ptr via sol::factories.
			lua->new_usertype<LUA_SpotLight>("SpotLight",
				sol::factories(
					[]() { return std::make_shared<LUA_SpotLight>(); },
					[](const Vec4 &color, float radius, const Vec3 &direction, float OutterCone, float InnerCone) { return std::make_shared<LUA_SpotLight>(color, radius, direction, OutterCone, InnerCone); }
				),
				"start", &LUA_SpotLight::Start,
				"update", &LUA_SpotLight::Update,
				"destroy", &LUA_SpotLight::Destroy,
				"enableShadows", &LUA_SpotLight::EnableCastShadows,
				"getShadowFar", &LUA_SpotLight::GetShadowFar,
				"getLightDirection", &LUA_SpotLight::GetLightDirection,
				"setLightDirection", &LUA_SpotLight::SetLightDirection,
				"getLightRadius", &LUA_SpotLight::GetLightRadius,
				"setLightRadius", &LUA_SpotLight::SetLightRadius,
				"getLightInnerCone", &LUA_SpotLight::GetLightInnerCone,
				"setLightInnerCone", &LUA_SpotLight::SetLightInnerCone,
				"getLightOutterCone", &LUA_SpotLight::GetLightOutterCone,
				"setLightOutterCone", &LUA_SpotLight::SetLightOutterCone,
				"getLightColor", &LUA_SpotLight::GetLightColor,
				"setLightColor", &LUA_SpotLight::SetLightColor,
				"getLightIntensity", &LUA_SpotLight::GetLightIntensity,
				"setLightIntensity", &LUA_SpotLight::SetLightIntensity,
				"setShadowPCFTexelSize", &LUA_SpotLight::SetShadowPCFTexelSize,
				// Filter radius and normal-offset bias, both in shadow-map
				// texels - see ILightComponent::SetShadowSoftness() and
				// SetShadowNormalBias(). setShadowPCFTexelSize is the old,
				// deprecated spelling of the first.
				"setShadowSoftness", &LUA_SpotLight::SetShadowSoftness,
				"getShadowSoftness", &LUA_SpotLight::GetShadowSoftness,
				"setShadowNormalBias", &LUA_SpotLight::SetShadowNormalBias,
				"getShadowNormalBias", &LUA_SpotLight::GetShadowNormalBias,
				// Shadow depth bias. Lights built from Lua defaulted to
				// 0/0 (ILightComponent's constructor), which is shadow
				// acne on every one of them, and there was no way to set
				// it without going through the scene serializer - so a
				// Lua-authored shadow-casting light could only ever be
				// biased by editing C++. Same factor/units pair
				// SceneSerializer reads.
				"setShadowBias", &LUA_SpotLight::SetShadowBias,
				"onUpdate", &LUA_SpotLight::on_update,
				"onInit", &LUA_SpotLight::on_init,
				"onDestroy", &LUA_SpotLight::on_destroy,
				sol::base_classes, sol::bases<IComponent>()
				);
		}


		{
			// Texture - shared_ptr via sol::factories (Stage 2 / LoadedSceneAssets)
			lua->new_usertype<Texture>("Texture",
				sol::factories(
					[]() { return std::make_shared<Texture>(); }
				),
				"loadTexture", &Texture::LoadTexture,
				"loadTextureFromMemory", &Texture::LoadTextureFromMemory,
				"createEmptyTexture", &Texture::CreateEmptyTexture,
				"setMinMagFilter", &Texture::SetMinMagFilter,
				"setRepeat", &Texture::SetRepeat,
				"enableCompareMode", &Texture::EnableCompareMode,
				"setTransparency", &Texture::SetTransparency,
				"resize", sol::overload(&Texture_Resize3, &Texture_Resize2),
				"updateData", &Texture::UpdateData,
				"UpdateMipmap", &Texture::UpdateMipmap,
				"setTextureByteAlignment", &Texture::SetTextureByteAlignment,
				"getWidth", &Texture::GetWidth,
				"getHeight", &Texture::GetHeight
				);
		}


		{
			// GenericShaderMaterial - shared_ptr via sol::factories
			lua->new_usertype<GenericShaderMaterial>("GenericShaderMaterial",
				sol::factories(
					[](int options) { return std::make_shared<GenericShaderMaterial>(options); }
				),
				"setColor", &GenericShaderMaterial::SetColor,
				"setColorRGBA", [](GenericShaderMaterial &m, const f32 r, const f32 g, const f32 b, const f32 a) { m.SetColor(Vec4(r, g, b, a)); },
				"setSpecular", &GenericShaderMaterial::SetSpecular,
				"setColorMap", &GenericShaderMaterial::SetColorMap,
				"setSpecularMap", &GenericShaderMaterial::SetSpecularMap,
				"setNormalMap", &GenericShaderMaterial::SetNormalMap,
				"setDisplacementMap", &GenericShaderMaterial::SetDisplacementMap,
				"setDisplacementHeight", &GenericShaderMaterial::SetDisplacementHeight,
				"setEnvMap", &GenericShaderMaterial::SetEnvMap,
				"setSkyboxMap", &GenericShaderMaterial::SetSkyboxMap,
				"setRefractMap", &GenericShaderMaterial::SetRefractMap,
				"addTexture", &GenericShaderMaterial::AddTexture,
				"setTextFont", &GenericShaderMaterial::SetTextFont,
				"setReflectivity", &GenericShaderMaterial::SetReflectivity,
				"setShininess", &GenericShaderMaterial::SetShininess,
				"setMetallic", &GenericShaderMaterial::SetMetallic,
				"setRoughness", &GenericShaderMaterial::SetRoughness,
				"setMetallicRoughnessMap", &GenericShaderMaterial::SetMetallicRoughnessMap,
				"setSSREnabled", &GenericShaderMaterial::SetSSREnabled,
				"setSubsurface", &GenericShaderMaterial::SetSubsurface,
				"setWetness", [](GenericShaderMaterial &m, f32 amount, sol::optional<f32> puddles, sol::optional<f32> size) { m.SetWetness(amount, puddles.value_or(0.f), size.value_or(3.f)); },
				// mat:setEmission(Vec3 colour, strength [, usesColorMap]) - light the surface gives off itself
				"setEmission", [](GenericShaderMaterial &m, const Vec3 &color, f32 strength, sol::optional<bool> usesMap) { m.SetEmission(color, strength, usesMap.value_or(false)); },
				"getEmissionStrength", &GenericShaderMaterial::GetEmissionStrength,
				"getEmissionColor", [](GenericShaderMaterial &m) { return m.GetEmissionColor(); },
				"setEmissionMap", &GenericShaderMaterial::SetEmissionMap,
				"setAlphaCutoff", &GenericShaderMaterial::SetAlphaCutoff,
				"getAlphaCutoff", &GenericShaderMaterial::GetAlphaCutoff,
				"setLightThrough", &GenericShaderMaterial::SetLightThrough,
				"setWind", sol::overload(
					[](GenericShaderMaterial &m, f32 strength) { m.SetWind(strength); },
					[](GenericShaderMaterial &m, f32 strength, f32 rate) { m.SetWind(strength, rate); },
					[](GenericShaderMaterial &m, f32 strength, f32 rate, f32 freq) { m.SetWind(strength, rate, freq); }),
				"getWind", &GenericShaderMaterial::GetWind,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}

		{
			// CustomShaderMaterial - shared_ptr via sol::factories
			lua->new_usertype<CustomShaderMaterial>("CustomShaderMaterial",
				sol::factories(
					[](const std::string &shaderFile) { return std::make_shared<CustomShaderMaterial>(shaderFile); },
					[]() { return std::make_shared<CustomShaderMaterial>(); }
				),
				// the surface the renderer draws it with (CustomShaderMaterial::SetSurfaceSource)
				"setSurface", &CustomShaderMaterial::SetSurfaceSource,
				"addSampler", &CustomShaderMaterial::AddSampler,
				// Material Editor Parameter nodes, by the name the graph
				// shows: setParameter("Tint", Vec4(1,0,0,1)) or
				// setParameter("Speed", 2.0). Shared by every object using
				// this material.
				"setParameter", sol::overload(
					[](CustomShaderMaterial &m, const std::string &name, const Vec4 &v) { m.SetParameter(name, v); },
					[](CustomShaderMaterial &m, const std::string &name, const Vec3 &v) { m.SetParameter(name, Vec4(v.x, v.y, v.z, 1.f)); },
					[](CustomShaderMaterial &m, const std::string &name, f32 v) { m.SetParameter(name, v); }),
				"getParameter", &CustomShaderMaterial::GetParameter,
				"hasParameter", &CustomShaderMaterial::HasParameter,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}
		{
			// OceanSim - an FFT sea (see Rendering/Water/OceanSim.h). ocean = OceanSim.new(); each frame ocean:update(dt);
			// ocean:bind(material) hands a custom material its eight fields (uOceanD0..3, uOceanS0..3).
			lua->new_usertype<OceanSim>("OceanSim",
				sol::factories([]() { return std::make_shared<OceanSim>(); }),
				"update", &OceanSim::Update,
				"getLength", &OceanSim::GetLength,
				"getDisplacement", &OceanSim::GetDisplacement,
				"getSlope", &OceanSim::GetSlope,
				"getLastUpdateMs", &OceanSim::GetLastUpdateMs,
				"getLastWorkMs", &OceanSim::GetLastWorkMs,
				"getStats", &OceanSim::GetStats,
				// heightAt(x, z[, wSwell, wChop, wFine]): the sea's height there, as drawn
				"heightAt", [](OceanSim &o, f32 x, f32 z, sol::optional<f32> a, sol::optional<f32> b, sol::optional<f32> c) {
					return o.HeightAt(x, z, Vec3(a.value_or(1.f), b.value_or(1.f), c.value_or(1.f)));
				},
				"bind", [](OceanSim &o, CustomShaderMaterial* m) {
					if (!m) return;
					for (uint32 c = 0; c < OceanSim::Cascades; c++)
					{
						m->AddSampler("uOceanD" + std::to_string(c), o.GetDisplacement(c));
						m->AddSampler("uOceanS" + std::to_string(c), o.GetSlope(c));
					}
				},
				// the wind: setWind(speed m/s, direction degrees[, choppiness]) - remakes the spectrum
				"setWind", [](OceanSim &o, f32 speed, f32 dir, sol::optional<f32> chop) {
					OceanSimParams p = o.GetParams();
					p.windSpeed = speed; p.windDirection = dir;
					if (chop) p.choppiness = *chop;
					o.SetParams(p);
				},
				"setSwell", [](OceanSim &o, f32 scale, f32 dir) {
					OceanSimParams p = o.GetParams();
					p.swellScale = scale; p.swellDirection = dir;
					o.SetParams(p);
				}
				);
		}
	}

	// SceneView: a second view drawn into a texture a material shows (a scope, a security camera, a mirror)
	//   local view = SceneView.new(512, 512)
	//   material:setColorMap(view:getTexture())
	//   view:render(projection, camera, scene)      -- now, ahead of the frame's own view
	static void RegisterLuaSceneView(sol::state* lua)
	{
		using p3d::SceneView;
		lua->new_usertype<SceneView>("SceneView",
			sol::factories([](const uint32 w, const uint32 h) -> std::shared_ptr<SceneView> {
				// (on the device the frame's own renderer draws with)
				p3d::SceneRenderer* main = SceneView::MainRenderer();
				if (!main || !main->GetDevice()) { echo("ERROR: SceneView.new - no renderer yet to draw it with"); return nullptr; }
				std::shared_ptr<SceneView> v = std::make_shared<SceneView>(main->GetDevice(), w, h);
				v->SetFollow(main);     // (looking as the frame's own view does - until one of the set* below)
				return v;
			}),
			"getTexture", [](SceneView &v) { return v.GetTexture(); },
			"getWidth", &SceneView::GetWidth,
			"getHeight", &SceneView::GetHeight,
			"resize", &SceneView::Resize,
			"render", [](SceneView &v, const Projection &projection, GameObject* camera, SceneGraph* scene) { v.Render(scene, camera, projection); },
			"setBackground", [](SceneView &v, const Vec4 &c) { v.SetBackground(SrgbToLinear(Vec3(c.x, c.y, c.z))); },
			"setGlobalLight", [](SceneView &v, const Vec4 &c) { v.SetFollow(nullptr); v.Renderer().SetAmbient(Vec3(c.x, c.y, c.z)); },
			"setSky", [](SceneView &v, bool on) { v.SetFollow(nullptr); v.Renderer().SetSky(on); },
			"setHaze", [](SceneView &v, float density) { v.SetFollow(nullptr); v.Renderer().SetHaze(density); },
			"setClouds", [](SceneView &v, float cover) { v.SetFollow(nullptr); v.Renderer().SetClouds(cover); },
			"setReflections", [](SceneView &v, bool on) { v.SetFollow(nullptr); v.Renderer().SetReflections(on); },
			"setAutoExposure", [](SceneView &v, bool on, sol::optional<f32> ev) { v.SetFollow(nullptr); v.Renderer().SetAutoExposure(on, ev.value_or(0.f)); },
			"setTemporalAA", [](SceneView &v, bool on) { v.Renderer().SetTemporalAA(on); },
			"setTransparentBackground", [](SceneView &v, bool on) { v.SetTransparentBackground(on); },
			"enableLOD", [](SceneView &v) { v.SetFollow(nullptr); v.Renderer().SetLOD(true); },
			"disableLOD", [](SceneView &v) { v.SetFollow(nullptr); v.Renderer().SetLOD(false); },
			"follow", [](SceneView &v) { v.SetFollow(SceneView::MainRenderer()); });
	}

	void RegisterLuaRenderLate(sol::state* lua)
	{
		RegisterLuaSceneView(lua);
		// RenderingComponent.new(mesh, material) - factories take sol::object
		// and convert explicitly (see LuaObjectToRenderable). SOL_CHECK_ARGUMENTS
		// will not match shared_ptr<Cube> to shared_ptr<Renderable> on its own.
		{
			lua->new_usertype<LUA_RenderingComponent>("RenderingComponent",
				sol::factories(
					&LuaNewRenderingComponentDist,
					&LuaNewRenderingComponent
				),
				"addLOD", sol::overload(
					&RenderingComponent_ADDLOD,
					&RenderingComponent_ADDLOD_DistOnly
				),
				"enable", &IComponent::Enable,
				"disable", &IComponent::Disable,
				"isActive", &IComponent::IsActive,
				"init", &LUA_RenderingComponent::Init,
				"update", &LUA_RenderingComponent::Update,
				"destroy", &LUA_RenderingComponent::Destroy,
				"setCullingGeometry", &LUA_RenderingComponent::SetCullingGeometry,
				"enableCullTest", &LUA_RenderingComponent::EnableCullTest,
				"disableCullTest", &LUA_RenderingComponent::DisableCullTest,
				"isCullTesting", &LUA_RenderingComponent::IsCullTesting,
				"enableCastShadows", &LUA_RenderingComponent::EnableCastShadows,
				"disableCastShadows", &LUA_RenderingComponent::DisableCastShadows,
				// rc:setAnimateWhenUnseen(false): its skeleton is posed only while
				// some renderer is drawing it; rc:wasSeenRecently() says whether.
				// rc:setShadowDetail(0.25): its shadow is drawn from its own model
				// with a quarter of the triangles. 0 puts it back.
				// rc:addSimplifiedLOD(0.2, 170): a further level of detail made from
				// its own model with a fifth of the triangles, drawn out to 170 m
				// (to any distance if not said). The component's first level
				// reaches as far as its constructor's distance.
				// rc:addHiddenLOD(): beyond the reach of its last level it is not drawn
				"addHiddenLOD", [](LUA_RenderingComponent &c) { c.AddHiddenLOD(); },
				"addSimplifiedLOD", sol::overload(
					[](LUA_RenderingComponent &c, const f32 ratio) { return c.AddSimplifiedLOD(ratio); },
					[](LUA_RenderingComponent &c, const f32 ratio, const f32 reach) { return c.AddSimplifiedLOD(ratio, reach); }),
				"setShadowDetail", [](LUA_RenderingComponent &c, const f32 ratio) { return c.SetShadowDetail(ratio); },
				// setShadowRenderable(mesh): what its shadow is cast from - a simpler copy, drawn into the
				// shadow maps in place of the model itself (RenderingComponent::SetShadowRenderable)
				"setShadowRenderable", [](LUA_RenderingComponent &c, sol::object renderable) { c.SetShadowRenderable(LuaObjectToRenderable(renderable)); },
				"getShadowDetail", [](LUA_RenderingComponent &c) { return c.GetShadowDetail(); },
				"setAnimateWhenUnseen", [](LUA_RenderingComponent &c, const bool on) { c.SetAnimateWhenUnseen(on); },
				"wasSeenRecently", [](LUA_RenderingComponent &c) { return c.WasSeenRecently(); },
				"isCastingShadows", &LUA_RenderingComponent::IsCastingShadows,
				"getRenderable", &LUA_RenderingComponent::GetRenderable,
				"getSkeleton", &LUA_RenderingComponent::GetSkeleton,
				"hasBones", &LUA_RenderingComponent::HasBones,
				"getMeshes", [](RenderingComponent &rc) -> std::vector<RenderingMesh*> & {
					// sol2 does not honour C++ default args - GetMeshes(LOD=0)
					// called as getMeshes() from Lua was failing with
					// "expected number, received no value", which aborted
					// Island's setIslandCull every frame (reflection cull)
					// and texture_anim.lua's init.
					return rc.GetMeshes(0);
				},
				"getMeshesLOD", &RenderingComponent::GetMeshes,
				// rc:dent(worldPoint, radius, worldPush): see RenderingComponent::Dent
				"dent", &RenderingComponent::Dent,
				// rc:getMorphTargets() -> { names }   rc:setMorphWeight(name, w)   rc:getMorphWeight(name)
				"getMorphTargets", [](RenderingComponent &rc, sol::this_state L) { sol::state_view lua(L); sol::table t = lua.create_table(); std::vector<std::string> n = rc.GetMorphTargetNames(); for (size_t i = 0; i < n.size(); i++) t[i + 1] = n[i]; return t; },
				"setMorphWeight", &RenderingComponent::SetMorphWeight,
				"getMorphWeight", &RenderingComponent::GetMorphWeight,
				"getLODSize", &LUA_RenderingComponent::GetLODSize,
				"getLODByDistance", &LUA_RenderingComponent::GetLODByDistance,
				"updateLOD", &LUA_RenderingComponent::UpdateLOD,
				"getComponents", &LUA_RenderingComponent::GetComponents,
				"getActiveSkeletonAnimation", &RenderingComponent_GetActiveSkeletonAnimation,
				"getActiveTextureAnimation", &RenderingComponent_GetActiveTextureAnimation,
				"onUpdate", &LUA_RenderingComponent::on_update,
				"onInit", &LUA_RenderingComponent::on_init,
				"onDestroy", &LUA_RenderingComponent::on_destroy,
				// RenderingComponent, not just IComponent: sol only performs
				// a derived->base pointer conversion for bases listed here,
				// so with IComponent alone every member bound as a free
				// function taking RenderingComponent& (getActiveSkeletonAnimation
				// / addLOD) rejected its own object with "expected userdata,
				// received sol.p3d::LUA_RenderingComponent *: value at this
				// index does not properly reflect the desired type". That is
				// what left the Skeleton Animation demo unanimated - its
				// script could never obtain the SkeletonAnimationInstance, so
				// SkeletonAnimation::Update() never ran and the model
				// rendered from uninitialised bone matrices.
				sol::base_classes, sol::bases<RenderingComponent, IComponent>()
				);
		}

        {
            // LUA RenderingInstancedComponent - shared_ptr via sol::factories
            lua->new_usertype<LUA_RenderingInstancedComponent>("RenderingInstancedComponent",
                // sol::object + LuaObjectToRenderable/LuaObjectToMaterial,
                // exactly like RenderingComponent's LuaNewRenderingComponent
                // above and for the same reason: sol will NOT match a
                // shared_ptr<Cube> (or any other concrete primitive) to a
                // shared_ptr<Renderable> parameter on its own. These
                // factories used to take the base shared_ptrs directly, so
                // every RenderingInstancedComponent.new(Cube.new(...), ...)
                // from Lua failed to match any overload - the type was bound
                // but could not actually be constructed with any mesh the
                // engine can produce.
                sol::factories(
                    [](sol::object renderableObj, sol::object materialOrOptions, int nrInstances, float boundingSphere) {
                        std::shared_ptr<Renderable> renderable = LuaObjectToRenderable(renderableObj);
                        if (!renderable)
                            throw std::runtime_error("RenderingInstancedComponent.new: first argument is not a Renderable");
                        // Material before options, same precedence as
                        // LuaNewRenderingComponent: options are numbers only.
                        std::shared_ptr<IMaterial> material = LuaObjectToMaterial(materialOrOptions);
                        if (material)
                            return std::make_shared<LUA_RenderingInstancedComponent>(renderable, material, (uint32)nrInstances, (f32)boundingSphere);
                        ShaderOptions options = 0;
                        if (LuaObjectToMaterialOptions(materialOrOptions, options))
                            return std::make_shared<LUA_RenderingInstancedComponent>(renderable, options, (uint32)nrInstances, (f32)boundingSphere);
                        throw std::runtime_error("RenderingInstancedComponent.new: second argument is not a Material or ShaderUsage options");
                    }
                ),
                // Bound here as well as on RenderingComponent: sol usertypes
                // do not inherit members, so this was a nil field on every
                // instanced component - rc:addLOD(...) raised, and because a
                // raising setup script simply stops, it looked like the LOD
                // meshes were rendering nothing.
                "addLOD", sol::overload(
                    [](LUA_RenderingInstancedComponent &c, sol::object renderable, f32 distance, sol::object materialOrOptions) {
                        RenderingComponent_ADDLOD(&c, renderable, distance, materialOrOptions);
                    },
                    [](LUA_RenderingInstancedComponent &c, sol::object renderable, f32 distance) {
                        RenderingComponent_ADDLOD_DistOnly(&c, renderable, distance);
                    }),
                "init", &LUA_RenderingInstancedComponent::Init,
                "update", &LUA_RenderingInstancedComponent::Update,
                "destroy", &LUA_RenderingInstancedComponent::Destroy,
                "setCullingGeometry", &LUA_RenderingInstancedComponent::SetCullingGeometry, "enableCullTest", &LUA_RenderingInstancedComponent::EnableCullTest,
                "disableCullTest", &LUA_RenderingInstancedComponent::DisableCullTest,
                "isCullTesting", &LUA_RenderingInstancedComponent::IsCullTesting,
                "enableCastShadows", &LUA_RenderingInstancedComponent::EnableCastShadows,
                "disableCastShadows", &LUA_RenderingInstancedComponent::DisableCastShadows,
                "isCastingShadows", &LUA_RenderingInstancedComponent::IsCastingShadows,
                "getRenderable", &LUA_RenderingInstancedComponent::GetRenderable,
                "getSkeleton", &LUA_RenderingInstancedComponent::GetSkeleton,
                "hasBones", &LUA_RenderingInstancedComponent::HasBones,
                "getMeshes", &LUA_RenderingInstancedComponent::GetMeshes,
                "getLODSize", &LUA_RenderingInstancedComponent::GetLODSize,
                "getLODByDistance", &LUA_RenderingInstancedComponent::GetLODByDistance,
                "updateLOD", &LUA_RenderingInstancedComponent::UpdateLOD,
                "getComponents", &LUA_RenderingInstancedComponent::GetComponents,
                "addBuffer", &LUA_RenderingInstancedComponent::AddBuffer,
                "removeBuffer", &LUA_RenderingInstancedComponent::RemoveBuffer,
                "numberOfInstances", &LUA_RenderingInstancedComponent::NumberOfInstances,
                "setNumberInstances", &LUA_RenderingInstancedComponent::SetNumberInstances,
                // The per-instance transforms, and the upload that makes an
                // edit to them visible. Everything above this was already
                // bound, but none of it can place an instance: without these
                // two, a Lua caller could construct the component and add it
                // to a GameObject and then had no way to say where any
                // instance goes - every one of them drew at the component's
                // own model matrix. RenderingInstancedComponent::transform is
                // a std::vector<Matrix> sized to nrInstances by the
                // constructor, so this is an index-assign, not a push.
                //
                // 1-based to match Lua's own array convention (the demo
                // scripts index everything else that way), and bounds-checked
                // rather than trusted: transform[] is written straight into a
                // GPU-mapped buffer, so an out-of-range index here is a heap
                // corruption, not a Lua error.
                "setTransform", [](LUA_RenderingInstancedComponent &c, int index, const Matrix &m) {
                    if (index < 1 || (size_t)index > c.transform.size()) return false;
                    c.transform[index - 1] = m;
                    return true;
                },
                "getTransform", [](LUA_RenderingInstancedComponent &c, int index) {
                    if (index < 1 || (size_t)index > c.transform.size()) return Matrix();
                    return c.transform[index - 1];
                },
                "updateTransforms", &LUA_RenderingInstancedComponent::UpdateTransforms,
                // Per-instance tint. Same 1-based, bounds-checked contract
                // as setTransform above and for the same reason.
                "enableInstanceColors", &LUA_RenderingInstancedComponent::EnableInstanceColors,
                "hasInstanceColors", &LUA_RenderingInstancedComponent::HasInstanceColors,
                "setInstanceColor", [](LUA_RenderingInstancedComponent &c, int index, const Vec4 &color) {
                    if (index < 1 || (size_t)index > c.instanceColor.size()) return false;
                    c.instanceColor[index - 1] = color;
                    return true;
                },
                "getInstanceColor", [](LUA_RenderingInstancedComponent &c, int index) {
                    if (index < 1 || (size_t)index > c.instanceColor.size()) return Vec4();
                    return c.instanceColor[index - 1];
                },
                "updateInstanceColors", &LUA_RenderingInstancedComponent::UpdateInstanceColors,
                // Bulk scatter in C++ - see ScatterInstances()' comment.
                // The per-instance setTransform path above costs one bound
                // call each, which is fine for a fixed set laid out at load
                // and hopeless for streaming chunks in at runtime.
                "scatterInstances", sol::overload(
                    [](LUA_RenderingInstancedComponent &c, uint32 seed, f32 sx, f32 sz, f32 h, f32 mins, f32 maxs, uint32 items, uint32 quads) {
                        c.ScatterInstances(seed, sx, sz, h, mins, maxs, items, quads);
                    },
                    [](LUA_RenderingInstancedComponent &c, uint32 seed, f32 sx, f32 sz, f32 h, f32 mins, f32 maxs, uint32 items, uint32 quads, const Vec4 &lo, const Vec4 &hi) {
                        c.ScatterInstances(seed, sx, sz, h, mins, maxs, items, quads, lo, hi);
                    }),
                "onUpdate", &LUA_RenderingInstancedComponent::on_update,
                "onInit", &LUA_RenderingInstancedComponent::on_init,
                "onDestroy", &LUA_RenderingInstancedComponent::on_destroy,
                sol::base_classes, sol::bases<IComponent>()
                );
        }

	}

	void RegisterLuaRender(sol::state* lua)
	{
		RegisterLuaRenderEarly(lua);
		RegisterLuaRenderMid(lua);
		RegisterLuaRenderLate(lua);
	}

} // namespace p3d

#endif
