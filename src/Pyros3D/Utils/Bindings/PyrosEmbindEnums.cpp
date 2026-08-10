//============================================================================
// Name        : PyrosEmbindEnums.cpp
// Description : Embind enum/constant parity with Lua PyrosBindings.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Materials/GenericShaderMaterials/ShaderLib.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Materials/Shaders/Uniforms.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Audio/AudioManager.h>
#include <Pyros3D/Core/InputManager/InputManager.h>
#include <Pyros3D/Rendering/Culling/Culling.h>

using namespace emscripten;
using namespace p3d;

namespace p3d {
	void PyrosEmbindEnumsForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_enums)
{
	// ShaderUsage
	constant("ShaderUsage_Color", (int)ShaderUsage::Color);
	constant("ShaderUsage_Texture", (int)ShaderUsage::Texture);
	constant("ShaderUsage_EnvMap", (int)ShaderUsage::EnvMap);
	constant("ShaderUsage_SkyBox", (int)ShaderUsage::Skybox);
	constant("ShaderUsage_Refraction", (int)ShaderUsage::Refraction);
	constant("ShaderUsage_Skinning", (int)ShaderUsage::Skinning);
	constant("ShaderUsage_CellShading", (int)ShaderUsage::CellShading);
	constant("ShaderUsage_BumpMapping", (int)ShaderUsage::BumpMapping);
	constant("ShaderUsage_SpecularMap", (int)ShaderUsage::SpecularMap);
	constant("ShaderUsage_SpecularColor", (int)ShaderUsage::SpecularColor);
	constant("ShaderUsage_DirectionalShadow", (int)ShaderUsage::DirectionalShadow);
	constant("ShaderUsage_PointShadow", (int)ShaderUsage::PointShadow);
	constant("ShaderUsage_SpotShadow", (int)ShaderUsage::SpotShadow);
	constant("ShaderUsage_CastShadows", (int)ShaderUsage::CastShadows);
	constant("ShaderUsage_Diffuse", (int)ShaderUsage::Diffuse);
	constant("ShaderUsage_TextRendering", (int)ShaderUsage::TextRendering);
	constant("ShaderUsage_DebugRendering", (int)ShaderUsage::DebugRendering);
	constant("ShaderUsage_ClipPlane", (int)ShaderUsage::ClipPlane);
	constant("ShaderUsage_DeferredRenderer_Gbuffer", (int)ShaderUsage::DeferredRenderer_Gbuffer);
	constant("ShaderUsage_ParallaxMapping", (int)ShaderUsage::ParallaxMapping);
	constant("ShaderUsage_InstancedRendering", (int)ShaderUsage::InstancedRendering);
	constant("ShaderUsage_VelocityRendering", (int)ShaderUsage::VelocityRendering);
	constant("ShaderUsage_PBR", (int)ShaderUsage::PBR);
	constant("ShaderUsage_PBRMap", (int)ShaderUsage::PBRMap);
	constant("ShaderUsage_AlphaTest", (int)ShaderUsage::AlphaTest);
	constant("ShaderUsage_InstancedColor", (int)ShaderUsage::InstancedColor);
	constant("ShaderUsage_VertexWind", (int)ShaderUsage::VertexWind);

	constant("TextureTransparency_Opaque", (int)TextureTransparency::Opaque);
	constant("TextureTransparency_Transparent", (int)TextureTransparency::Transparent);

	constant("TextureFilter_Nearest", (int)TextureFilter::Nearest);
	constant("TextureFilter_Linear", (int)TextureFilter::Linear);
	constant("TextureFilter_LinearMipmapLinear", (int)TextureFilter::LinearMipmapLinear);
	constant("TextureFilter_LinearMipmapNearest", (int)TextureFilter::LinearMipmapNearest);
	constant("TextureFilter_NearestMipmapNearest", (int)TextureFilter::NearestMipmapNearest);
	constant("TextureFilter_NearestMipmapLinear", (int)TextureFilter::NearestMipmapLinear);

	// Correct name + Lua typo alias
	constant("TextureRepeat_Clamp", (int)TextureRepeat::Clamp);
	constant("TextureRepeat_Clapm", (int)TextureRepeat::Clamp);
	constant("TextureRepeat_ClampToBorder", (int)TextureRepeat::ClampToBorder);
	constant("TextureRepeat_ClampToEdge", (int)TextureRepeat::ClampToEdge);
	constant("TextureRepeat_Repeat", (int)TextureRepeat::Repeat);

	constant("TextureDataType_RGBA", (int)TextureDataType::RGBA);
	constant("TextureDataType_BGR", (int)TextureDataType::BGR);
	constant("TextureDataType_BGRA", (int)TextureDataType::BGRA);
	constant("TextureDataType_DepthComponent", (int)TextureDataType::DepthComponent);
	constant("TextureDataType_DepthComponent16", (int)TextureDataType::DepthComponent16);
	constant("TextureDataType_DepthComponent24", (int)TextureDataType::DepthComponent24);
	constant("TextureDataType_DepthComponent32", (int)TextureDataType::DepthComponent32);
	constant("TextureDataType_R", (int)TextureDataType::R8);
	constant("TextureDataType_R16F", (int)TextureDataType::R16F);
	constant("TextureDataType_R32F", (int)TextureDataType::R32F);
	constant("TextureDataType_R16I", (int)TextureDataType::R16I);
	constant("TextureDataType_R32I", (int)TextureDataType::R32I);
	constant("TextureDataType_RG", (int)TextureDataType::RG8);
	constant("TextureDataType_RG16F", (int)TextureDataType::RG16F);
	constant("TextureDataType_RG32F", (int)TextureDataType::RG32F);
	constant("TextureDataType_RG16I", (int)TextureDataType::RG16I);
	constant("TextureDataType_RG32I", (int)TextureDataType::RG32I);
	constant("TextureDataType_RGB", (int)TextureDataType::RGB8);
	constant("TextureDataType_RGB16F", (int)TextureDataType::RGB16F);
	constant("TextureDataType_RGB32F", (int)TextureDataType::RGB32F);
	constant("TextureDataType_RGB16I", (int)TextureDataType::RGB16I);
	constant("TextureDataType_RGB32I", (int)TextureDataType::RGB32I);
	constant("TextureDataType_RGBA16F", (int)TextureDataType::RGBA16F);
	constant("TextureDataType_RGBA32F", (int)TextureDataType::RGBA32F);
	constant("TextureDataType_RGBA16I", (int)TextureDataType::RGBA16I);
	constant("TextureDataType_RGBA32I", (int)TextureDataType::RGBA32I);
	constant("TextureDataType_LUMINANCE", (int)TextureDataType::LUMINANCE);
	constant("TextureDataType_LUMINANCE_ALPHA", (int)TextureDataType::LUMINANCE_ALPHA);

	constant("TextureType_CubemapPositive_X", (int)TextureType::CubemapPositive_X);
	constant("TextureType_CubemapNegative_X", (int)TextureType::CubemapNegative_X);
	constant("TextureType_CubemapPositive_Y", (int)TextureType::CubemapPositive_Y);
	constant("TextureType_CubemapNegative_Y", (int)TextureType::CubemapNegative_Y);
	constant("TextureType_CubemapPositive_Z", (int)TextureType::CubemapPositive_Z);
	constant("TextureType_CubemapNegative_Z", (int)TextureType::CubemapNegative_Z);
	constant("TextureType_Texture_Multisample", (int)TextureType::Texture_Multisample);
	constant("TextureType_Texture", (int)TextureType::Texture);

	constant("FrameBufferAttachmentFormat_Color_Attachment0", (int)FrameBufferAttachmentFormat::Color_Attachment0);
	constant("FrameBufferAttachmentFormat_Color_Attachment1", (int)FrameBufferAttachmentFormat::Color_Attachment1);
	constant("FrameBufferAttachmentFormat_Color_Attachment2", (int)FrameBufferAttachmentFormat::Color_Attachment2);
	constant("FrameBufferAttachmentFormat_Color_Attachment3", (int)FrameBufferAttachmentFormat::Color_Attachment3);
	constant("FrameBufferAttachmentFormat_Color_Attachment4", (int)FrameBufferAttachmentFormat::Color_Attachment4);
	constant("FrameBufferAttachmentFormat_Color_Attachment5", (int)FrameBufferAttachmentFormat::Color_Attachment5);
	constant("FrameBufferAttachmentFormat_Color_Attachment6", (int)FrameBufferAttachmentFormat::Color_Attachment6);
	constant("FrameBufferAttachmentFormat_Color_Attachment7", (int)FrameBufferAttachmentFormat::Color_Attachment7);
	constant("FrameBufferAttachmentFormat_Color_Attachment8", (int)FrameBufferAttachmentFormat::Color_Attachment8);
	constant("FrameBufferAttachmentFormat_Color_Attachment9", (int)FrameBufferAttachmentFormat::Color_Attachment9);
	constant("FrameBufferAttachmentFormat_Color_Attachment10", (int)FrameBufferAttachmentFormat::Color_Attachment10);
	constant("FrameBufferAttachmentFormat_Color_Attachment11", (int)FrameBufferAttachmentFormat::Color_Attachment11);
	constant("FrameBufferAttachmentFormat_Color_Attachment12", (int)FrameBufferAttachmentFormat::Color_Attachment12);
	constant("FrameBufferAttachmentFormat_Color_Attachment13", (int)FrameBufferAttachmentFormat::Color_Attachment13);
	constant("FrameBufferAttachmentFormat_Color_Attachment14", (int)FrameBufferAttachmentFormat::Color_Attachment14);
	constant("FrameBufferAttachmentFormat_Color_Attachment15", (int)FrameBufferAttachmentFormat::Color_Attachment15);
	constant("FrameBufferAttachmentFormat_Depth_Attachment", (int)FrameBufferAttachmentFormat::Depth_Attachment);
	constant("FrameBufferAttachmentFormat_Stencil_Attachment", (int)FrameBufferAttachmentFormat::Stencil_Attachment);

	constant("RenderBufferDataType_RGBA", (int)RenderBufferDataType::RGBA);
	constant("RenderBufferDataType_Depth", (int)RenderBufferDataType::Depth);
	constant("RenderBufferDataType_Stencil", (int)RenderBufferDataType::Stencil);
	constant("RenderBufferDataType_RGBA_Multisample", (int)RenderBufferDataType::RGBA_Multisample);
	constant("RenderBufferDataType_Depth_Multisample", (int)RenderBufferDataType::Depth_Multisample);
	constant("RenderBufferDataType_Stencil_Multisample", (int)RenderBufferDataType::Stencil_Multisample);

	constant("FBOAttachmentType_Texture", (int)FBOAttachmentType::Texture);
	constant("FBOAttachmentType_RenderBuffer", (int)FBOAttachmentType::RenderBuffer);

	constant("FBOAccess_Read_Write", (int)FBOAccess::Read_Write);
	constant("FBOAccess_Read", (int)FBOAccess::Read);
	constant("FBOAccess_Write", (int)FBOAccess::Write);

	constant("FBOBufferBit_Color", (int)FBOBufferBit::Color);
	constant("FBOBufferBit_Depth", (int)FBOBufferBit::Depth);
	constant("FBOBufferBit_Stencil", (int)FBOBufferBit::Stencil);

	constant("BufferBit_None", (int)Buffer_Bit::None);
	constant("BufferBit_Color", (int)Buffer_Bit::Color);
	constant("BufferBit_Depth", (int)Buffer_Bit::Depth);
	constant("BufferBit_Stencil", (int)Buffer_Bit::Stencil);

	constant("BlendFunc_Zero", (int)BlendFunc::Zero);
	constant("BlendFunc_One", (int)BlendFunc::One);
	constant("BlendFunc_Src_Alpha", (int)BlendFunc::Src_Alpha);
	constant("BlendFunc_One_Minus_Src_Alpha", (int)BlendFunc::One_Minus_Src_Alpha);
	constant("BlendFunc_Dst_Alpha", (int)BlendFunc::Dst_Alpha);
	constant("BlendFunc_One_Minus_Dst_Alpha", (int)BlendFunc::One_Minus_Dst_Alpha);

	constant("CullFace_BackFace", (int)CullFace::BackFace);
	constant("CullFace_FrontFace", (int)CullFace::FrontFace);
	constant("CullFace_DoubleSided", (int)CullFace::DoubleSided);

	constant("FBOFilter_Linear", (int)FBOFilter::Linear);
	constant("FBOFilter_Nearest", (int)FBOFilter::Nearest);

	constant("DrawingType_Triangles", (int)DrawingType::Triangles);
	constant("DrawingType_Lines", (int)DrawingType::Lines);
	constant("DrawingType_Line_Loop", (int)DrawingType::Line_Loop);
	constant("DrawingType_Line_Strip", (int)DrawingType::Line_Strip);
	constant("DrawingType_Triangle_Fan", (int)DrawingType::Triangle_Fan);
	constant("DrawingType_Triangle_Strip", (int)DrawingType::Triangle_Strip);
	constant("DrawingType_Quads", (int)DrawingType::Quads);
	constant("DrawingType_Points", (int)DrawingType::Points);
	constant("DrawingType_Polygons", (int)DrawingType::Polygons);

	constant("UniformUsage_ProjectionMatrix", (int)Uniforms::DataUsage::ProjectionMatrix);
	constant("UniformUsage_ViewMatrix", (int)Uniforms::DataUsage::ViewMatrix);
	constant("UniformUsage_ModelMatrix", (int)Uniforms::DataUsage::ModelMatrix);
	constant("UniformUsage_CameraPosition", (int)Uniforms::DataUsage::CameraPosition);
	constant("UniformUsage_Timer", (int)Uniforms::DataUsage::Timer);
	constant("UniformUsage_NearFarPlane", (int)Uniforms::DataUsage::NearFarPlane);
	constant("UniformUsage_Other", (int)Uniforms::DataUsage::Other);

	constant("UniformDataType_Int", (int)Uniforms::DataType::Int);
	constant("UniformDataType_Float", (int)Uniforms::DataType::Float);
	constant("UniformDataType_Vec2", (int)Uniforms::DataType::Vec2);
	constant("UniformDataType_Vec3", (int)Uniforms::DataType::Vec3);
	constant("UniformDataType_Vec4", (int)Uniforms::DataType::Vec4);
	constant("UniformDataType_Matrix", (int)Uniforms::DataType::Matrix);

	constant("RTT_Color", (int)RTT::Color);
	constant("RTT_Depth", (int)RTT::Depth);
	constant("RTT_LastRTT", (int)RTT::LastRTT);
	constant("RTT_CustomTexture", (int)RTT::CustomTexture);

	constant("CullingMode_FrustumCulling", (int)CullingMode::FrustumCulling);

	constant("ParticleBlendMode_AlphaBlend", (int)ParticleBlendMode::AlphaBlend);
	constant("ParticleBlendMode_Additive", (int)ParticleBlendMode::Additive);

	constant("AttenuationModel_None", (int)AttenuationModel::None);
	constant("AttenuationModel_Inverse", (int)AttenuationModel::Inverse);
	constant("AttenuationModel_Linear", (int)AttenuationModel::Linear);
	constant("AttenuationModel_Exponential", (int)AttenuationModel::Exponential);

	constant("AudioFilterType_None", (int)AudioFilterType::None);
	constant("AudioFilterType_LowPass", (int)AudioFilterType::LowPass);
	constant("AudioFilterType_HighPass", (int)AudioFilterType::HighPass);
	constant("AudioFilterType_BandPass", (int)AudioFilterType::BandPass);

	constant("AudioEQType_None", (int)AudioEQType::None);
	constant("AudioEQType_Peak", (int)AudioEQType::Peak);
	constant("AudioEQType_Notch", (int)AudioEQType::Notch);
	constant("AudioEQType_LowShelf", (int)AudioEQType::LowShelf);
	constant("AudioEQType_HighShelf", (int)AudioEQType::HighShelf);

	using namespace Event::Input;
	constant("Key_A", (int)Keyboard::A);
	constant("Key_B", (int)Keyboard::B);
	constant("Key_C", (int)Keyboard::C);
	constant("Key_D", (int)Keyboard::D);
	constant("Key_E", (int)Keyboard::E);
	constant("Key_F", (int)Keyboard::F);
	constant("Key_G", (int)Keyboard::G);
	constant("Key_H", (int)Keyboard::H);
	constant("Key_I", (int)Keyboard::I);
	constant("Key_J", (int)Keyboard::J);
	constant("Key_K", (int)Keyboard::K);
	constant("Key_L", (int)Keyboard::L);
	constant("Key_M", (int)Keyboard::M);
	constant("Key_N", (int)Keyboard::N);
	constant("Key_O", (int)Keyboard::O);
	constant("Key_P", (int)Keyboard::P);
	constant("Key_Q", (int)Keyboard::Q);
	constant("Key_R", (int)Keyboard::R);
	constant("Key_S", (int)Keyboard::S);
	constant("Key_T", (int)Keyboard::T);
	constant("Key_U", (int)Keyboard::U);
	constant("Key_V", (int)Keyboard::V);
	constant("Key_W", (int)Keyboard::W);
	constant("Key_X", (int)Keyboard::X);
	constant("Key_Y", (int)Keyboard::Y);
	constant("Key_Z", (int)Keyboard::Z);
	constant("Key_Num0", (int)Keyboard::Num0);
	constant("Key_Num1", (int)Keyboard::Num1);
	constant("Key_Num2", (int)Keyboard::Num2);
	constant("Key_Num3", (int)Keyboard::Num3);
	constant("Key_Num4", (int)Keyboard::Num4);
	constant("Key_Num5", (int)Keyboard::Num5);
	constant("Key_Num6", (int)Keyboard::Num6);
	constant("Key_Num7", (int)Keyboard::Num7);
	constant("Key_Num8", (int)Keyboard::Num8);
	constant("Key_Num9", (int)Keyboard::Num9);
	constant("Key_Escape", (int)Keyboard::Escape);
	constant("Key_LControl", (int)Keyboard::LControl);
	constant("Key_LShift", (int)Keyboard::LShift);
	constant("Key_LAlt", (int)Keyboard::LAlt);
	constant("Key_RControl", (int)Keyboard::RControl);
	constant("Key_RShift", (int)Keyboard::RShift);
	constant("Key_RAlt", (int)Keyboard::RAlt);
	constant("Key_Space", (int)Keyboard::Space);
	constant("Key_Return", (int)Keyboard::Return);
	constant("Key_Back", (int)Keyboard::Back);
	constant("Key_Tab", (int)Keyboard::Tab);
	constant("Key_Left", (int)Keyboard::Left);
	constant("Key_Right", (int)Keyboard::Right);
	constant("Key_Up", (int)Keyboard::Up);
	constant("Key_Down", (int)Keyboard::Down);
	constant("Key_F1", (int)Keyboard::F1);
	constant("Key_F2", (int)Keyboard::F2);
	constant("Key_F3", (int)Keyboard::F3);
	constant("Key_F4", (int)Keyboard::F4);
	constant("Key_F5", (int)Keyboard::F5);
	constant("Key_F6", (int)Keyboard::F6);
	constant("Key_F7", (int)Keyboard::F7);
	constant("Key_F8", (int)Keyboard::F8);
	constant("Key_F9", (int)Keyboard::F9);
	constant("Key_F10", (int)Keyboard::F10);
	constant("Key_F11", (int)Keyboard::F11);
	constant("Key_F12", (int)Keyboard::F12);

	constant("MouseButton_Left", (int)Mouse::Left);
	constant("MouseButton_Middle", (int)Mouse::Middle);
	constant("MouseButton_Right", (int)Mouse::Right);
}

#endif /* EMSCRIPTEN */
