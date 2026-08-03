//============================================================================
// Name        : PyrosBindings.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : lua Bindings
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosBindings.h>
namespace p3d {

	// ******************************* OVERLOADS *******************************
	// Vec2
	Vec2 Vec2_operator_add(Vec2 &v1, const Vec2 &v2)
	{
		return v1 + v2;
	}
	Vec2 Vec2_operator_sub(Vec2 &v1, const Vec2 &v2)
	{
		return v1 - v2;
	}
	Vec2 Vec2_operator_mul(Vec2 &v1, const Vec2 &v2)
	{
		return v1 * v2;
	}
	Vec2 Vec2_operator_div(Vec2 &v1, const Vec2 &v2)
	{
		return v1 / v2;
	}
	Vec2 Vec2_operator_addS(Vec2 &v1, float f)
	{
		return v1 + f;
	}
	Vec2 Vec2_operator_subS(Vec2 &v1, float f)
	{
		return v1 - f;
	}
	Vec2 Vec2_operator_mulS(Vec2 &v1, float f)
	{
		return v1 * f;
	}
	Vec2 Vec2_operator_divS(Vec2 &v1, float f)
	{
		return v1 / f;
	}
	void Vec2_operator_Eadd(Vec2 &v1, const Vec2 &v2)
	{
		return v1 += v2;
	}
	void Vec2_operator_Esub(Vec2 &v1, const Vec2 &v2)
	{
		return v1 -= v2;
	}
	void Vec2_operator_Emul(Vec2 &v1, const Vec2 &v2)
	{
		return v1 *= v2;
	}
	void Vec2_operator_Ediv(Vec2 &v1, const Vec2 &v2)
	{
		return v1 /= v2;
	}
	void Vec2_operator_EaddS(Vec2 &v1, float f)
	{
		return v1 += f;
	}
	void Vec2_operator_EsubS(Vec2 &v1, float f)
	{
		return v1 -=  f;
	}
	void Vec2_operator_EmulS(Vec2 &v1, float f)
	{
		return v1 *= f;
	}
	void Vec2_operator_EdivS(Vec2 &v1, float f)
	{
		v1 /= f;
	}

	// Vec3
	Vec3 Vec3_operator_add(Vec3 &v1, const Vec3 &v2)
	{
		return v1 + v2;
	}
	Vec3 Vec3_operator_sub(Vec3 &v1, const Vec3 &v2)
	{
		return v1 - v2;
	}
	Vec3 Vec3_operator_mul(Vec3 &v1, const Vec3 &v2)
	{
		return v1 * v2;
	}
	Vec3 Vec3_operator_div(Vec3 &v1, const Vec3 &v2)
	{
		return v1 / v2;
	}
	Vec3 Vec3_operator_addS(Vec3 &v1, float f)
	{
		return v1 + f;
	}
	Vec3 Vec3_operator_subS(Vec3 &v1, float f)
	{
		return v1 - f;
	}
	Vec3 Vec3_operator_mulS(Vec3 &v1, float f)
	{
		return v1 * f;
	}
	Vec3 Vec3_operator_divS(Vec3 &v1, float f)
	{
		return v1 / f;
	}
	void Vec3_operator_Eadd(Vec3 &v1, const Vec3 &v2)
	{
		return v1 += v2;
	}
	void Vec3_operator_Esub(Vec3 &v1, const Vec3 &v2)
	{
		return v1 -= v2;
	}
	void Vec3_operator_Emul(Vec3 &v1, const Vec3 &v2)
	{
		return v1 *= v2;
	}
	void Vec3_operator_Ediv(Vec3 &v1, const Vec3 &v2)
	{
		return v1 /= v2;
	}
	void Vec3_operator_EaddS(Vec3 &v1, float f)
	{
		return v1 += f;
	}
	void Vec3_operator_EsubS(Vec3 &v1, float f)
	{
		return v1 -= f;
	}
	void Vec3_operator_EmulS(Vec3 &v1, float f)
	{
		return v1 *= f;
	}
	void Vec3_operator_EdivS(Vec3 &v1, float f)
	{
		v1 /= f;
	}

	// Vec4
	Vec4 Vec4_operator_add(Vec4 &v1, const Vec4 &v2)
	{
		return v1 + v2;
	}
	Vec4 Vec4_operator_sub(Vec4 &v1, const Vec4 &v2)
	{
		return v1 - v2;
	}
	Vec4 Vec4_operator_mul(Vec4 &v1, const Vec4 &v2)
	{
		return v1 * v2;
	}
	Vec4 Vec4_operator_div(Vec4 &v1, const Vec4 &v2)
	{
		return v1 / v2;
	}
	Vec4 Vec4_operator_addS(Vec4 &v1, float f)
	{
		return v1 + f;
	}
	Vec4 Vec4_operator_subS(Vec4 &v1, float f)
	{
		return v1 - f;
	}
	Vec4 Vec4_operator_mulS(Vec4 &v1, float f)
	{
		return v1 * f;
	}
	Vec4 Vec4_operator_divS(Vec4 &v1, float f)
	{
		return v1 / f;
	}
	void Vec4_operator_Eadd(Vec4 &v1, const Vec4 &v2)
	{
		return v1 += v2;
	}
	void Vec4_operator_Esub(Vec4 &v1, const Vec4 &v2)
	{
		return v1 -= v2;
	}
	void Vec4_operator_Emul(Vec4 &v1, const Vec4 &v2)
	{
		return v1 *= v2;
	}
	void Vec4_operator_Ediv(Vec4 &v1, const Vec4 &v2)
	{
		return v1 /= v2;
	}
	void Vec4_operator_EaddS(Vec4 &v1, float f)
	{
		return v1 += f;
	}
	void Vec4_operator_EsubS(Vec4 &v1, float f)
	{
		return v1 -= f;
	}
	void Vec4_operator_EmulS(Vec4 &v1, float f)
	{
		return v1 *= f;
	}
	void Vec4_operator_EdivS(Vec4 &v1, float f)
	{
		v1 /= f;
	}

	// Matrix
	Matrix Matrix_operator_mul(Matrix &m1, const Matrix &m2)
	{
		return m1 * m2;
	}
	Matrix Matrix_operator_mulS(Matrix &m, const f32 f) 
	{
		return m * f;
	}
	Vec3 Matrix_operator_mulVec3(Matrix &m, const Vec3 &v)
	{
		return m * v;
	}
	Vec4 Matrix_operator_mulVec4(Matrix &m, const Vec4 &v)
	{
		return m * v;
	}
	void Matrix_operator_Emul(Matrix &m1, const Matrix &m2)
	{
		m1 *= m2;
	}
	void Matrix_lookAt(Math::Matrix &m, const Math::Vec3 &eye, const Math::Vec3 &center, const Math::Vec3 &up)
	{
		m.LookAt(eye, center, up);
	}
	void Matrix_lookAt2(Math::Matrix &m, const Math::Vec3 &eye, const Math::Vec3 &center)
	{
		m.LookAt(eye, center);
	}
	void Matrix_translateXYZ(Math::Matrix &m, float x, float y, float z)
	{
		m.Translate(x, y, z);
	}
	void Matrix_translateVec3(Math::Matrix &m, const Math::Vec3 &v)
	{
		m.Translate(v);
	}
	void Matrix_scaleXYZ(Math::Matrix &m, float x, float y, float z)
	{
		m.Scale(x, y, z);
	}
	void Matrix_scaleVec3(Math::Matrix &m, const Math::Vec3 &v)
	{
		m.Scale(v);
	}
	// Quaternion
	Quaternion Quaternion_operator_mul(Quaternion &q1, const Quaternion &q2) 
	{
		return q1 * q2;
	}
	Quaternion Quaternion_operator_mulS(Quaternion &q, const f32 s)
	{
		return q * s;
	}
	Vec3 Quaternion_operator_mulVec3(Quaternion &q, const Vec3 &v)
	{
		return q * v;
	}
	Quaternion Quaternion_operator_negate(Quaternion &q)
	{
		return -q;
	}

