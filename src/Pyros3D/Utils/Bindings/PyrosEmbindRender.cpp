//============================================================================
// Name        : PyrosEmbindRender.cpp
// Description : Embind materials, lights, rendering components.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Assets/Renderable/Renderables.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingInstancedComponent.h>
#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>

#include <memory>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;
using namespace p3d::embind_helpers;

namespace {

	std::shared_ptr<IMaterial> MakeIMaterial() { return std::make_shared<IMaterial>(); }
	std::shared_ptr<GenericShaderMaterial> MakeGenericMaterial(uint32 options)
	{
		return std::make_shared<GenericShaderMaterial>(options);
	}
	std::shared_ptr<CustomShaderMaterial> MakeCustomFromFile(const std::string &path)
	{
		return std::make_shared<CustomShaderMaterial>(path);
	}

	// Sways the vertices with height: strength, rate, spatial frequency, lean.
	void GenericShaderMaterial_SetWind(GenericShaderMaterial &m, float strength, float rate, float frequency, float lean) { m.SetWind(strength, rate, frequency, lean); }
	std::shared_ptr<RenderingComponent> MakeRenderingComponent(
		std::shared_ptr<Renderable> mesh, std::shared_ptr<IMaterial> material)
	{
		return std::make_shared<RenderingComponent>(mesh, material);
	}
	std::shared_ptr<RenderingComponent> MakeRenderingComponentOpts(
		std::shared_ptr<Renderable> mesh, uint32 options)
	{
		return std::make_shared<RenderingComponent>(mesh, options);
	}
	std::shared_ptr<RenderingComponent> MakeRenderingComponentDist(
		std::shared_ptr<Renderable> mesh, std::shared_ptr<IMaterial> material, float dist)
	{
		return std::make_shared<RenderingComponent>(mesh, material, dist);
	}
	std::shared_ptr<RenderingComponent> MakeRenderingComponentOptsDist(
		std::shared_ptr<Renderable> mesh, uint32 options, float dist)
	{
		return std::make_shared<RenderingComponent>(mesh, options, dist);
	}

	std::shared_ptr<RenderingInstancedComponent> MakeInstanced(
		std::shared_ptr<Renderable> mesh, std::shared_ptr<IMaterial> mat, int nr, float bs)
	{
		return std::make_shared<RenderingInstancedComponent>(mesh, mat, (uint32)nr, bs);
	}
	std::shared_ptr<RenderingInstancedComponent> MakeInstancedOpts(
		std::shared_ptr<Renderable> mesh, int props, int nr, float bs)
	{
		return std::make_shared<RenderingInstancedComponent>(mesh, (uint32)props, (uint32)nr, bs);
	}

	std::shared_ptr<DirectionalLight> MakeDirLight() { return std::make_shared<DirectionalLight>(); }
	std::shared_ptr<DirectionalLight> MakeDirLightColor(const Vec4 &c) { return std::make_shared<DirectionalLight>(c); }
	std::shared_ptr<DirectionalLight> MakeDirLightColorDir(const Vec4 &c, const Vec3 &d) { return std::make_shared<DirectionalLight>(c, d); }
	std::shared_ptr<PointLight> MakePointLight() { return std::make_shared<PointLight>(); }
	std::shared_ptr<PointLight> MakePointLightCR(const Vec4 &c, float r) { return std::make_shared<PointLight>(c, r); }
	std::shared_ptr<SpotLight> MakeSpotLight() { return std::make_shared<SpotLight>(); }
	std::shared_ptr<SpotLight> MakeSpotLightFull(const Vec4 &c, float r, const Vec3 &d, float outter, float inner)
	{
		return std::make_shared<SpotLight>(c, r, d, outter, inner);
	}

