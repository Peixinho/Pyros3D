//============================================================================
// Name        : PyrosEmbindRender.cpp
// Description : Embind renderers, FBO, materials, lights, rendering components.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Materials/GenericShaderMaterials/GenericShaderMaterial.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Materials/Shaders/Shaders.h>
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
	std::shared_ptr<CustomShaderMaterial> MakeCustomFromShader(Shader *shader)
	{
		return std::make_shared<CustomShaderMaterial>(shader);
	}

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

	// ----- DeferredRenderer IRenderer wrappers -----
	void DeferredRenderer_PreRender(DeferredRenderer &r, const std::shared_ptr<GameObject> &cam, SceneGraph &scene)
	{
		r.PreRender(cam.get(), &scene);
	}
	void DeferredRenderer_PreRenderTag(DeferredRenderer &r, const std::shared_ptr<GameObject> &cam, SceneGraph &scene, const std::string &tag)
	{
		r.PreRender(cam.get(), &scene, tag);
	}
	void DeferredRenderer_RenderScene(DeferredRenderer &r, const Projection &proj, const std::shared_ptr<GameObject> &cam, SceneGraph &scene)
	{
		r.RenderScene(proj, cam.get(), &scene);
	}
	void DeferredRenderer_RenderSceneOptions(DeferredRenderer &r, const Projection &proj, const std::shared_ptr<GameObject> &cam, SceneGraph &scene, uint32 opts)
	{
		r.RenderScene(proj, cam.get(), &scene, opts);
	}
	void DeferredRenderer_ClearBufferBit(DeferredRenderer &r, uint32 option) { r.ClearBufferBit(option); }
	void DeferredRenderer_EnableClipPlane(DeferredRenderer &r, uint32 n) { r.EnableClipPlane(n); }
	void DeferredRenderer_EnableClipPlaneDefault(DeferredRenderer &r) { r.EnableClipPlane(1); }
	void DeferredRenderer_SetBackground(DeferredRenderer &r, const Vec4 &color) { r.SetBackground(color); }
	void DeferredRenderer_UnsetBackground(DeferredRenderer &r) { r.UnsetBackground(); }
	void DeferredRenderer_SetGlobalLight(DeferredRenderer &r, const Vec4 &light) { r.SetGlobalLight(light); }
	void DeferredRenderer_Resize(DeferredRenderer &r, uint32 w, uint32 h) { r.Resize(w, h); }
	void DeferredRenderer_EnableClearDepthBuffer(DeferredRenderer &r) { r.EnableClearDepthBuffer(); }
	void DeferredRenderer_DisableClearDepthBuffer(DeferredRenderer &r) { r.DisableClearDepthBuffer(); }
	void DeferredRenderer_ClearDepthBuffer(DeferredRenderer &r) { r.ClearDepthBuffer(); }
	void DeferredRenderer_DisableClipPlane(DeferredRenderer &r) { r.DisableClipPlane(); }
	void DeferredRenderer_SetClipPlane0(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane0(p); }
	void DeferredRenderer_SetClipPlane1(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane1(p); }
	void DeferredRenderer_SetClipPlane2(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane2(p); }
	void DeferredRenderer_SetClipPlane3(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane3(p); }
	void DeferredRenderer_SetClipPlane4(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane4(p); }
	void DeferredRenderer_SetClipPlane5(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane5(p); }
	void DeferredRenderer_SetClipPlane6(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane6(p); }
	void DeferredRenderer_SetClipPlane7(DeferredRenderer &r, const Vec4 &p) { r.SetClipPlane7(p); }
	void DeferredRenderer_EnableStencil(DeferredRenderer &r) { r.EnableStencil(); }
	void DeferredRenderer_DisableStencil(DeferredRenderer &r) { r.DisableStencil(); }
	void DeferredRenderer_ClearStencilBuffer(DeferredRenderer &r) { r.ClearStencilBuffer(); }
	void DeferredRenderer_StencilFunction(DeferredRenderer &r, uint32 func, uint32 ref, uint32 mask) { r.StencilFunction(func, ref, mask); }
	void DeferredRenderer_StencilOperation(DeferredRenderer &r, uint32 sfail, uint32 dpfail, uint32 dppass) { r.StencilOperation(sfail, dpfail, dppass); }
	void DeferredRenderer_EnableScissorTest(DeferredRenderer &r) { r.EnableScissorTest(); }
	void DeferredRenderer_DisableScissorTest(DeferredRenderer &r) { r.DisableScissorTest(); }
	void DeferredRenderer_ScissorTestRect(DeferredRenderer &r, f32 x, f32 y, f32 width, f32 height) { r.ScissorTestRect(x, y, width, height); }
	void DeferredRenderer_EnableWireFrame(DeferredRenderer &r) { r.EnableWireFrame(); }
	void DeferredRenderer_DisableWireFrame(DeferredRenderer &r) { r.DisableWireFrame(); }
	void DeferredRenderer_ColorMask(DeferredRenderer &r, bool rmask, bool g, bool b, bool a) { r.ColorMask(rmask, g, b, a); }
	void DeferredRenderer_EnableSorting(DeferredRenderer &r) { r.EnableSorting(); }
	void DeferredRenderer_DisableSorting(DeferredRenderer &r) { r.DisableSorting(); }
	void DeferredRenderer_EnableLOD(DeferredRenderer &r) { r.EnableLOD(); }
	void DeferredRenderer_DisableLOD(DeferredRenderer &r) { r.DisableLOD(); }
	bool DeferredRenderer_IsUsingLOD(DeferredRenderer &r) { return r.IsUsingLOD(); }
	void DeferredRenderer_EnableDepthBias(DeferredRenderer &r, const Vec2 &factor) { r.EnableDepthBias(factor); }
	void DeferredRenderer_DisableDepthBias(DeferredRenderer &r) { r.DisableDepthBias(); }
	void DeferredRenderer_SetViewPort(DeferredRenderer &r, uint32 initX, uint32 initY, uint32 endX, uint32 endY) { r.SetViewPort(initX, initY, endX, endY); }
	void DeferredRenderer_ResetViewPort(DeferredRenderer &r) { r.ResetViewPort(); }
	void DeferredRenderer_ActivateCulling(DeferredRenderer &r, const uint32 mode) { r.ActivateCulling(mode); }
	void DeferredRenderer_DeactivateCulling(DeferredRenderer &r) { r.DeactivateCulling(); }
	void DeferredRenderer_SetSSRDistances(DeferredRenderer &r, float step, float maxd) { r.SetSSRDistances(step, maxd); }
	void DeferredRenderer_EnableSSR(DeferredRenderer &r) { r.EnableSSR(); }
	void DeferredRenderer_DisableSSR(DeferredRenderer &r) { r.DisableSSR(); }

	void Velocity_Render(VelocityRenderer &v, const Projection &proj, const std::shared_ptr<GameObject> &cam, SceneGraph &scene)
	{
		v.RenderVelocityMap(proj, cam.get(), &scene);
	}
	void Cubemap_Render(CubemapRenderer &c, SceneGraph &scene, const std::shared_ptr<GameObject> &cam, f32 nearP, f32 farP)
	{
		c.RenderCubeMap(&scene, cam.get(), nearP, farP);
	}

	void Shader_LoadFile(Shader &s, const std::string &path) { s.LoadShaderFile(path.c_str()); }
	void Shader_LoadText(Shader &s, const std::string &text) { s.LoadShaderText(text); }
	bool Shader_Compile(Shader &s, uint32 type) { return s.CompileShader(type); }
	bool Shader_CompileDef(Shader &s, uint32 type, const std::string &defs) { return s.CompileShader(type, defs); }
	bool Shader_Link(Shader &s) { return s.LinkProgram(); }
	void Shader_SendUniform(const Uniform &u, int handle) { Shader::SendUniform(u, handle); }

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
		.function("addUniform", optional_override([](IMaterial &m, const Uniform &u) {
			m.AddUniform(u);
		}))
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
		.function("bindTextures", &GenericShaderMaterial::BindTextures)
		.function("unbindTextures", &GenericShaderMaterial::UnbindTextures);

	class_<CustomShaderMaterial, base<IMaterial>>("CustomShaderMaterial")
		.smart_ptr<std::shared_ptr<CustomShaderMaterial>>("CustomShaderMaterialPtr")
		// Embind overloads only by arity — keep file ctor; Shader* via factory.
		.constructor(&MakeCustomFromFile)
		.class_function("fromShader", &MakeCustomFromShader, allow_raw_pointers())
		.function("setShader", &CustomShaderMaterial::SetShader, allow_raw_pointers())
		.function("addSampler", &CustomShaderMaterial::AddSampler);

	class_<Uniform>("Uniform")
		.constructor<>()
		.constructor<std::string, uint32, uint32>();

	class_<Shader>("Shader")
		.constructor<>()
		.function("loadShaderFile", &Shader_LoadFile)
		.function("loadShaderText", &Shader_LoadText)
		.function("compileShader", &Shader_Compile)
		.function("compileShaderDefs", &Shader_CompileDef)
		.function("deleteShader", &Shader::DeleteShader)
		.function("linkProgram", &Shader_Link)
		.function("shaderProgram", &Shader::ShaderProgram)
		.function("getUniformLocation", &Shader::GetUniformLocation)
		.function("getAttributeLocation", &Shader::GetAttributeLocation)
		.class_function("sendUniform", &Shader_SendUniform);

	class_<Renderable>("Renderable")
		.smart_ptr<std::shared_ptr<Renderable>>("RenderablePtr");

	class_<RenderingMesh>("RenderingMesh")
		.constructor<>()
		.constructor<int>()
		.function("getDrawingType", &RenderingMesh::GetDrawingType)
		.function("getGenericMaterial", &RenderingMesh_GetGenericMaterial)
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

	class_<FrameBuffer>("FrameBuffer")
		.constructor<>()
		.function("initTex", &FrameBuffer_InitTex)
		.function("initRenderBuffer", &FrameBuffer_InitRenderBuffer4)
		.function("initRenderBufferMsaa", &FrameBuffer_InitRenderBuffer5)
		.function("addAttachTex", &FrameBuffer_AddAttachTex)
		.function("addAttachRenderBuffer", &FrameBuffer_AddAttachRenderBuffer4)
		.function("addAttachRenderBufferMsaa", &FrameBuffer_AddAttachRenderBuffer5)
		.function("resize", &FrameBuffer::Resize)
		.function("bind", &FrameBuffer_Bind)
		.function("bindDefault", &FrameBuffer_BindDefault)
		.function("isBinded", &FrameBuffer::IsBinded)
		.function("getBindID", &FrameBuffer::GetBindID)
		.function("unbind", &FrameBuffer::UnBind)
		.function("getFrameBufferFormat", &FrameBuffer::GetFrameBufferFormat)
		.function("isInitialized", &FrameBuffer::IsInitialized)
		.class_function("enableMultiSample", &FrameBuffer::EnableMultisample)
		.class_function("disableMultiSample", &FrameBuffer::DisableMultisample)
		.class_function("blitFrameBuffer", &FrameBuffer::BlitFrameBuffer);

	class_<DeferredRenderer>("DeferredRenderer")
		.constructor<const uint32, const uint32, FrameBuffer *>(allow_raw_pointers())
		.function("clearBufferBit", &DeferredRenderer_ClearBufferBit)
		.function("enableClearDepthBuffer", &DeferredRenderer_EnableClearDepthBuffer)
		.function("disableClearDepthBuffer", &DeferredRenderer_DisableClearDepthBuffer)
		.function("clearDepthBuffer", &DeferredRenderer_ClearDepthBuffer)
		.function("enableClipPlane", &DeferredRenderer_EnableClipPlane)
		.function("enableClipPlaneDefault", &DeferredRenderer_EnableClipPlaneDefault)
		.function("disableClipPlane", &DeferredRenderer_DisableClipPlane)
		.function("setClipPlane0", &DeferredRenderer_SetClipPlane0)
		.function("setClipaPlane0", &DeferredRenderer_SetClipPlane0)
		.function("setClipaPlane1", &DeferredRenderer_SetClipPlane1)
		.function("setClipaPlane2", &DeferredRenderer_SetClipPlane2)
		.function("setClipaPlane3", &DeferredRenderer_SetClipPlane3)
		.function("setClipaPlane4", &DeferredRenderer_SetClipPlane4)
		.function("setClipaPlane5", &DeferredRenderer_SetClipPlane5)
		.function("setClipaPlane6", &DeferredRenderer_SetClipPlane6)
		.function("setClipaPlane7", &DeferredRenderer_SetClipPlane7)
		.function("enableStencil", &DeferredRenderer_EnableStencil)
		.function("disableStencil", &DeferredRenderer_DisableStencil)
		.function("clearStencilBuffer", &DeferredRenderer_ClearStencilBuffer)
		.function("stencilFunction", &DeferredRenderer_StencilFunction)
		.function("stencilOperation", &DeferredRenderer_StencilOperation)
		.function("enableScissorTest", &DeferredRenderer_EnableScissorTest)
		.function("disableScissorTest", &DeferredRenderer_DisableScissorTest)
		.function("scissorTestRect", &DeferredRenderer_ScissorTestRect)
		.function("enableWireFrame", &DeferredRenderer_EnableWireFrame)
		.function("disableWireFrame", &DeferredRenderer_DisableWireFrame)
		.function("colorMask", &DeferredRenderer_ColorMask)
		.function("enableSorting", &DeferredRenderer_EnableSorting)
		.function("disableSorting", &DeferredRenderer_DisableSorting)
		.function("enableLOD", &DeferredRenderer_EnableLOD)
		.function("disableLOD", &DeferredRenderer_DisableLOD)
		.function("isUsingLOD", &DeferredRenderer_IsUsingLOD)
		.function("setBackground", &DeferredRenderer_SetBackground)
		.function("unsetBackground", &DeferredRenderer_UnsetBackground)
		.function("setGlobalLight", &DeferredRenderer_SetGlobalLight)
		.function("enableDepthBias", &DeferredRenderer_EnableDepthBias)
		.function("disableDepthBias", &DeferredRenderer_DisableDepthBias)
		.function("setViewPort", &DeferredRenderer_SetViewPort)
		.function("resetViewPort", &DeferredRenderer_ResetViewPort)
		.function("resize", &DeferredRenderer_Resize)
		.function("activateCulling", &DeferredRenderer_ActivateCulling)
		.function("deactivateCulling", &DeferredRenderer_DeactivateCulling)
		.function("preRender", &DeferredRenderer_PreRender)
		.function("preRenderTag", &DeferredRenderer_PreRenderTag)
		.function("renderScene", &DeferredRenderer_RenderScene)
		.function("renderSceneOptions", &DeferredRenderer_RenderSceneOptions)
		.function("setSSRDistances", &DeferredRenderer_SetSSRDistances)
		.function("enableSSR", &DeferredRenderer_EnableSSR)
		.function("disableSSR", &DeferredRenderer_DisableSSR);

	class_<VelocityRenderer>("VelocityRenderer")
		.constructor<uint32, uint32>()
		.function("renderVelocityMap", &Velocity_Render)
		.function("resize", &VelocityRenderer::Resize)
		.function("getTexture", &VelocityRenderer::GetTexture, allow_raw_pointers());

	class_<CubemapRenderer>("CubemapRenderer")
		.constructor<uint32, uint32>()
		.function("renderCubeMap", &Cubemap_Render);
}

#endif /* EMSCRIPTEN */
