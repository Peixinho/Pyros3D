//============================================================================
// Name        : PyrosLuaRender.cpp
// Description : Renderers, FBO, materials, lights, rendering components.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaRenderEarly(sol::state* lua)
	{
		{
			// DeferredRenderer
			sol::constructors<sol::types<float, float, FrameBuffer*>> con;
			lua->new_usertype<DeferredRenderer>("DeferredRenderer",
				con,
				"clearBufferBit", &DeferredRenderer_ClearBufferBit,
				"enableClearDepthBuffer", &DeferredRenderer::EnableClearDepthBuffer,
				"disableClearDepthBuffer", &DeferredRenderer::DisableClearDepthBuffer,
				"clearDepthBuffer", &DeferredRenderer::ClearDepthBuffer,
				"enableClipPlane", sol::overload(&DeferredRenderer_EnableClipPlane, &DeferredRenderer_EnableClipPlaneDefault),
				"disableClipPlane", &DeferredRenderer::DisableClipPlane,
				"setClipPlane0", &DeferredRenderer::SetClipPlane0,
				"setClipaPlane0", &DeferredRenderer::SetClipPlane0,
				"setClipaPlane1", &DeferredRenderer::SetClipPlane1,
				"setClipaPlane2", &DeferredRenderer::SetClipPlane2,
				"setClipaPlane3", &DeferredRenderer::SetClipPlane3,
				"setClipaPlane4", &DeferredRenderer::SetClipPlane4,
				"setClipaPlane5", &DeferredRenderer::SetClipPlane5,
				"setClipaPlane6", &DeferredRenderer::SetClipPlane6,
				"setClipaPlane7", &DeferredRenderer::SetClipPlane7,
				"enableStencil", &DeferredRenderer::EnableStencil,
				"disableStencil", &DeferredRenderer::DisableStencil,
				"clearStencilBuffer", &DeferredRenderer::ClearStencilBuffer,
				"stencilFunction", &DeferredRenderer::StencilFunction,
				"stencilOperation", &DeferredRenderer::StencilOperation,
				"enableScissorTest", &DeferredRenderer::EnableScissorTest,
				"disableScissorTest", &DeferredRenderer::DisableScissorTest,
				"scissorTestRect", &DeferredRenderer::ScissorTestRect,
				"enableWireFrame", &DeferredRenderer::EnableWireFrame,
				"disableWireFrame", &DeferredRenderer::DisableWireFrame,
				"colorMask", &DeferredRenderer::ColorMask,
				"enableSorting", &DeferredRenderer::EnableSorting,
				"disableSorting", &DeferredRenderer::DisableSorting,
				"enableLOD", &DeferredRenderer::EnableLOD,
				"disableLOD", &DeferredRenderer::DisableLOD,
				"isUsingLOD", &DeferredRenderer::IsUsingLOD,
				"setBackground", &DeferredRenderer::SetBackground,
				"unsetBackground", &DeferredRenderer::UnsetBackground,
				"setGlobalLight", &DeferredRenderer::SetGlobalLight,
				"enableDepthBias", &DeferredRenderer::EnableDepthBias,
				"disableDepthBias", &DeferredRenderer::DisableDepthBias,
				"setViewPort", &DeferredRenderer::SetViewPort,
				"resetViewPort", &DeferredRenderer::ResetViewPort,
				"resize", &DeferredRenderer::Resize,
				"activateCulling", &DeferredRenderer::ActivateCulling,
				"deactivateCulling", &DeferredRenderer::DeactivateCulling,
				"renderScene", sol::overload(&DeferredRenderer_RenderScene, &DeferredRenderer_RenderSceneOptions),
				"preRender", sol::overload(&DeferredRenderer_PreRender, &DeferredRenderer_PreRenderTag),
				"setSSRDistances", &DeferredRenderer::SetSSRDistances,
				"enableSSR", &DeferredRenderer::EnableSSR,
				"disableSSR", &DeferredRenderer::DisableSSR
				);
		}

		{
			// VelocityRenderer - used by DemoLauncher motion-blur demos.
			sol::constructors<sol::types<uint32, uint32>> con;
			lua->new_usertype<VelocityRenderer>("VelocityRenderer",
				con,
				"renderVelocityMap", &VelocityRenderer::RenderVelocityMap,
				"resize", &VelocityRenderer::Resize,
				"getTexture", &VelocityRenderer::GetTexture
				);
		}

		{
			// CubemapRenderer
			sol::constructors<sol::types<float, float>> con;
			lua->new_usertype<CubemapRenderer>("CubemapRenderer",
				con,
				"renderCubeMap", &CubemapRenderer::RenderCubeMap
				);
		}

		{
			// Frame Buffer
			sol::constructors<sol::types<>> con;
			lua->new_usertype<FrameBuffer>("FrameBuffer",
				con,
				"init", sol::overload(
					&FrameBuffer_InitTex,
					&FrameBuffer_InitRenderBuffer4,
					&FrameBuffer_InitRenderBuffer5
				),
				"addAttach", sol::overload(
					&FrameBuffer_AddAttachTex,
					&FrameBuffer_AddAttachRenderBuffer4,
					&FrameBuffer_AddAttachRenderBuffer5
				),
				"resize", &FrameBuffer::Resize,
				"bind", sol::overload(&FrameBuffer_Bind, &FrameBuffer_BindDefault),
				"isBinded", &FrameBuffer::IsBinded,
				"getBindID", &FrameBuffer::GetBindID,
				"unbind", &FrameBuffer::UnBind,
				"getAttachments", &FrameBuffer::GetAttachments,
				"getFrameBufferFormat", &FrameBuffer::GetFrameBufferFormat,
				"isInitialized", &FrameBuffer::IsInitialized,
				"enableMultiSample", &FrameBuffer::EnableMultisample,
				"disableMultiSample", &FrameBuffer::DisableMultisample,
				"blitFrameBuffer", &FrameBuffer::BlitFrameBuffer
				);
		}

		// Base usertypes for mesh/material hierarchies.
		lua->new_usertype<Renderable>("Renderable");

		{
			// IMaterial - shared_ptr via sol::factories
			lua->new_usertype<IMaterial>("IMaterial",
				sol::factories(
					[]() { return std::make_shared<IMaterial>(); }
				),
				"preRender", &IMaterial::PreRender,
				"render", &IMaterial::Render,
				"afterRender", &IMaterial::AfterRender,
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
				"addUniform", &IMaterial::AddUniform,
				"startRenderWireFrame", &IMaterial::StartRenderWireFrame,
				"stopRenderWireFrame", &IMaterial::StopRenderWireFrame,
				"enableCastingShadows", &IMaterial::EnableCastingShadows,
				"disableCastingShadows", &IMaterial::DisableCastingShadows,
				"isCastingShadows", &IMaterial::IsCastingShadows,
				"destroy", &IMaterial::Destroy,
				"getShader", &IMaterial::GetShader,
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
				"drawingType", &RenderingMesh::drawingType,
				"renderingComponent", &RenderingMesh::renderingComponent,
				"cullingGeometry", &RenderingMesh::CullingGeometry,
				"active", &RenderingMesh::Active,
				"clickable", &RenderingMesh::Clickable
				);
		}

		lua->new_usertype<IComponent>("IComponent",
			"getOwner", &IComponent::GetOwner,
			"enable", &IComponent::Enable,
			"disable", &IComponent::Disable,
			"isActive", &IComponent::IsActive
			);
		lua->new_usertype<ILightComponent>("IlightComponent",
			sol::base_classes, sol::bases<IComponent>()
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
				"onUpdate", &LUA_SpotLight::on_update,
				"onInit", &LUA_SpotLight::on_init,
				"onDestroy", &LUA_SpotLight::on_destroy,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Uniform
			sol::constructors<sol::types<>, sol::types<std::string, int, int>, sol::types<std::string, int, void*, int>> con;
			lua->new_usertype<Uniform>("Uniform",
				con,
				"setValue", &Uniform::SetValue
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
				"getBindID", &Texture::GetBindID,
				"getWidth", &Texture::GetWidth,
				"getHeight", &Texture::GetHeight,
				"bind", &Texture::Bind,
				"unbind", &Texture::Unbind,
				"deleteTexture", &Texture::DeleteTexture,
				"getLastBindedUnit", &Texture::GetLastBindedUnit
				);
		}

		{
			// Shader
			sol::constructors<sol::types<>> con;
			lua->new_usertype<Shader>("Shader",
				con,
				"loadShaderFile", &Shader::LoadShaderFile,
				"loadShaderText", &Shader::LoadShaderText,
				"compileShader", &Shader::CompileShader,
				"deleteShader", &Shader::DeleteShader,
				"linkProgram", &Shader::LinkProgram,
				"shaderProgram", &Shader::ShaderProgram,
				"getUniformLocation", &Shader::GetUniformLocation,
				"getAttributeLocation", &Shader::GetAttributeLocation,
				"SendUniform", sol::overload(
					sol::resolve<void(const Uniform&, const int)>(&Shader::SendUniform),
					sol::resolve<void(const Uniform&, void*, const int, const unsigned int)>(&Shader::SendUniform)
				)
				);
		}

		{
			// GenericShaderMaterial - shared_ptr via sol::factories
			lua->new_usertype<GenericShaderMaterial>("GenericShaderMaterial",
				sol::factories(
					[](int options) { return std::make_shared<GenericShaderMaterial>(options); }
				),
				"setColor", &GenericShaderMaterial::SetColor,
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
				"bindTextures", &GenericShaderMaterial::BindTextures,
				"setAlphaCutoff", &GenericShaderMaterial::SetAlphaCutoff,
				"getAlphaCutoff", &GenericShaderMaterial::GetAlphaCutoff,
				"setWind", sol::overload(
					[](GenericShaderMaterial &m, f32 strength) { m.SetWind(strength); },
					[](GenericShaderMaterial &m, f32 strength, f32 rate) { m.SetWind(strength, rate); },
					[](GenericShaderMaterial &m, f32 strength, f32 rate, f32 freq) { m.SetWind(strength, rate, freq); }),
				"getWind", &GenericShaderMaterial::GetWind,
				"unbindTextures", &GenericShaderMaterial::UnbindTextures,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}

		{
			// CustomShaderMaterial - shared_ptr via sol::factories
			lua->new_usertype<CustomShaderMaterial>("CustomShaderMaterial",
				sol::factories(
					[](const std::string &shaderFile) { return std::make_shared<CustomShaderMaterial>(shaderFile); },
					[](Shader* shader) { return std::make_shared<CustomShaderMaterial>(shader); }
				),
				"setShader", &CustomShaderMaterial::SetShader,
				"addSampler", &CustomShaderMaterial::AddSampler,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}
		{
			lua->new_enum("UniformUsage",
				"ProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix,
				"ViewMatrix", Uniforms::DataUsage::ViewMatrix,
				"ModelMatrix", Uniforms::DataUsage::ModelMatrix,
				"CameraPosition", Uniforms::DataUsage::CameraPosition,
				"Timer", Uniforms::DataUsage::Timer,
				"NearFarPlane", Uniforms::DataUsage::NearFarPlane,
				"Other", Uniforms::DataUsage::Other
			);
			lua->new_enum("UniformDataType",
				"Int", Uniforms::DataType::Int,
				"Float", Uniforms::DataType::Float,
				"Vec2", Uniforms::DataType::Vec2,
				"Vec3", Uniforms::DataType::Vec3,
				"Vec4", Uniforms::DataType::Vec4,
				"Matrix", Uniforms::DataType::Matrix
			);
			// Configure IMaterial::extraUniforms[index] from Lua (WaterShader blocks, etc.).
			lua->set_function("setMaterialExtraUniformBlock",
				[](IMaterial &m, int index, uint32 binding, const std::string &blockName, uint32 size, sol::table offsets) {
					std::map<std::string, uint32> off;
					offsets.for_each([&](sol::object key, sol::object value) {
						if (key.get_type() == sol::type::string && value.get_type() == sol::type::number)
							off[key.as<std::string>()] = (uint32)value.as<double>();
					});
					m.SetExtraUniformBlock(index, binding, blockName, size, off);
				});
		}

	}

	void RegisterLuaRenderLate(sol::state* lua)
	{
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
                        uint32 options = 0;
                        if (LuaObjectToMaterialOptions(materialOrOptions, options))
                            return std::make_shared<LUA_RenderingInstancedComponent>(renderable, options, (uint32)nrInstances, (f32)boundingSphere);
                        throw std::runtime_error("RenderingInstancedComponent.new: second argument is not a Material or ShaderUsage options");
                    }
                ),
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