	void RenderingComponent_AddLOD(RenderingComponent &rc, std::shared_ptr<Renderable> mesh, float dist, std::shared_ptr<IMaterial> mat)
	{
		rc.AddLOD(mesh, dist, mat);
	}
	void RenderingComponent_AddLODOpts(RenderingComponent &rc, std::shared_ptr<Renderable> mesh, float dist, uint32 opts)
	{
		rc.AddLOD(mesh, dist, opts);
	}
	void RenderingComponent_AddLODDist(RenderingComponent &rc, std::shared_ptr<Renderable> mesh, float dist)
	{
		rc.AddLOD(mesh, dist, 0u);
	}
	size_t RenderingComponent_GetMeshCount(RenderingComponent &rc)
	{
		return rc.GetMeshes(0).size();
	}
	RenderingMesh *RenderingComponent_GetMesh(RenderingComponent &rc, uint32 index)
	{
		auto &meshes = rc.GetMeshes(0);
		return index < meshes.size() ? meshes[index] : nullptr;
	}
	SkeletonAnimationInstance *RenderingComponent_GetActiveSkeletonAnimation(RenderingComponent &rc)
	{
		return static_cast<SkeletonAnimationInstance *>(rc.GetActiveSkeletonAnimation());
	}
	TextureAnimationInstance *RenderingComponent_GetActiveTextureAnimation(RenderingComponent &rc)
	{
		return static_cast<TextureAnimationInstance *>(rc.GetActiveTextureAnimation());
	}
	std::shared_ptr<GenericShaderMaterial> RenderingMesh_GetGenericMaterial(RenderingMesh &m)
	{
		return std::dynamic_pointer_cast<GenericShaderMaterial>(m.Material);
	}
	std::shared_ptr<CustomShaderMaterial> RenderingMesh_GetCustomMaterial(RenderingMesh &m)
	{
		return std::dynamic_pointer_cast<CustomShaderMaterial>(m.Material);
	}

} // namespace