	// SceneGraph - save/load, see Utils/Serialization/SceneSerializer.h.
	// Bound as lambdas (not plain free functions) at the usertype
	// registration site below - both need `lua` itself (to save/restore
	// real LuaComponent behavior), only available inside GenerateBindings.
	// GameObject
	bool GameObject_HaveTagSTR(GameObject &g, const std::string &tag)
	{
		return g.HaveTag(tag);
	}
	bool GameObject_HaveTagUINT(GameObject &g, const uint32 tag)
	{
		return g.HaveTag(tag);
	}
	// Real, minimal "find a sibling component by type" for Lua behavior
	// scripts attached via attachScript() - e.g. a skeleton-animation-
	// driving script reaching the RenderingComponent on the same
	// GameObject, or a particle-tuning script reaching its ParticleSystem.
	// Covers only the component types an attached script actually needs
	// to look up this way today; grows on demand rather than trying to
	// genericize every ComponentType up front. Takes a plain GameObject&
	// (not LUA_GameObject&) and sol::this_state instead of a captured
	// lua* so it can be bound identically on both the LUA_GameObject and
	// base GameObject usertypes - self.owner in a script is always the
	// latter's static type (see the GameObjectBase usertype's comment).
	sol::object GameObject_GetComponent(GameObject &go, const std::string &typeName, sol::this_state s)
	{
		sol::state_view lua(s);
		for (IComponent* c : go.GetComponents())
		{
			if (typeName == "RenderingComponent" && c->GetComponentType() == ComponentType::RenderingComponent)
			{
				// Must be pushed as LUA_RenderingComponent, the type actually
				// registered as a sol usertype - sol picks the metatable from
				// the pointer's *static* type, so pushing a plain
				// RenderingComponent* yields a bare userdata that throws
				// "attempt to index a sol.p3d::RenderingComponent * value" on
				// the first method call. Anything built by SceneSerializer
				// with a Lua state is already a LUA_RenderingComponent (see
				// its DeserializeComponent comment); return nil rather than
				// that unusable userdata for anything else, so a script's
				// `if rc then` guard actually guards.
				LUA_RenderingComponent* lrc = dynamic_cast<LUA_RenderingComponent*>(c);
				if (lrc) return sol::make_object(lua, lrc);
				return sol::lua_nil;
			}
			if (typeName == "ParticleSystem" && c->GetComponentType() == ComponentType::ParticleSystem)
				return sol::make_object(lua, static_cast<ParticleSystem*>(c));
		}
		return sol::lua_nil;
	}
	// ForwardRenderer
	void ForwardRenderer_PreRender(ForwardRenderer &r, GameObject* Camera, SceneGraph* Scene)
	{
		r.PreRender(Camera, Scene);
	}
	void ForwardRenderer_PreRenderTag(ForwardRenderer &r, GameObject* Camera, SceneGraph* Scene, const std::string &tag)
	{
		r.PreRender(Camera, Scene, tag);
	}
	// DeferredRenderer
	void DeferredRenderer_PreRender(DeferredRenderer &r, GameObject* Camera, SceneGraph* Scene)
	{
		r.PreRender(Camera, Scene);
	}
	void DeferredRenderer_PreRenderTag(DeferredRenderer &r, GameObject* Camera, SceneGraph* Scene, const std::string &tag)
	{
		r.PreRender(Camera, Scene, tag);
	}
	// DeferredRenderer::RenderScene() carries a fourth BufferOptions
	// parameter that ForwardRenderer's does not. sol binds the function's
	// full arity, so C++ default arguments are not optional from Lua -
	// without this wrapper a script had to pass BufferOptions explicitly,
	// which it could not even spell: the values come from Buffer_Bit,
	// and only the unrelated FBOBufferBit enum (0/1/2 rather than
	// 0x10/0x20/0x40) was exposed. Restoring the default here also makes
	// renderScene() identical across both renderers, so a script can swap
	// one for the other without touching its render loop.
	void DeferredRenderer_RenderScene(DeferredRenderer &r, const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene)
	{
		r.RenderScene(projection, Camera, Scene);
	}
	void DeferredRenderer_RenderSceneOptions(DeferredRenderer &r, const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions)
	{
		r.RenderScene(projection, Camera, Scene, BufferOptions);
	}
	// Shader
	void Shader_SendUniform(Shader &s, const Uniform &uniform, int32 Handle)
	{
		Shader::SendUniform(uniform, Handle);
	}
	void Shader_SendUniformPTR(Shader &s, const Uniform &uniform, void* data, int32 Handle, uint32 elementCount)
	{
		Shader::SendUniform(uniform, data, Handle, elementCount);
	}
	// Skeleton Animation Instance
	void SkeletonAnimationInstance_AddBone(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone)
	{
		a.AddBone(LayerID, bone);
	}
	void SkeletonAnimationInstance_AddBoneSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone)
	{
		a.AddBone(LayerName, bone);
	}
	void SkeletonAnimationInstance_AddBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive = true)
	{
		a.AddBoneAndChilds(LayerID, bone, inclusive);
	}
	void SkeletonAnimationInstance_AddBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive = true)
	{
		a.AddBoneAndChilds(LayerName, bone, inclusive);
	}
	void SkeletonAnimationInstance_RemoveBone(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone)
	{
		a.RemoveBone(LayerID, bone);
	}
	void SkeletonAnimationInstance_RemoveBoneSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone)
	{
		a.RemoveBone(LayerName, bone);
	}
	void SkeletonAnimationInstance_RemoveBoneAndChilds(SkeletonAnimationInstance &a, const uint32 LayerID, const std::string &bone, bool inclusive = true)
	{
		a.AddBoneAndChilds(LayerID, bone, inclusive);
	}
	void SkeletonAnimationInstance_RemoveBoneAndChildsSTR(SkeletonAnimationInstance &a, const std::string &LayerName, const std::string &bone, bool inclusive = true)
	{
		a.AddBoneAndChilds(LayerName, bone, inclusive);
	}
	bool SkeletonAnimationInstance_IsPaused(SkeletonAnimationInstance &a)
	{
		return a.IsPaused();
	}
	bool SkeletonAnimationInstance_IsPausedID(SkeletonAnimationInstance &a, int ID)
	{
		return a.IsPaused(ID);
	}
	void SkeletonAnimationInstance_DestroyLayer(SkeletonAnimationInstance &a, int id)
	{
		a.DestroyLayer(id);
	}
	void SkeletonAnimationInstance_DestroyLayerSTR(SkeletonAnimationInstance &a, const std::string &str)
	{
		a.DestroyLayer(str);
	}
	// Text
	void Text_UpdateText(Text &t, const std::string &text, const Vec4 &color = Vec4(1, 1, 1, 1))
	{
		t.UpdateText(text, color);
	}
	void Text_UpdateTextColors(Text &t, const std::string &text, const std::vector<Vec4> &color)
	{
		t.UpdateText(text, color);
	}
	// IPhysics
	IPhysicsComponent* IPhysics_CreateTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass = 0.f)
	{
		return p.CreateTriangleMesh(rcomp, mass);
	}
	IPhysicsComponent* IPhysics_CreateTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f)
	{
		return p.CreateTriangleMesh(index, vertex, mass);
	}
	IPhysicsComponent* IPhysics_CreateConvexTriangleMeshRCOMP(IPhysics &p, RenderingComponent* rcomp, const f32 mass = 0.f)
	{
		return p.CreateConvexTriangleMesh(rcomp, mass);
	}
	IPhysicsComponent* IPhysics_CreateConvexTriangleMesh(IPhysics &p, const std::vector<uint32> &index, const std::vector<Vec3> &vertex, const f32 mass = 0.f)
	{
		return p.CreateConvexTriangleMesh(index, vertex, mass);
	}
	// Frame Buffer
	void FrameBuffer_Init(FrameBuffer &f, const uint32 attachmentFormat, const uint32 TextureType, Texture* attachment)
	{
		f.Init(attachmentFormat, TextureType, attachment);
	}
	void FrameBuffer_InitRenderBuffer(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa = 0)
	{
		f.Init(attachmentFormat, attachmentDataType, Width, Height, msaa);
	}
	void FrameBuffer_AddAttach(FrameBuffer &f, const uint32 attachmentFormat, const uint32 TextureType, Texture* attachment)
	{
		f.AddAttach(attachmentFormat, TextureType, attachment);
	}
	void FrameBuffer_AddAttachRenderBuffer(FrameBuffer &f, const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa = 0)
	{
		f.AddAttach(attachmentFormat, attachmentDataType, Width, Height, msaa);
	}
	// RenderingComponent::GetActiveSkeletonAnimation() returns void* (see
	// its header comment - avoids a circular #include), so it can't be
	// bound to Lua directly; this wraps the documented, guaranteed-safe
	// cast back to SkeletonAnimationInstance* (only ever set from that
	// class's own constructor) so a Lua behavior script attached to the
	// same GameObject can reach the skeleton animation actually driving
	// its model (e.g. via owner:getComponent("RenderingComponent")).
	SkeletonAnimationInstance* RenderingComponent_GetActiveSkeletonAnimation(RenderingComponent &rc)
	{
		return static_cast<SkeletonAnimationInstance*>(rc.GetActiveSkeletonAnimation());
	}
	void RenderingComponent_ADDLOD_MaterialOptions(RenderingComponent* rcomp, Renderable* renderable, const f32 Distance, IMaterial* Material)
	{
		rcomp->AddLOD(renderable, Distance, Material);
	}
	void RenderingComponent_ADDLOD_MaterialPointer(RenderingComponent* rcomp, Renderable* renderable, const f32 Distance, const uint32 MaterialOptions = 0)
	{
		rcomp->AddLOD(renderable, Distance, MaterialOptions);
	}
	// ******************************* OVERLOADS *******************************

	void GenerateBindings(sol::state* lua)
	{
		lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::coroutine, sol::lib::table, sol::lib::string);

		// ******************************* ENUMS *******************************
		{
			// Shader Usage
			lua->new_enum("ShaderUsage",
				"Color", ShaderUsage::Color,
				"Texture", ShaderUsage::Texture,
				"EnvMap", ShaderUsage::EnvMap,
				"SkyBox", ShaderUsage::Skybox,
				"Refraction", ShaderUsage::Refraction,
				"Skinning", ShaderUsage::Skinning,
				"CellShading", ShaderUsage::CellShading,
				"BumpMapping", ShaderUsage::BumpMapping,
				"SpecularMap", ShaderUsage::SpecularMap,
				"SpecularColor", ShaderUsage::SpecularColor,
				"DirectionalShadow", ShaderUsage::DirectionalShadow,
				"PointShadow", ShaderUsage::PointShadow,
				"SpotShadow", ShaderUsage::SpotShadow,
				"CastShadows", ShaderUsage::CastShadows,
				"Diffuse", ShaderUsage::Diffuse,
				"TextRendering", ShaderUsage::TextRendering,
				"DebugRendering", ShaderUsage::DebugRendering,
				"ClipPlane", ShaderUsage::ClipPlane,
				// The remaining ShaderLib.h flags. Without these a Lua scene
				// could construct a DeferredRenderer but had no way to build
				// a material that writes the G-buffer, so every object was
				// silently missing from the deferred pass - the enum stopped
				// at ClipPlane while the C++ enum carried six more values.
				"DeferredRenderer_Gbuffer", ShaderUsage::DeferredRenderer_Gbuffer,
				"ParallaxMapping", ShaderUsage::ParallaxMapping,
				"InstancedRendering", ShaderUsage::InstancedRendering,
				"VelocityRendering", ShaderUsage::VelocityRendering,
				"PBR", ShaderUsage::PBR,
				"PBRMap", ShaderUsage::PBRMap
			);

			lua->new_enum("TextureTransparency",
				"Opaque", TextureTransparency::Opaque,
				"Transparent", TextureTransparency::Transparent
			);

			lua->new_enum("TextureFilter",
				"Nearest", TextureFilter::Nearest,
				"Linear", TextureFilter::Linear,
				"LinearMipmapLinear", TextureFilter::LinearMipmapLinear,
				"LinearMipmapNearest", TextureFilter::LinearMipmapNearest,
				"NearestMipmapNearest", TextureFilter::NearestMipmapNearest,
				"NearestMipmapLinear", TextureFilter::NearestMipmapLinear
			);

			lua->new_enum("TextureRepeat",
				"Clapm", TextureRepeat::Clamp,
				"ClampToBorder", TextureRepeat::ClampToBorder,
				"ClampToEdge", TextureRepeat::ClampToEdge,
				"Repeat", TextureRepeat::Repeat
			);

			lua->new_enum("TextureDataType",
				"RGBA", TextureDataType::RGBA,
				"BGR", TextureDataType::BGR,
				"BGRA", TextureDataType::BGRA,
				"DepthComponent", TextureDataType::DepthComponent,
				"DepthComponent16", TextureDataType::DepthComponent16,
				"DepthComponent24", TextureDataType::DepthComponent24,
				"DepthComponent32", TextureDataType::DepthComponent32,
				"R", TextureDataType::R8,
				"R16F", TextureDataType::R16F,
				"R32F", TextureDataType::R32F,
				"R16I", TextureDataType::R16I,
				"R32I", TextureDataType::R32I,
				"RG", TextureDataType::RG8,
				"RG16F", TextureDataType::RG16F,
				"RG32F", TextureDataType::RG32F,
				"RG16I", TextureDataType::RG16I,
				"RG32I", TextureDataType::RG32I,
				"RGB", TextureDataType::RGB8,
				"RGB16F", TextureDataType::RGB16F,
				"RGB32F", TextureDataType::RGB32F,
				"RGB16I", TextureDataType::RGB16I,
				"RGB32I", TextureDataType::RGB32I,
				"RGBA16F", TextureDataType::RGBA16F,
				"RGBA32F", TextureDataType::RGBA32F,
				"RGBA16I", TextureDataType::RGBA16I,
				"RGBA32I", TextureDataType::RGBA32I,
				"LUMINANCE", TextureDataType::LUMINANCE,
				"LUMINANCE_ALPHA", TextureDataType::LUMINANCE_ALPHA
			);

			lua->new_enum("TextureType",
				"CubemapPositive_X", TextureType::CubemapPositive_X,
				"CubemapNegative_X", TextureType::CubemapNegative_X,
				"CubemapPositive_Y", TextureType::CubemapPositive_Y,
				"CubemapNegative_Y", TextureType::CubemapNegative_Y,
				"CubemapPositive_Z", TextureType::CubemapPositive_Z,
				"CubemapNegative_Z", TextureType::CubemapNegative_Z,
				"Texture_Multisample", TextureType::Texture_Multisample,
				"Texture", TextureType::Texture
			);

			lua->new_enum("FrameBufferAttachmentFormat",
				"Color_Attachment0", FrameBufferAttachmentFormat::Color_Attachment0,
				"Color_Attachment1", FrameBufferAttachmentFormat::Color_Attachment1,
				"Color_Attachment2", FrameBufferAttachmentFormat::Color_Attachment2,
				"Color_Attachment3", FrameBufferAttachmentFormat::Color_Attachment3,
				"Color_Attachment4", FrameBufferAttachmentFormat::Color_Attachment4,
				"Color_Attachment5", FrameBufferAttachmentFormat::Color_Attachment5,
				"Color_Attachment6", FrameBufferAttachmentFormat::Color_Attachment6,
				"Color_Attachment7", FrameBufferAttachmentFormat::Color_Attachment7,
				"Color_Attachment8", FrameBufferAttachmentFormat::Color_Attachment8,
				"Color_Attachment9", FrameBufferAttachmentFormat::Color_Attachment9,
				"Color_Attachment10", FrameBufferAttachmentFormat::Color_Attachment10,
				"Color_Attachment11", FrameBufferAttachmentFormat::Color_Attachment11,
				"Color_Attachment12", FrameBufferAttachmentFormat::Color_Attachment12,
				"Color_Attachment13", FrameBufferAttachmentFormat::Color_Attachment13,
				"Color_Attachment14", FrameBufferAttachmentFormat::Color_Attachment14,
				"Color_Attachment15", FrameBufferAttachmentFormat::Color_Attachment15,
				"Depth_Attachment", FrameBufferAttachmentFormat::Depth_Attachment,
				"Stencil_Attachment", FrameBufferAttachmentFormat::Stencil_Attachment
			);

			lua->new_enum("RenderBufferDataType",
				"RGBA", RenderBufferDataType::RGBA,
				"Depth", RenderBufferDataType::Depth,
				"Stencil", RenderBufferDataType::Stencil,
				"RGBA_Multisample", RenderBufferDataType::RGBA_Multisample,
				"Depth_Multisample", RenderBufferDataType::Depth_Multisample,
				"Stencil_Multisample", RenderBufferDataType::Stencil_Multisample
			);

			lua->new_enum("FBOAttachmentType",
				"Texture", FBOAttachmentType::Texture,
				"RenderBuffer", FBOAttachmentType::RenderBuffer
			);

			lua->new_enum("FBOAccess",
				"Read_Write", FBOAccess::Read_Write,
				"Read", FBOAccess::Read,
				"Write", FBOAccess::Write
			);

			lua->new_enum("FBOBufferBit",
				"Color", FBOBufferBit::Color,
				"Depth", FBOBufferBit::Depth,
				"Stencil", FBOBufferBit::Stencil
			);

			// Distinct from FBOBufferBit above despite the similar name:
			// these are real OR-able mask bits (0x10/0x20/0x40), whereas
			// FBOBufferBit is a 0/1/2 index used to pick an attachment.
			// clearBufferBit()/renderScene()'s BufferOptions take THESE -
			// previously unreachable from Lua, so any script calling
			// clearBufferBit(FBOBufferBit.Color) was passing 0 (None).
			lua->new_enum("BufferBit",
				"None", Buffer_Bit::None,
				"Color", Buffer_Bit::Color,
				"Depth", Buffer_Bit::Depth,
				"Stencil", Buffer_Bit::Stencil
			);

			lua->new_enum("FBOFilter",
				"Linear", FBOFilter::Linear,
				"Nearest", FBOFilter::Nearest
			);

			// Drawing Type
			lua->new_enum("DrawingType",
				"Triangles", DrawingType::Triangles,
				"Lines", DrawingType::Lines,
				"Line_Loop", DrawingType::Line_Loop,
				"Line_Strip", DrawingType::Line_Strip,
				"Triangle_Fan", DrawingType::Triangle_Fan,
				"Triangle_Strip", DrawingType::Triangle_Strip,
				"Quads", DrawingType::Quads,
				"Points", DrawingType::Points,
				"Polygons", DrawingType::Polygons
			);
		}

		// ******************************* ENUMS *******************************

		// ******************************* CLASS *******************************
		{
			// VEC2
			sol::constructors<sol::types<>, sol::types<float, float>> con;
			lua->new_usertype<Math::Vec2>("Vec2",
				con,
				"x", &Math::Vec2::x,
				"y", &Math::Vec2::y,
				"dotProduct", &Math::Vec2::dotProduct,
				"magnitude", &Math::Vec2::magnitude,
				"magnitudeSQR", &Math::Vec2::magnitudeSQR,
				"distance", &Math::Vec2::distance,
				"distanceSQR", &Math::Vec2::distanceSQR,
				"normalize", &Math::Vec2::normalize,
				"negate", &Math::Vec2::negate,
				"abs", &Math::Vec2::Abs,
				"__add", sol::overload(
					&Vec2_operator_add,
					&Vec2_operator_addS
				),
				"__sub", sol::overload(
					&Vec2_operator_sub,
					&Vec2_operator_subS
				),
				"__mul", sol::overload(
					&Vec2_operator_mul,
					&Vec2_operator_mulS
				),
				"__div", sol::overload(
					&Vec2_operator_div,
					&Vec2_operator_divS
				),
				"__eq", &Math::Vec2::operator==,
				"__lt", &Math::Vec2::operator<,
				"__le", &Math::Vec2::operator<=
				);
		}

		{
			// VEC3
			sol::constructors<sol::types<>, sol::types<float, float, float>> con;
			lua->new_usertype<Math::Vec3>("Vec3",
				con,
				"x", &Math::Vec3::x,
				"y", &Math::Vec3::y,
				"z", &Math::Vec3::z,
				"dotProduct", &Math::Vec3::dotProduct,
				"magnitude", &Math::Vec3::magnitude,
				"magnitudeSQR", &Math::Vec3::magnitudeSQR,
				"distance", &Math::Vec3::distance,
				"distanceSQR", &Math::Vec3::distanceSQR,
				"normalize", &Math::Vec3::normalize,
				"normalizeSelft", &Math::Vec3::normalizeSelf,
				"negate", &Math::Vec3::negate,
				"negateSelf", &Math::Vec3::negateSelf,
				"abs", &Math::Vec3::Abs,
				"cross", &Math::Vec3::cross,
				"__add", sol::overload(
					&Vec3_operator_add,
					&Vec3_operator_addS
				),
				"__sub", sol::overload(
					&Vec3_operator_sub,
					&Vec3_operator_subS
				),
				"__mul", sol::overload(
					&Vec3_operator_mul,
					&Vec3_operator_mulS
				),
				"__div", sol::overload(
					&Vec3_operator_div,
					&Vec3_operator_divS
				),
				"__eq", &Math::Vec3::operator==,
				"__lt", &Math::Vec3::operator<,
				"__le", &Math::Vec3::operator<=
				);
		}

		{
			// VEC4
			sol::constructors<sol::types<>, sol::types<float, float, float, float>> con;
			lua->new_usertype<Math::Vec4>("Vec4",
				con,
				"x", &Math::Vec4::x,
				"y", &Math::Vec4::y,
				"z", &Math::Vec4::z,
				"w", &Math::Vec4::w,
				"dotProduct", &Math::Vec4::dotProduct,
				"magnitude", &Math::Vec4::magnitude,
				"magnitudeSQR", &Math::Vec4::magnitudeSQR,
				"abs", &Math::Vec4::Abs,
				"__add", sol::overload(
					&Vec4_operator_add,
					&Vec4_operator_addS
				),
				"__sub", sol::overload(
					&Vec4_operator_sub,
					&Vec4_operator_subS
				),
				"__mul", sol::overload(
					&Vec4_operator_mul,
					&Vec4_operator_mulS
				),
				"__div", sol::overload(
					&Vec4_operator_div,
					&Vec4_operator_divS
				),
				"__eq", &Math::Vec4::operator==,
				"__lt", &Math::Vec4::operator<,
				"__le", &Math::Vec4::operator<=
				);
		}

		{
			// Quaternion
			sol::constructors<sol::types<>, sol::types<float, float, float>, sol::types<float, float, float, float>, sol::types<Vec3, float>> con;
			lua->new_usertype<Math::Quaternion>("Quaternion",
				con,
				"x", &Math::Quaternion::x,
				"y", &Math::Quaternion::y,
				"z", &Math::Quaternion::z,
				"w", &Math::Quaternion::w,
				"convertToMatrix", &Math::Quaternion::ConvertToMatrix,
				"magnitude", &Math::Quaternion::Magnitude,
				"dot", &Math::Quaternion::Dot,
				"abs", &Math::Quaternion::Normalize,
				"rotation", &Math::Quaternion::Rotation,
				"setRotationFromEuler", &Math::Quaternion::SetRotationFromEuler,
				"getEulerRotation", &Math::Quaternion::GetEulerFromQuaternion,
				"axisToQuaternion", &Math::Quaternion::AxisToQuaternion,
				"slerp", &Math::Quaternion::Slerp,
				"nlerp", &Math::Quaternion::Nlerp,
				"inverse", &Math::Quaternion::Inverse,
				"__add", &Quaternion::operator+,
				"__sub", &Quaternion_operator_negate,
				"__mul", sol:: overload(
					&Quaternion_operator_mul,
					&Quaternion_operator_mulS,
					&Quaternion_operator_mulVec3
				)
				);
		}

		{
			// Matrix
			sol::constructors<sol::types<>, sol::types<float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float>> con;
			lua->new_usertype<Math::Matrix>("Matrix",
				con,
				"identity", &Math::Matrix::identity,
				"lookAt", sol::overload(&Matrix_lookAt, &Matrix_lookAt2),
				"translate", sol::overload(&Matrix_translateXYZ, &Matrix_translateVec3),
				"translateX", &Math::Matrix::TranslateX,
				"translateY", &Math::Matrix::TranslateY,
				"translateZ", &Math::Matrix::TranslateZ,
				"getTranslation", &Math::Matrix::GetTranslation,
				"rotationX", &Math::Matrix::RotationX,
				"rotationY", &Math::Matrix::RotationY,
				"rotationZ", &Math::Matrix::RotationZ,
				"setRotationFromEuler", &Math::Matrix::SetRotationFromEuler,
				"getEuler", &Math::Matrix::GetEulerFromRotationMatrix,
				"getRotation", &Math::Matrix::GetRotation,
				"scale", sol::overload(&Matrix_scaleXYZ, &Matrix_scaleVec3),
				"scaleX", &Math::Matrix::ScaleX,
				"scaleY", &Math::Matrix::ScaleY,
				"scaleZ", &Math::Matrix::ScaleZ,
				"getScale", &Math::Matrix::GetScale,
				"getDeterminant", &Math::Matrix::Determinant,
				"transpose", &Math::Matrix::Transpose,
				"inverse", &Math::Matrix::Inverse,
				"perspectiveMatrix", &Math::Matrix::PerspectiveMatrix,
				"orthoMatrix", &Math::Matrix::OrthoMatrix,
				"convertToQuaternion", &Math::Matrix::ConvertToQuaternion,
				"__mul", sol::overload(
					&Matrix_operator_mul,
					&Matrix_operator_mulS,
					&Matrix_operator_mulVec3,
					&Matrix_operator_mulVec4
				),
				"__eq", &Math::Matrix::operator==
				);
		}

		{
			// SceneGraph
			lua->new_usertype<SceneGraph>("Scene",
				"update", &SceneGraph::Update,
				"add", &SceneGraph::Add,
				"remove", &SceneGraph::Remove,
				"removeAll", &SceneGraph::RemoveAll,
				"addGameObject", &SceneGraph::AddGameObject,
				"removeGameobject", &SceneGraph::RemoveGameObject,
				"getTime", &SceneGraph::GetTime,
				"save", [lua](SceneGraph &scene, const std::string &path) {
					return SceneSerializer::SaveScene(&scene, path, lua);
				},
				"load", sol::overload(
					[lua](SceneGraph &scene, const std::string &path) {
						return SceneSerializer::LoadScene(&scene, path, NULL, lua);
					},
					[lua](SceneGraph &scene, const std::string &path, IPhysics* physics) {
						return SceneSerializer::LoadScene(&scene, path, physics, lua);
					}
				)
				);
		};

		{
			// GameObject
			sol::constructors<sol::types<>> con;
			lua->new_usertype<LUA_GameObject>("GameObject",
				con,
				"init", &LUA_GameObject::Init,
				"update", &LUA_GameObject::Update,
				"destroy", &LUA_GameObject::Destroy,
				"getLocalTransformation", &LUA_GameObject::GetLocalTransformation,
				"getPosition", &LUA_GameObject::GetPosition,
				"getRotation", &LUA_GameObject::GetRotation,
				"getScale", &LUA_GameObject::GetScale,
				"getDirection", &LUA_GameObject::GetDirection,
				"getWorldTransformation", &LUA_GameObject::GetWorldTransformation,
				"getWorldPosition", &LUA_GameObject::GetWorldPosition,
				"getWorldRotation", &LUA_GameObject::GetWorldRotation,
				"setPosition", &LUA_GameObject::SetPosition,
				"setRotation", &LUA_GameObject::SetRotation,
				"setScale", &LUA_GameObject::SetScale,
				"setTransformationMatrix", &LUA_GameObject::SetTransformationMatrix,
				"lookAtGameObject", &LUA_GameObject::LookAtGameObject,
				"lookAtVec", &LUA_GameObject::LookAtVec,
				"addComponent", &LUA_GameObject::AddComponent,
				"removeComponent", &LUA_GameObject::RemoveComponent,
				"addGameObject", &LUA_GameObject::AddGameObject,
				"removeGameObject", &LUA_GameObject::RemoveGameObject,
				"getParent", &LUA_GameObject::GetParent,
				"haveParent", &LUA_GameObject::HaveParent,
				"addTag", &LUA_GameObject::AddTag,
				"removeTag", &LUA_GameObject::RemoveTag,
				"haveTag", sol::overload(&GameObject_HaveTagSTR, &GameObject_HaveTagUINT),
				"isStatic", &LUA_GameObject::IsStatic,
				"onUpdate", &LUA_GameObject::on_update,
				"onInit", &LUA_GameObject::on_init,
				"onDestroy", &LUA_GameObject::on_destroy,
				// Convenience: load scriptFile (a .lua file returning a
				// middleclass class - see LuaComponent_FromFile's
				// comment), instantiate it, wrap it in a LuaComponent,
				// and attach it to this GameObject in one call - the
				// "drag a behavior script onto an object" pattern. Returns
				// the LuaComponent (nil if scriptFile didn't return a
				// usable class) so the caller can still reach `.data` if
				// needed.
				"attachScript", [lua](LUA_GameObject &go, const std::string &scriptFile) -> LuaComponent* {
					LuaComponent* comp = LuaComponent_FromFile(*lua, scriptFile);
					if (comp) go.AddComponent(comp);
					return comp;
				},
				"getComponent", &GameObject_GetComponent,
				sol::base_classes, sol::bases<GameObject>()
				);
		}

		{
			// GameObject (base) - registered separately from LUA_GameObject
			// above. sol2 selects push/index behavior from a pushed
			// value's STATIC C++ type, not its runtime type, so a plain
			// GameObject* (e.g. IComponent::GetOwner()'s declared return
			// type - the case for every attachScript()'d script's
			// self.owner) always resolved via this type even when the
			// actual object is a LUA_GameObject. With only LUA_GameObject
			// registered, GameObject* had no known metatable and sol2
			// fell back to an empty, method-less userdata wrapper -
			// "attempt to index a sol.p3d::GameObject * value" on every
			// self.owner:method() call. Mirrors the base-only subset of
			// LUA_GameObject's bindings (excludes the Lua-scripting-only
			// hooks onUpdate/onInit/onDestroy/attachScript, which have no
			// base-class equivalent - getComponent is included since it
			// only needs GameObject::GetComponents(), shared via
			// GameObject_GetComponent above).
			lua->new_usertype<GameObject>("GameObjectBase",
				"getLocalTransformation", &GameObject::GetLocalTransformation,
				"getPosition", &GameObject::GetPosition,
				"getRotation", &GameObject::GetRotation,
				"getScale", &GameObject::GetScale,
				"getDirection", &GameObject::GetDirection,
				"getWorldTransformation", &GameObject::GetWorldTransformation,
				"getWorldPosition", &GameObject::GetWorldPosition,
				"getWorldRotation", &GameObject::GetWorldRotation,
				"setPosition", &GameObject::SetPosition,
				"setRotation", &GameObject::SetRotation,
				"setScale", &GameObject::SetScale,
				"setTransformationMatrix", &GameObject::SetTransformationMatrix,
				"lookAtGameObject", &GameObject::LookAtGameObject,
				"lookAtVec", &GameObject::LookAtVec,
				"addComponent", &GameObject::AddComponent,
				"removeComponent", &GameObject::RemoveComponent,
				"addGameObject", &GameObject::AddGameObject,
				"removeGameObject", &GameObject::RemoveGameObject,
				"getParent", &GameObject::GetParent,
				"haveParent", &GameObject::HaveParent,
				"addTag", &GameObject::AddTag,
				"removeTag", &GameObject::RemoveTag,
				"haveTag", sol::overload(&GameObject_HaveTagSTR, &GameObject_HaveTagUINT),
				"isStatic", &GameObject::IsStatic,
				"getComponent", &GameObject_GetComponent
				);
		}

		{
			// Projection
			lua->new_usertype<Projection>("Projection",
				"perspective", &Projection::Perspective,
				"ortho", &Projection::Ortho,
				"getProjectionMatrix", &Projection::GetProjectionMatrix
				);
		};

		{
			// ForwardRenderer
			sol::constructors<sol::types<float, float>> con;
			lua->new_usertype<ForwardRenderer>("ForwardRenderer",
				con,
				"clearBufferBit", &ForwardRenderer::ClearBufferBit,
				"enableClearDepthBuffer", &ForwardRenderer::EnableClearDepthBuffer,
				"disableClearDepthBuffer", &ForwardRenderer::DisableClearDepthBuffer,
				"clearDepthBuffer", &ForwardRenderer::ClearDepthBuffer,
				"enableClipPlane", &ForwardRenderer::EnableClipPlane,
				"disableClipPlane", &ForwardRenderer::DisableClipPlane,
				"setClipaPlane0", &ForwardRenderer::SetClipPlane0,
				"setClipaPlane1", &ForwardRenderer::SetClipPlane1,
				"setClipaPlane2", &ForwardRenderer::SetClipPlane2,
				"setClipaPlane3", &ForwardRenderer::SetClipPlane3,
				"setClipaPlane4", &ForwardRenderer::SetClipPlane4,
				"setClipaPlane5", &ForwardRenderer::SetClipPlane5,
				"setClipaPlane6", &ForwardRenderer::SetClipPlane6,
				"setClipaPlane7", &ForwardRenderer::SetClipPlane7,
				"enableStencil", &ForwardRenderer::EnableStencil,
				"disableStencil", &ForwardRenderer::DisableStencil,
				"clearStencilBuffer", &ForwardRenderer::ClearStencilBuffer,
				"stencilFunction", &ForwardRenderer::StencilFunction,
				"stencilOperation", &ForwardRenderer::StencilOperation,
				"enableScissorTest", &ForwardRenderer::EnableScissorTest,
				"disableScissorTest", &ForwardRenderer::DisableScissorTest,
				"scissorTestRect", &ForwardRenderer::ScissorTestRect,
				"enableWireFrame", &ForwardRenderer::EnableWireFrame,
				"disableWireFrame", &ForwardRenderer::DisableWireFrame,
				"colorMask", &ForwardRenderer::ColorMask,
				"enableSorting", &ForwardRenderer::EnableSorting,
				"disableSorting", &ForwardRenderer::DisableSorting,
				"enableLOD", &ForwardRenderer::EnableLOD,
				"disableLOD", &ForwardRenderer::DisableLOD,
				"isUsingLOD", &ForwardRenderer::IsUsingLOD,
				"setBackground", &ForwardRenderer::SetBackground,
				"unsetBackground", &ForwardRenderer::UnsetBackground,
				"setGlobalLight", &ForwardRenderer::SetGlobalLight,
				"enableDepthBias", &ForwardRenderer::EnableDepthBias,
				"disableDepthBias", &ForwardRenderer::DisableDepthBias,
				"setViewPort", &ForwardRenderer::SetViewPort,
				"resetViewPort", &ForwardRenderer::ResetViewPort,
				"resize", &ForwardRenderer::Resize,
				"activateCulling", &ForwardRenderer::ActivateCulling,
				"deactivateCulling", &ForwardRenderer::DeactivateCulling,
				"renderScene", &ForwardRenderer::RenderScene,
				"preRender", sol::overload(&ForwardRenderer_PreRender, &ForwardRenderer_PreRenderTag)
				);
		}

		{
			// DeferredRenderer
			sol::constructors<sol::types<float, float, FrameBuffer*>> con;
			lua->new_usertype<DeferredRenderer>("DeferredRenderer",
				con,
				"clearBufferBit", &DeferredRenderer::ClearBufferBit,
				"enableClearDepthBuffer", &DeferredRenderer::EnableClearDepthBuffer,
				"disableClearDepthBuffer", &DeferredRenderer::DisableClearDepthBuffer,
				"clearDepthBuffer", &DeferredRenderer::ClearDepthBuffer,
				"enableClipPlane", &DeferredRenderer::EnableClipPlane,
				"disableClipPlane", &DeferredRenderer::DisableClipPlane,
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
			// VelocityRenderer
			sol::constructors<sol::types<float, float>> con;
			lua->new_usertype<VelocityRenderer>("VelocityRenderer",
				con,
				"renderVelocityMap", &VelocityRenderer::RenderVelocityMap
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

					&FrameBuffer_Init,
					&FrameBuffer_InitRenderBuffer
				),
				"addAttach", sol::overload(
					&FrameBuffer_AddAttach,
					&FrameBuffer_AddAttachRenderBuffer
				),
				"resize", &FrameBuffer::Resize,
				"bind", &FrameBuffer::Bind,
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

		{
			// LUA RenderingComponent
			sol::constructors<sol::types<Renderable*, IMaterial*, float>, sol::types<Renderable*, IMaterial*>, sol::types<Renderable*, int, float>, sol::types<Renderable*, int>> con;
			lua->new_usertype<LUA_RenderingComponent>("RenderingComponent",
				con,
				"addLOD", sol::overload(
					&RenderingComponent_ADDLOD_MaterialPointer, 
					& RenderingComponent_ADDLOD_MaterialOptions
				),
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
				"getMeshes", &LUA_RenderingComponent::GetMeshes,
				"getLODSize", &LUA_RenderingComponent::GetLODSize,
				"getLODByDistance", &LUA_RenderingComponent::GetLODByDistance,
				"updateLOD", &LUA_RenderingComponent::UpdateLOD,
				"getComponents", &LUA_RenderingComponent::GetComponents,
				"getActiveSkeletonAnimation", &RenderingComponent_GetActiveSkeletonAnimation,
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
            // LUA RenderingInstancedComponent
            sol::constructors<sol::types<Renderable*, IMaterial*, int, float>, sol::types<Renderable*, int, int, float>> con;
            lua->new_usertype<LUA_RenderingInstancedComponent>("RenderingInstancedComponent",
                con,
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
                "onUpdate", &LUA_RenderingInstancedComponent::on_update,
                "onInit", &LUA_RenderingInstancedComponent::on_init,
                "onDestroy", &LUA_RenderingInstancedComponent::on_destroy,
                sol::base_classes, sol::bases<IComponent>()
                );
        }

		{
			sol::constructors<sol::types<int>, sol::types<>> con;
			lua->new_usertype<RenderingMesh>("RenderingMesh",
				con,
				"getDrawingType", &RenderingMesh::GetDrawingType,
				"geometry", &RenderingMesh::Geometry,
				"material", &RenderingMesh::Material,
				"drawingType", &RenderingMesh::drawingType,
				"renderingComponent", &RenderingMesh::renderingComponent,
				"cullingGeometry", &RenderingMesh::CullingGeometry,
				"active", &RenderingMesh::Active,
				"clickable", &RenderingMesh::Clickable
				);
		}

		lua->new_usertype<IComponent>("IComponent",
			"getOwner", &IComponent::GetOwner
			);
		lua->new_usertype<Renderable>("Renderable");
		lua->new_usertype<ILightComponent>("IlightComponent",
			sol::base_classes, sol::bases<IComponent>()
			);
		// Real read/write physics API - was previously registered with
		// zero bound methods (only IPhysics::CreateBox/CreateSphere/...,
		// which return this type, were bound), meaning physics bodies
		// were write-only from Lua: creatable but unqueryable and
		// undrivable by force/impulse. OnCollisionEnter/OnCollisionExit
		// are plain fields here (like onUpdate/onInit elsewhere) - sol2
		// auto-converts an assigned Lua closure to std::function, same
		// proven pattern, no extra binding plumbing needed.
		lua->new_usertype<IPhysicsComponent>("IPhysicsComponent",
			"getMass", &IPhysicsComponent::GetMass,
			"getShape", &IPhysicsComponent::GetShape,
			"setPosition", &IPhysicsComponent::SetPosition,
			"setRotation", &IPhysicsComponent::SetRotation,
			"cleanForces", &IPhysicsComponent::CleanForces,
			"setAngularVelocity", &IPhysicsComponent::SetAngularVelocity,
			"setLinearVelocity", &IPhysicsComponent::SetLinearVelocity,
			"getLinearVelocity", &IPhysicsComponent::GetLinearVelocity,
			"getAngularVelocity", &IPhysicsComponent::GetAngularVelocity,
			"applyCentralForce", &IPhysicsComponent::ApplyCentralForce,
			"applyCentralImpulse", &IPhysicsComponent::ApplyCentralImpulse,
			"setMass", &IPhysicsComponent::SetMass,
			"activate", &IPhysicsComponent::Activate,
			"isGhost", &IPhysicsComponent::IsGhost,
			"onCollisionEnter", &IPhysicsComponent::OnCollisionEnter,
			"onCollisionExit", &IPhysicsComponent::OnCollisionExit,
			sol::base_classes, sol::bases<IComponent>()
			);

		{
			// RayCastHit / RayCast - real raycasting from Lua, e.g. for
			// click-picking or ground checks. hasHit gates whether the
			// rest of the fields are meaningful (mirrors the real C++
			// struct exactly - no Lua-side reinterpretation).
			sol::constructors<sol::types<>> con;
			lua->new_usertype<RayCastHit>("RayCastHit",
				con,
				"hasHit", &RayCastHit::hasHit,
				"point", &RayCastHit::point,
				"normal", &RayCastHit::normal,
				"distance", &RayCastHit::distance,
				"component", &RayCastHit::component
				);
		}

		{
			// Real, generic Lua-side "attach arbitrary behavior to any
			// GameObject" component - see PyrosBindings.h's LuaComponent
			// class comment. Attach via the already-bound
			// GameObject::addComponent(), same as any other component.
			sol::constructors<sol::types<>> con;
			lua->new_usertype<LuaComponent>("LuaComponent",
				con,
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
			// Directional Light
			sol::constructors<sol::types<>, sol::types<Vec4>, sol::types<Vec4, Vec3>> con;
			lua->new_usertype<LUA_DirectionalLight>("DirectionalLight",
				con,
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
			// Point Light
			sol::constructors<sol::types<>, sol::types<Vec4, float>> con;
			lua->new_usertype<LUA_PointLight>("PointLight",
				con,
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
			// to their real names.
			sol::constructors<sol::types<>, sol::types<Vec4, float, Vec3, float, float>> con;
			lua->new_usertype<LUA_SpotLight>("SpotLight",
				con,
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
			// Texture
			sol::constructors<sol::types<>> con;
			lua->new_usertype<Texture>("Texture",
				con,
				"loadTexture", &Texture::LoadTexture,
				"loadTextureFromMemory", &Texture::LoadTextureFromMemory,
				"createEmptyTexture", &Texture::CreateEmptyTexture,
				"setMinMagFilter", &Texture::SetMinMagFilter,
				"setRepeat", &Texture::SetRepeat,
				"enableCompareMode", &Texture::EnableCompareMode,
				"setTransparency", &Texture::SetTransparency,
				"resize", &Texture::Resize,
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
			// IMaterial
			sol::constructors<sol::types<>> con;
			lua->new_usertype<IMaterial>("IMaterial",
				con,
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
			// GenericShaderMaterial
			sol::constructors<sol::types<int>> con;
			lua->new_usertype<GenericShaderMaterial>("GenericShaderMaterial",
				con,
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
				"unbindTextures", &GenericShaderMaterial::UnbindTextures,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}

		{
			// CustomShaderMaterial
			sol::constructors<sol::types<std::string>, sol::types<Shader*>> con;
			lua->new_usertype<CustomShaderMaterial>("CustomShaderMaterial",
				con,
				"setShader", &CustomShaderMaterial::SetShader,
				sol::base_classes, sol::bases<IMaterial>()
				);
		}

		{
			// Cube
			sol::constructors<sol::types<float, float, float, bool, bool, bool>, sol::types<float, float, float, bool, bool>, sol::types<float, float, float, bool>, sol::types<float, float, float>> con;
			lua->new_usertype<Cube>("Cube",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Capsule
			sol::constructors<sol::types<float, float, float, int, int, bool, bool, bool>, sol::types<float, float, float, int, int, bool, bool>, sol::types<float, float, float, int, int, bool>, sol::types<float, float, float, int, int>> con;
			lua->new_usertype<Capsule>("Capsule",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Cone
			sol::constructors<sol::types<float, float, int, int, bool, bool, bool, bool>, sol::types<float, float, int, int, bool, bool, bool>, sol::types<float, float, int, int, bool, bool>, sol::types<float, float, int, int, bool>> con;
			lua->new_usertype<Cone>("Cone",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Cylinder
			sol::constructors<sol::types<float, float, int, int, bool, bool, bool, bool>, sol::types<float, float, int, int, bool, bool, bool>, sol::types<float, float, int, int, bool, bool>, sol::types<float, float, int, int, bool>> con;
			lua->new_usertype<Cylinder>("Cylinder",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Plane
			sol::constructors<sol::types<float, float, bool, bool, bool>, sol::types<float, float, bool, bool>, sol::types<float, float, bool>, sol::types<float, float>> con;
			lua->new_usertype<Plane>("Plane",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Sphere
			sol::constructors<sol::types<float, int, int, bool, bool, bool, bool>, sol::types<float, int, int, bool, bool, bool>, sol::types<float, int, int, bool, bool>, sol::types<float, int, int, bool>, sol::types<float, int, int>> con;
			lua->new_usertype<Sphere>("Sphere",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Torus
			sol::constructors<sol::types<float, float, int, int, bool, bool, bool>, sol::types<float, float, int, int, bool, bool>, sol::types<float, float, int, int, bool>, sol::types<float, float, int, int>, sol::types<float, float, int>, sol::types<float, float>> con;
			lua->new_usertype<Torus>("Torus",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// TorusKnot
			sol::constructors<sol::types<float, float, int, int, float, float, int, bool, bool, bool>, sol::types<float, float, int, int, float, float, int, bool, bool>, sol::types<float, float, int, int, float, float, int, bool>, sol::types<float, float, int, int, float, float, int>, sol::types<float, float, int, int, float, float>, sol::types<float, float, int, int, float>, sol::types<float, float, int, int>, sol::types<float, float, int>, sol::types<float, float>> con;
			lua->new_usertype<TorusKnot>("TorusKnot",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Model
			sol::constructors<sol::types<std::string>, sol::types<std::string, bool>,sol::types<std::string, bool>> con;
			lua->new_usertype<Model>("Model",
				con,
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Decals
			sol::constructors<sol::types < std::vector <DecalVertex>, bool>, sol::types<std::vector < DecalVertex > > > con;
			lua->new_usertype<Decal>("Decal",
				con,
				// Renderable listed explicitly alongside Model: sol does not
				// walk a transitive base chain, so listing only Model left
				// Decal unusable everywhere a Renderable* is expected -
				// including RenderingComponent's own constructor.
				sol::base_classes, sol::bases<Model, Renderable>()
				);
		}

		{
			// Font
			sol::constructors<sol::types<std::string, float>> con;
			lua->new_usertype<Font>("Font",
				con,
				"createText", &Font::CreateText,
				"getTexture", &Font::GetTexture,
				"getFontSize", &Font::GetFontSize,
				"getGlyphs", &Font::GetGlyphs
				);
		}

		{
			// Text
			sol::constructors<sol::types<Font*, std::string, float, float, const Vec4&, bool>> con;
			lua->new_usertype<Text>("Text",
				con,
				"updateText", sol::overload(
					&Text_UpdateText,
					&Text_UpdateTextColors),
				// Text IS a Renderable, but sol only performs the
				// derived->base pointer conversion for bases listed here -
				// without this, RenderingComponent.new(text, material)
				// failed with "no matching function call takes this number
				// of arguments and the specified types", which made it
				// impossible to draw text at all from Lua. Same class of
				// bug as the SkeletonAnimation one recorded on
				// LUA_RenderingComponent above.
				sol::base_classes, sol::bases<Renderable>()
				);
		}

		{
			// Skeleton Animation
			sol::constructors<sol::types<>> con;
			lua->new_usertype<SkeletonAnimation>("SekeletonAnimation",
				con,
				"loadAnimation", &SkeletonAnimation::LoadAnimation,
				"getNumberAnimatons", &SkeletonAnimation::GetNumberAnimations,
				"update", &SkeletonAnimation::Update,
				"createInstance", &SkeletonAnimation::CreateInstance,
				"destroyInstance", &SkeletonAnimation::DestroyInstance,
				"getAnimationIDByName", &SkeletonAnimation::GetAnimationIDByName
				);
		}

		{
			// Skeleton Animation Instance
			sol::constructors<sol::types<SkeletonAnimation*, RenderingComponent*>> con;
			lua->new_usertype<SkeletonAnimationInstance>("SekeletonAnimationInstance",
				con,
				"getOwner", &SkeletonAnimationInstance::GetOwner,
				"play", &SkeletonAnimationInstance::Play,
				"changeProperties", &SkeletonAnimationInstance::ChangeProperties,
				"pause", &SkeletonAnimationInstance::Pause,
				"PauseAnimation", &SkeletonAnimationInstance::PauseAnimation,
				"resumeAnimation", &SkeletonAnimationInstance::ResumeAnimation,
				"resume", &SkeletonAnimationInstance::Resume,
				"stopAnimation", &SkeletonAnimationInstance::StopAnimation,
				"stop", &SkeletonAnimationInstance::Stop,
				"getAnimationCurrentProgress", &SkeletonAnimationInstance::GetAnimationCurrentProgress,
				"getAnimationDuration", &SkeletonAnimationInstance::GetAnimationDuration,
				"getAnimationCurrentTime", &SkeletonAnimationInstance::GetAnimationCurrentTime,
				"getAniamtionSpeed", &SkeletonAnimationInstance::GetAnimationSpeed,
				"getAnimationStartTime", &SkeletonAnimationInstance::GetAnimationStartTime,
				"getAnimationID", &SkeletonAnimationInstance::GetAnimationID,
				"getAnimationScale", &SkeletonAnimationInstance::GetAnimationScale,
				"createLayer", &SkeletonAnimationInstance::CreateLayer,
				"addBone", sol::overload(
					&SkeletonAnimationInstance_AddBone,
					&SkeletonAnimationInstance_AddBoneSTR
				),
				"addBoneAndChilds", sol::overload(
					&SkeletonAnimationInstance_AddBoneAndChilds,
					&SkeletonAnimationInstance_AddBoneAndChildsSTR
				),
				"removeBone", sol::overload(
					&SkeletonAnimationInstance_RemoveBone,
					&SkeletonAnimationInstance_RemoveBoneSTR
				),
				"removeBoneAndChilds", sol::overload(
					&SkeletonAnimationInstance_RemoveBoneAndChilds,
					&SkeletonAnimationInstance_RemoveBoneAndChildsSTR
				),
				"destroyLayer", sol::overload(
					&SkeletonAnimationInstance_DestroyLayer,
					&SkeletonAnimationInstance_DestroyLayerSTR
				),
				"isPaused", sol::overload(
					&SkeletonAnimationInstance_IsPaused,
					&SkeletonAnimationInstance_IsPausedID
				)
				);
		}

		{
			// Texture Animation
			sol::constructors<sol::types<>> con;
			lua->new_usertype<TextureAnimation>("TextureAnimation",
				con,
				"getFrame", &TextureAnimation::GetFrame,
				"getNumberFrames", &TextureAnimation::GetNumberFrames,
				"addFrame", &TextureAnimation::AddFrame,
				"update", &TextureAnimation::Update,
				"createInstance", &TextureAnimation::CreateInstance,
				"destroyInstance", &TextureAnimation::DestroyInstance
				);
		}

		{
			// Texture Animation Instance
			sol::constructors<sol::types<TextureAnimation*, float>> con;
			lua->new_usertype<TextureAnimationInstance>("TextureAnimationInstance",
				con,
				"play", &TextureAnimationInstance::Play,
				"pause", &TextureAnimationInstance::Pause,
				"stop", &TextureAnimationInstance::Stop,
				"isPlaying", &TextureAnimationInstance::IsPlaying,
				"yoyo", &TextureAnimationInstance::YoYo,
				"getTexture", &TextureAnimationInstance::GetTexture,
				"getFrame", &TextureAnimationInstance::GetFrame
				);
		}

		{
			// IPhysics
			lua->new_usertype<IPhysics>("IPhysics",
				"initPhysics", &IPhysics::InitPhysics,
				"enableDebugDraw", &IPhysics::EnableDebugDraw,
				"renderDebugDraw", &IPhysics::RenderDebugDraw,
				"disableDebugDraw", &IPhysics::DisableDebugDraw,
				"update", &IPhysics::Update,
				"endPhysics", &IPhysics::EndPhysics,
				"RemovePhysicsComponent", &IPhysics::RemovePhysicsComponent,
				"UpdateTransformations", &IPhysics::UpdateTransformations,
				"UpdatePosition", &IPhysics::UpdatePosition,
				"UpdateRotation", &IPhysics::UpdateRotation,
				"CleanForces", &IPhysics::CleanForces,
				"SetAngularVelocity", &IPhysics::SetAngularVelocity,
				"SetLinearVelocity", &IPhysics::SetLinearVelocity,
				"Activate", &IPhysics::Activate,
				"rayCast", &IPhysics::RayCast,
				"createBox", &IPhysics::CreateBox,
				"createCapsule", &IPhysics::CreateCapsule,
				"createCone", &IPhysics::CreateCone,
				"createConvexHull", &IPhysics::CreateConvexHull,
				"createCylinder", &IPhysics::CreateCylinder,
				"createMultiplerSphere", &IPhysics::CreateMultipleSphere,
				"createSphere", &IPhysics::CreateSphere,
				"createStaticPlane", &IPhysics::CreateStaticPlane,
				"createVehicle", &IPhysics::CreateVehicle,
				"addWheel", &IPhysics::AddWheel,
				"createTriangleMesh", sol::overload(
					&IPhysics_CreateTriangleMesh,
					&IPhysics_CreateTriangleMeshRCOMP
				),
				"createConvexTriangleMesh", sol::overload(
					&IPhysics_CreateConvexTriangleMesh,
					&IPhysics_CreateConvexTriangleMeshRCOMP
				)
				);
		}

		{
			// Bullet Physics
			sol::constructors<sol::types<>> con;
			lua->new_usertype<BulletPhysics>("BulletPhysics",
				con,
				sol::base_classes, sol::bases<IPhysics>()
				);
		}

		{
			// Post Effects Manager
			sol::constructors<sol::types<int, int>> con;
			lua->new_usertype<PostEffectsManager>("PostEffectsManager",
				con,
				"resize", &PostEffectsManager::Resize,
				"captureFrame", &PostEffectsManager::CaptureFrame,
				"endCapture", &PostEffectsManager::EndCapture,
				"processPostEffects", &PostEffectsManager::ProcessPostEffects,
				"addEffect", &PostEffectsManager::AddEffect,
				"removeEffect", &PostEffectsManager::RemoveEffect,
				"getNumberEffects", &PostEffectsManager::GetNumberEffects,
				"getExternalFrameBuffer", &PostEffectsManager::GetExternalFrameBuffer,
				"getColor", &PostEffectsManager::GetColor,
				"getDepth", &PostEffectsManager::GetDepth,
				"getLastRTT", &PostEffectsManager::GetLastRTT
				);
		}
		{
			// IEffect
			sol::constructors<sol::types<int, int>> con;
			lua->new_usertype<IEffect>("IEffect",
				con,
				"compileShaders", &IEffect::CompileShaders,
				"resize", &IEffect::Resize,
				"getWidth", &IEffect::GetWidth,
				"getHeight", &IEffect::GetHeight,
				"getTexture", &IEffect::GetTexture
				);
		}
		{
			// Bloom
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BloomEffect>("BloomEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// BlurXEffect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BlurXEffect>("BlurXEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// BlurYEffect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<BlurYEffect>("BlurYEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// Resize Effect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<ResizeEffect>("ResizeEffect",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// RTT Debug
			sol::constructors<sol::types<int, int, int, int>> con;
			lua->new_usertype<RTTDebug>("RTTDebug",
				con,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// SSAO Effect
			sol::constructors<sol::types<int, int, int>> con;
			lua->new_usertype<SSAOEffect>("SSAOEffect",
				con,
				"setViewMatrix", &SSAOEffect::SetViewMatrix,
				"setRadius", &SSAOEffect::SetRadius,
				"setStrength", &SSAOEffect::SetStrength,
				"setScale", &SSAOEffect::SetScale,
				"setTreshOld", &SSAOEffect::SetTreshOld,
				sol::base_classes, sol::bases<IEffect>()
				);
		}
		{
			// VigneteEffec
			sol::constructors<sol::types<int, int, int, float, float>> con;
			lua->new_usertype<VignetteEffect>("VignetEffect",
				con,
				"setRadius", &VignetteEffect::SetRadius,
				"setSoftness", &VignetteEffect::SetSoftness,
				sol::base_classes, sol::bases<IEffect>()
				);
		}

		{
			// MotionBlur
			sol::constructors<sol::types<int, Texture*, int, int>> con;
			lua->new_usertype<MotionBlurEffect>("MotionBlur",
				con,
				"setTargetFPS", &MotionBlurEffect::SetTargetFPS,
				"setCurrentFPS", &MotionBlurEffect::SetCurrentFPS,
				sol::base_classes, sol::bases<IEffect>()
				);
		}

		{
			lua->new_enum("ParticleBlendMode",
				"AlphaBlend", ParticleBlendMode::AlphaBlend,
				"Additive", ParticleBlendMode::Additive
			);

			// ParticleSystemDesc - plain data, bound as a field-assignable
			// usertype (construct with ParticleSystemDesc.new(), set the
			// fields that matter, pass to ParticleSystem.new(desc)) rather
			// than a giant constructor overload - matches how the C++ side
			// itself is designed (every field has a sane default from
			// ParticleSystemDesc()).
			sol::constructors<sol::types<>> descCon;
			lua->new_usertype<ParticleSystemDesc>("ParticleSystemDesc",
				descCon,
				"maxParticles", &ParticleSystemDesc::maxParticles,
				"texture", &ParticleSystemDesc::texture,
				"looping", &ParticleSystemDesc::looping,
				"emissionRate", &ParticleSystemDesc::emissionRate,
				"burstCount", &ParticleSystemDesc::burstCount,
				"minLifetime", &ParticleSystemDesc::minLifetime,
				"maxLifetime", &ParticleSystemDesc::maxLifetime,
				"direction", &ParticleSystemDesc::direction,
				"spreadAngle", &ParticleSystemDesc::spreadAngle,
				"minSpeed", &ParticleSystemDesc::minSpeed,
				"maxSpeed", &ParticleSystemDesc::maxSpeed,
				"gravity", &ParticleSystemDesc::gravity,
				"damping", &ParticleSystemDesc::damping,
				"startSize", &ParticleSystemDesc::startSize,
				"endSize", &ParticleSystemDesc::endSize,
				"sizeRandomJitter", &ParticleSystemDesc::sizeRandomJitter,
				"startColor", &ParticleSystemDesc::startColor,
				"endColor", &ParticleSystemDesc::endColor,
				"fadeInFraction", &ParticleSystemDesc::fadeInFraction,
				"fadeOutFraction", &ParticleSystemDesc::fadeOutFraction,
				"minRotationSpeed", &ParticleSystemDesc::minRotationSpeed,
				"maxRotationSpeed", &ParticleSystemDesc::maxRotationSpeed,
				"blendMode", &ParticleSystemDesc::blendMode,
				"boundingSphereRadius", &ParticleSystemDesc::boundingSphereRadius
				);

			sol::constructors<sol::types<const ParticleSystemDesc&>> con;
			lua->new_usertype<ParticleSystem>("ParticleSystem",
				con,
				"update", &ParticleSystem::Update,
				"play", &ParticleSystem::Play,
				"stop", &ParticleSystem::Stop,
				"clear", &ParticleSystem::Clear,
				"getLiveParticleCount", &ParticleSystem::GetLiveParticleCount,
				"isPlaying", &ParticleSystem::IsPlaying,
				"setEmissionRate", &ParticleSystem::SetEmissionRate,
				"setBurstCount", &ParticleSystem::SetBurstCount,
				"setLifetime", &ParticleSystem::SetLifetime,
				"setDirection", &ParticleSystem::SetDirection,
				"setSpread", &ParticleSystem::SetSpread,
				"setSpeed", &ParticleSystem::SetSpeed,
				"setGravity", &ParticleSystem::SetGravity,
				"setDamping", &ParticleSystem::SetDamping,
				"setSizes", &ParticleSystem::SetSizes,
				"setColors", &ParticleSystem::SetColors,
				"setFade", &ParticleSystem::SetFade,
				"setRotationSpeed", &ParticleSystem::SetRotationSpeed,
				"setBlendMode", &ParticleSystem::SetBlendMode,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// ******************************* Audio *******************************

			lua->new_enum("AttenuationModel",
				"None", AttenuationModel::None,
				"Inverse", AttenuationModel::Inverse,
				"Linear", AttenuationModel::Linear,
				"Exponential", AttenuationModel::Exponential
			);

			// AudioManager - construct exactly one and keep it alive; see the
			// class comment for the active-manager registration this relies on.
			sol::constructors<sol::types<>> audioCon;
			lua->new_usertype<AudioManager>("AudioManager",
				audioCon,
				"isInitialized", &AudioManager::IsInitialized,
				"setMasterVolume", &AudioManager::SetMasterVolume,
				"getMasterVolume", &AudioManager::GetMasterVolume,
				"setListener", [](AudioManager &a, const Vec3 &position, const Vec3 &forward, const Vec3 &up) {
					a.SetListener(position, forward, up);
				},
				// The common case - point the listener at the camera object.
				// Call it after scene:update(), which is what refreshes the
				// world transform this reads.
				"setListenerFromGameObject", &AudioManager::SetListenerFromGameObject,
				"getListenerPosition", &AudioManager::GetListenerPosition
				);

			// Sound - pooled one-shot effects.
			sol::constructors<sol::types<std::string>, sol::types<std::string, uint32>> soundCon;
			lua->new_usertype<Sound>("Sound",
				soundCon,
				"isLoaded", &Sound::IsLoaded,
				"getFile", &Sound::GetFile,
				// Defaults spelled out as overloads: sol binds a function's
				// full arity, so C++ default arguments are not optional from
				// Lua (the same reason DeferredRenderer_RenderScene above
				// needs its 3-argument wrapper).
				"play", sol::overload(
					[](Sound &s) { s.Play(); },
					[](Sound &s, const f32 volume) { s.Play(volume); },
					[](Sound &s, const f32 volume, const f32 pitch) { s.Play(volume, pitch); }
				),
				"playAt", sol::overload(
					[](Sound &s, const Vec3 &position) { s.PlayAt(position); },
					[](Sound &s, const Vec3 &position, const f32 volume) { s.PlayAt(position, volume); },
					[](Sound &s, const Vec3 &position, const f32 volume, const f32 pitch) { s.PlayAt(position, volume, pitch); }
				),
				"stop", &Sound::Stop,
				"getPlayingCount", &Sound::GetPlayingCount,
				"setAttenuation", &Sound::SetAttenuation
				);

			// AudioSource - a positional emitter component.
			sol::constructors<sol::types<std::string>, sol::types<std::string, bool>> sourceCon;
			lua->new_usertype<AudioSource>("AudioSource",
				sourceCon,
				"isLoaded", &AudioSource::IsLoaded,
				"getFile", &AudioSource::GetFile,
				"play", &AudioSource::Play,
				"pause", &AudioSource::Pause,
				"stop", &AudioSource::Stop,
				"isPlaying", &AudioSource::IsPlaying,
				"setLooping", &AudioSource::SetLooping,
				"isLooping", &AudioSource::IsLooping,
				"setVolume", &AudioSource::SetVolume,
				"getVolume", &AudioSource::GetVolume,
				"setPitch", &AudioSource::SetPitch,
				"getPitch", &AudioSource::GetPitch,
				"fadeIn", &AudioSource::FadeIn,
				"fadeOut", &AudioSource::FadeOut,
				"setSpatialization", &AudioSource::SetSpatialization,
				"isSpatialized", &AudioSource::IsSpatialized,
				"setAttenuation", &AudioSource::SetAttenuation,
				"setCone", &AudioSource::SetCone,
				"clearCone", &AudioSource::ClearCone,
				"setDirectionalAttenuation", &AudioSource::SetDirectionalAttenuation,
				"setDopplerFactor", &AudioSource::SetDopplerFactor,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Input - real keyboard/mouse enums plus the LuaInputBridge
			// registration API (see PyrosBindings.h's LuaInputBridge
			// class comment for why a bridge object is needed instead of
			// binding InputManager directly).
			lua->new_enum("Key",
				"A", Event::Input::Keyboard::A, "B", Event::Input::Keyboard::B, "C", Event::Input::Keyboard::C,
				"D", Event::Input::Keyboard::D, "E", Event::Input::Keyboard::E, "F", Event::Input::Keyboard::F,
				"G", Event::Input::Keyboard::G, "H", Event::Input::Keyboard::H, "I", Event::Input::Keyboard::I,
				"J", Event::Input::Keyboard::J, "K", Event::Input::Keyboard::K, "L", Event::Input::Keyboard::L,
				"M", Event::Input::Keyboard::M, "N", Event::Input::Keyboard::N, "O", Event::Input::Keyboard::O,
				"P", Event::Input::Keyboard::P, "Q", Event::Input::Keyboard::Q, "R", Event::Input::Keyboard::R,
				"S", Event::Input::Keyboard::S, "T", Event::Input::Keyboard::T, "U", Event::Input::Keyboard::U,
				"V", Event::Input::Keyboard::V, "W", Event::Input::Keyboard::W, "X", Event::Input::Keyboard::X,
				"Y", Event::Input::Keyboard::Y, "Z", Event::Input::Keyboard::Z,
				"Num0", Event::Input::Keyboard::Num0, "Num1", Event::Input::Keyboard::Num1,
				"Num2", Event::Input::Keyboard::Num2, "Num3", Event::Input::Keyboard::Num3,
				"Num4", Event::Input::Keyboard::Num4, "Num5", Event::Input::Keyboard::Num5,
				"Num6", Event::Input::Keyboard::Num6, "Num7", Event::Input::Keyboard::Num7,
				"Num8", Event::Input::Keyboard::Num8, "Num9", Event::Input::Keyboard::Num9,
				"Escape", Event::Input::Keyboard::Escape,
				"LControl", Event::Input::Keyboard::LControl, "LShift", Event::Input::Keyboard::LShift,
				"LAlt", Event::Input::Keyboard::LAlt, "RControl", Event::Input::Keyboard::RControl,
				"RShift", Event::Input::Keyboard::RShift, "RAlt", Event::Input::Keyboard::RAlt,
				"Space", Event::Input::Keyboard::Space, "Return", Event::Input::Keyboard::Return,
				"Back", Event::Input::Keyboard::Back, "Tab", Event::Input::Keyboard::Tab,
				"Left", Event::Input::Keyboard::Left, "Right", Event::Input::Keyboard::Right,
				"Up", Event::Input::Keyboard::Up, "Down", Event::Input::Keyboard::Down,
				"F1", Event::Input::Keyboard::F1, "F2", Event::Input::Keyboard::F2,
				"F3", Event::Input::Keyboard::F3, "F4", Event::Input::Keyboard::F4,
				"F5", Event::Input::Keyboard::F5, "F6", Event::Input::Keyboard::F6,
				"F7", Event::Input::Keyboard::F7, "F8", Event::Input::Keyboard::F8,
				"F9", Event::Input::Keyboard::F9, "F10", Event::Input::Keyboard::F10,
				"F11", Event::Input::Keyboard::F11, "F12", Event::Input::Keyboard::F12
			);
			lua->new_enum("MouseButton",
				"Left", Event::Input::Mouse::Left,
				"Middle", Event::Input::Mouse::Middle,
				"Right", Event::Input::Mouse::Right
			);

			sol::constructors<sol::types<>> con;
			lua->new_usertype<LuaInputBridge>("Input",
				con,
				"onKeyPressed", &LuaInputBridge::OnKeyPressed,
				"onKeyReleased", &LuaInputBridge::OnKeyReleased,
				"onMouseButtonPressed", &LuaInputBridge::OnMouseButtonPressed,
				"onMouseButtonReleased", &LuaInputBridge::OnMouseButtonReleased,
				"onMouseMoved", &LuaInputBridge::OnMouseMoved,
				"onMouseWheelMoved", &LuaInputBridge::OnMouseWheelMoved
				);
		}

		{
			//File
			sol::constructors<sol::types<>> con;
			lua->new_usertype<File>("File",
				con,
				"open", &File::Open,
				"write", &File::Write,
				"read", &File::Read,
				"rewind", &File::Rewind,
				"close", &File::Close,
				"size", &File::Size,
				"getData", &File::GetData
				);
		}
		// ******************************* CLASS *******************************
}

};

#endif
