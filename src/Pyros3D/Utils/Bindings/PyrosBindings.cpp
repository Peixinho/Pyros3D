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

	// GameObject
	bool GameObject_HaveTagSTR(GameObject &g, const std::string &tag)
	{
		return g.HaveTag(tag);
	}
	bool GameObject_HaveTagUINT(GameObject &g, const uint32 tag)
	{
		return g.HaveTag(tag);
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
		lua->open_libraries(sol::lib::base, sol::lib::math);

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
				"ClipPlane", ShaderUsage::ClipPlane
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
				"addGameObject", &SceneGraph::AddGameObject,
				"removeGameobject", &SceneGraph::RemoveGameObject,
				"getTime", &SceneGraph::GetTime
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
				sol::base_classes, sol::bases<GameObject>()
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
				"renderScene", &DeferredRenderer::RenderScene,
				"preRender", sol::overload(&DeferredRenderer_PreRender, &DeferredRenderer_PreRenderTag)
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
				"onUpdate", &LUA_RenderingComponent::on_update,
				"onInit", &LUA_RenderingComponent::on_init,
				sol::base_classes, sol::bases<IComponent>()
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
                "onUpdate", &LUA_RenderingInstancedComponent::on_update,
                "onInit", &LUA_RenderingInstancedComponent::on_init,
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

		lua->new_usertype<IComponent>("IComponent");
		lua->new_usertype<Renderable>("Renderable");
		lua->new_usertype<ILightComponent>("IlightComponent",
			sol::base_classes, sol::bases<IComponent>()
			);
		lua->new_usertype<IPhysicsComponent>("IPhysicsComponent",
			sol::base_classes, sol::bases<IComponent>()
			);


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
				sol::base_classes, sol::bases<ILightComponent>(),
				"onUpdate", &LUA_DirectionalLight::on_update,
				"onInit", &LUA_DirectionalLight::on_init,
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
				"onUpdate", &LUA_PointLight::on_update,
				"onInit", &LUA_PointLight::on_init,
				"setShadowPCFTexelSize", &LUA_PointLight::SetShadowPCFTexelSize,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// Spot Light
			sol::constructors<sol::types<>, sol::types<Vec4, float, Vec3, float, float>> con;
			lua->new_usertype<LUA_SpotLight>("SpotLight",
				con,
				"start", &LUA_SpotLight::Start,
				"update", &LUA_SpotLight::Update,
				"destroy", &LUA_SpotLight::Destroy,
				"enableShadows", &LUA_SpotLight::EnableCastShadows,
				"getShadowFar", &LUA_SpotLight::GetShadowFar,
				"getLightProjection", &LUA_SpotLight::GetLightDirection,
				"setLightProjection", &LUA_SpotLight::SetLightDirection,
				"getLightRadius", &LUA_SpotLight::GetLightRadius,
				"setLightRadius", &LUA_SpotLight::SetLightRadius,
				"getLightInnerCone", &LUA_SpotLight::GetLightInnerCone,
				"setLightInnerCone", &LUA_SpotLight::SetLightInnerCone,
				"getLightOutterCone", &LUA_SpotLight::GetLightOutterCone,
				"setLightOutterCone", &LUA_SpotLight::SetLightOutterCone,
				"getLightColor", &LUA_SpotLight::GetLightColor,
				"setShadowPCFTexelSize", &LUA_SpotLight::SetShadowPCFTexelSize,
				sol::base_classes, sol::bases<IComponent>()
				);
		}

		{
			// LUA Spot Light
			sol::constructors<sol::types<>, sol::types<Vec4, float, Vec3, float, float>> con;
			lua->new_usertype<LUA_SpotLight>("SpotLight",
				con,
				"onUpdate", &LUA_SpotLight::on_update,
				"onInit", &LUA_SpotLight::on_init,
				sol::base_classes, sol::bases<SpotLight>()
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
				sol::base_classes, sol::bases<Model>()
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
					&Text_UpdateTextColors)
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