namespace p3d {
	void PyrosEmbindRenderForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_render)
{
	class_<IMaterial>("IMaterial")
		.smart_ptr<std::shared_ptr<IMaterial>>("IMaterialPtr")
		.constructor(&MakeIMaterial)
		.function("setCullFace", &IMaterial::SetCullFace)
		.function("getCullFace", &IMaterial::GetCullFace)
		.function("isWireFrame", &IMaterial::IsWireFrame)
		.function("getOpacity", &IMaterial::GetOpacity)
		.function("isTransparent", &IMaterial::IsTransparent)
		.function("setOpacity", &IMaterial::SetOpacity)
		.function("setTransparencyFlag", &IMaterial::SetTransparencyFlag)
		.function("enableDepthBias", &IMaterial::EnableDethBias)
		.function("disableDepthBias", &IMaterial::DisableDethBias)
		.function("enableBlending", &IMaterial::EnableBlending)
		.function("disableBlending", &IMaterial::DisableBlending)
		.function("blendingFunction", &IMaterial::BlendingFunction)
		.function("blendingEquation", &IMaterial::BlendingEquation)
		.function("startRenderWireFrame", &IMaterial::StartRenderWireFrame)
		.function("stopRenderWireFrame", &IMaterial::StopRenderWireFrame)
		.function("enableCastingShadows", &IMaterial::EnableCastingShadows)
		.function("disableCastingShadows", &IMaterial::DisableCastingShadows)
		.function("isCastingShadows", &IMaterial::IsCastingShadows)
		.function("destroy", &IMaterial::Destroy)
		.function("getInternalID", &IMaterial::GetInternalID)
		.function("enableDepthTest", &IMaterial::EnableDepthTest)
		.function("disableDepthTest", &IMaterial::DisableDepthTest)
		.function("enableDepthWrite", &IMaterial::EnableDepthWrite)
		.function("disableDepthWrite", &IMaterial::DisableDepthWrite)
		.function("isDepthWritting", &IMaterial::IsDepthWritting)
		.function("isDepthTesting", &IMaterial::IsDepthTesting);

	class_<GenericShaderMaterial, base<IMaterial>>("GenericShaderMaterial")
		.smart_ptr<std::shared_ptr<GenericShaderMaterial>>("GenericShaderMaterialPtr")
		.constructor(&MakeGenericMaterial)
		.function("setColor", &GenericShaderMaterial::SetColor)
		.function("setSpecular", &GenericShaderMaterial::SetSpecular)
		.function("setColorMap", &GenericShaderMaterial::SetColorMap)
		.function("setWind", &GenericShaderMaterial_SetWind)
		.function("setSpecularMap", &GenericShaderMaterial::SetSpecularMap)
		.function("setNormalMap", &GenericShaderMaterial::SetNormalMap)
		.function("setDisplacementMap", &GenericShaderMaterial::SetDisplacementMap)
		.function("setDisplacementHeight", &GenericShaderMaterial::SetDisplacementHeight)
		.function("setEnvMap", &GenericShaderMaterial::SetEnvMap)
		.function("setSkyboxMap", &GenericShaderMaterial::SetSkyboxMap)
		.function("setRefractMap", &GenericShaderMaterial::SetRefractMap)
		.function("addTexture", &GenericShaderMaterial::AddTexture)
		.function("setTextFont", &GenericShaderMaterial::SetTextFont, allow_raw_pointers())
		.function("setReflectivity", &GenericShaderMaterial::SetReflectivity)
		.function("setShininess", &GenericShaderMaterial::SetShininess)
		.function("setMetallic", &GenericShaderMaterial::SetMetallic)
		.function("setRoughness", &GenericShaderMaterial::SetRoughness)
		.function("setMetallicRoughnessMap", &GenericShaderMaterial::SetMetallicRoughnessMap)
		.function("setSSREnabled", &GenericShaderMaterial::SetSSREnabled)
		.function("setSubsurface", &GenericShaderMaterial::SetSubsurface);

	class_<CustomShaderMaterial, base<IMaterial>>("CustomShaderMaterial")
		.smart_ptr<std::shared_ptr<CustomShaderMaterial>>("CustomShaderMaterialPtr")
		// (a surface file - <path>.surface - or "" and setSurface: the material's code, see CustomShaderMaterial.h)
		.constructor(&MakeCustomFromFile)
		.function("setSurface", &CustomShaderMaterial::SetSurfaceSource)
		.function("getSurface", &CustomShaderMaterial::GetSurfaceSource)
		.function("clearSamplers", &CustomShaderMaterial::ClearSamplers)
		.function("addSampler", &CustomShaderMaterial::AddSampler)
		.function("setParameterVec4", select_overload<void(const std::string&, const Vec4&)>(&CustomShaderMaterial::SetParameter))
		.function("setParameter", select_overload<void(const std::string&, f32)>(&CustomShaderMaterial::SetParameter))
		.function("getParameter", &CustomShaderMaterial::GetParameter)
		.function("hasParameter", &CustomShaderMaterial::HasParameter);

	class_<Renderable>("Renderable")
		.smart_ptr<std::shared_ptr<Renderable>>("RenderablePtr");

	class_<RenderingMesh>("RenderingMesh")
		.constructor<>()
		.constructor<int>()
		.function("getDrawingType", &RenderingMesh::GetDrawingType)
		.function("getGenericMaterial", &RenderingMesh_GetGenericMaterial)
		.function("getCustomMaterial", &RenderingMesh_GetCustomMaterial)
		.property("active", &RenderingMesh::Active)
		.property("clickable", &RenderingMesh::Clickable)
		.property("drawingType", &RenderingMesh::drawingType);

	class_<ILightComponent, base<IComponent>>("ILightComponent")
		.smart_ptr<std::shared_ptr<ILightComponent>>("ILightComponentPtr")
		.function("getLightColor", &ILightComponent::GetLightColor)
		.function("setLightColor", &ILightComponent::SetLightColor)
		.function("getLightIntensity", &ILightComponent::GetLightIntensity)
		.function("setLightIntensity", &ILightComponent::SetLightIntensity)
		.function("setShadowPCFTexelSize", &ILightComponent::SetShadowPCFTexelSize)
		.function("setShadowSoftness", &ILightComponent::SetShadowSoftness)
		.function("getShadowSoftness", &ILightComponent::GetShadowSoftness)
		.function("setShadowPenumbra", &ILightComponent::SetShadowPenumbra)
		.function("getShadowPenumbra", &ILightComponent::GetShadowPenumbra)
		.function("setShadowNormalBias", &ILightComponent::SetShadowNormalBias)
		.function("getShadowNormalBias", &ILightComponent::GetShadowNormalBias)
		.function("isCastingShadows", &ILightComponent::IsCastingShadows)
		.function("disableCastShadows", &ILightComponent::DisableCastShadows);

	// Must base on ILightComponent so color/intensity (declared there) appear on JS prototypes.
	class_<DirectionalLight, base<ILightComponent>>("DirectionalLight")
		.smart_ptr<std::shared_ptr<DirectionalLight>>("DirectionalLightPtr")
		.constructor(&MakeDirLight)
		.constructor(&MakeDirLightColor)
		.constructor(&MakeDirLightColorDir)
		.function("enableShadows", &DirectionalLight::EnableCastShadows)
		.function("getNumberCascades", &DirectionalLight::GetNumberCascades)
		.function("getLightDirection", &DirectionalLight::GetLightDirection)
		.function("setLightDirection", &DirectionalLight::SetLightDirection);

	class_<PointLight, base<ILightComponent>>("PointLight")
		.smart_ptr<std::shared_ptr<PointLight>>("PointLightPtr")
		.constructor(&MakePointLight)
		.constructor(&MakePointLightCR)
		.function("enableShadows", &PointLight::EnableCastShadows)
		.function("getShadowFar", &PointLight::GetShadowFar)
		.function("getLightRadius", &PointLight::GetLightRadius)
		.function("setLightRadius", &PointLight::SetLightRadius);

	class_<SpotLight, base<ILightComponent>>("SpotLight")
		.smart_ptr<std::shared_ptr<SpotLight>>("SpotLightPtr")
		.constructor(&MakeSpotLight)
		.constructor(&MakeSpotLightFull)
		.function("enableShadows", &SpotLight::EnableCastShadows)
		.function("getShadowFar", &SpotLight::GetShadowFar)
		.function("getLightDirection", &SpotLight::GetLightDirection)
		.function("setLightDirection", &SpotLight::SetLightDirection)
		.function("getLightRadius", &SpotLight::GetLightRadius)
		.function("setLightRadius", &SpotLight::SetLightRadius)
		.function("getLightInnerCone", &SpotLight::GetLightInnerCone)
		.function("setLightInnerCone", &SpotLight::SetLightInnerCone)
		.function("getLightOutterCone", &SpotLight::GetLightOutterCone)
		.function("setLightOutterCone", &SpotLight::SetLightOutterCone);

	class_<RenderingComponent, base<IComponent>>("RenderingComponent")
		.smart_ptr<std::shared_ptr<RenderingComponent>>("RenderingComponentPtr")
		// Embind: one ctor per arity. Material path is primary; options via factories.
		.constructor(&MakeRenderingComponent)
		.constructor(&MakeRenderingComponentDist)
		.class_function("fromOptions", &MakeRenderingComponentOpts)
		.class_function("fromOptionsDist", &MakeRenderingComponentOptsDist)
		.function("addLOD", &RenderingComponent_AddLOD)
		.function("addLODOpts", &RenderingComponent_AddLODOpts)
		.function("addLODDist", &RenderingComponent_AddLODDist)
		.function("setCullingGeometry", &RenderingComponent::SetCullingGeometry)
		.function("enableCullTest", &RenderingComponent::EnableCullTest)
		.function("disableCullTest", &RenderingComponent::DisableCullTest)
		.function("isCullTesting", &RenderingComponent::IsCullTesting)
		.function("enableCastShadows", &RenderingComponent::EnableCastShadows)
		.function("disableCastShadows", &RenderingComponent::DisableCastShadows)
		.function("isCastingShadows", &RenderingComponent::IsCastingShadows)
		.function("getRenderable", &RenderingComponent::GetRenderable, allow_raw_pointers())
		.function("hasBones", &RenderingComponent::HasBones)
		.function("getMeshCount", &RenderingComponent_GetMeshCount)
		.function("getMesh", &RenderingComponent_GetMesh, allow_raw_pointers())
		.function("getMeshesLOD", &RenderingComponent::GetMeshes, allow_raw_pointers())
		.function("getLODSize", &RenderingComponent::GetLODSize)
		.function("getLODByDistance", &RenderingComponent::GetLODByDistance)
		.function("updateLOD", &RenderingComponent::UpdateLOD)
		.function("getActiveSkeletonAnimation", &RenderingComponent_GetActiveSkeletonAnimation, allow_raw_pointers())
		.function("getActiveTextureAnimation", &RenderingComponent_GetActiveTextureAnimation, allow_raw_pointers());

	class_<RenderingInstancedComponent, base<IComponent>>("RenderingInstancedComponent")
		.smart_ptr<std::shared_ptr<RenderingInstancedComponent>>("RenderingInstancedComponentPtr")
		.constructor(&MakeInstanced)
		.class_function("fromOptions", &MakeInstancedOpts)
		.function("setCullingGeometry", &RenderingInstancedComponent::SetCullingGeometry)
		.function("enableCullTest", &RenderingInstancedComponent::EnableCullTest)
		.function("disableCullTest", &RenderingInstancedComponent::DisableCullTest)
		.function("isCullTesting", &RenderingInstancedComponent::IsCullTesting)
		.function("enableCastShadows", &RenderingInstancedComponent::EnableCastShadows)
		.function("disableCastShadows", &RenderingInstancedComponent::DisableCastShadows)
		.function("isCastingShadows", &RenderingInstancedComponent::IsCastingShadows)
		.function("getRenderable", &RenderingInstancedComponent::GetRenderable, allow_raw_pointers())
		.function("hasBones", &RenderingInstancedComponent::HasBones)
		.function("getLODSize", &RenderingInstancedComponent::GetLODSize)
		.function("getLODByDistance", &RenderingInstancedComponent::GetLODByDistance)
		.function("updateLOD", &RenderingInstancedComponent::UpdateLOD)
		.function("numberOfInstances", &RenderingInstancedComponent::NumberOfInstances)
		.function("setNumberInstances", &RenderingInstancedComponent::SetNumberInstances);
}

#endif /* EMSCRIPTEN */
