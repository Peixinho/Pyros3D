//============================================================================
// Name        : PyrosLuaEnums.cpp
// Description : Enums / constants.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/Rendering/Culling/Culling.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaEnums(sol::state* lua)
	{
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
				// The remaining ShaderLib.h flags (DeferredRenderer_Gbuffer: ignored, kept so
				// scripts that set it still run).
				"DeferredRenderer_Gbuffer", ShaderUsage::DeferredRenderer_Gbuffer,
				"ParallaxMapping", ShaderUsage::ParallaxMapping,
				"InstancedRendering", ShaderUsage::InstancedRendering,
				"VelocityRendering", ShaderUsage::VelocityRendering,
				"PBR", ShaderUsage::PBR,
				"PBRMap", ShaderUsage::PBRMap,
				"AlphaTest", ShaderUsage::AlphaTest,
            "InstancedColor", ShaderUsage::InstancedColor,
            "VertexWind", ShaderUsage::VertexWind,
            "VertexColor", ShaderUsage::VertexColor,
            // (past bit 31: Lua integers are 64-bit, and LuaObjectToMaterialOptions reads them whole)
            "GlobalIllumination", ShaderUsage::GlobalIllumination,
            "EmissionMap", ShaderUsage::EmissionMap
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

			lua->new_enum("BlendFunc",
				"Zero", BlendFunc::Zero,
				"One", BlendFunc::One,
				"Src_Alpha", BlendFunc::Src_Alpha,
				"One_Minus_Src_Alpha", BlendFunc::One_Minus_Src_Alpha,
				"Dst_Alpha", BlendFunc::Dst_Alpha,
				"One_Minus_Dst_Alpha", BlendFunc::One_Minus_Dst_Alpha
			);
			// (renderer:activateCulling's argument)
			lua->new_enum("CullingMode",
				"FrustumCulling", CullingMode::FrustumCulling
			);
			lua->new_enum("CullFace",
				"BackFace", CullFace::BackFace,
				"FrontFace", CullFace::FrontFace,
				"DoubleSided", CullFace::DoubleSided
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

	}

} // namespace p3d

#endif
