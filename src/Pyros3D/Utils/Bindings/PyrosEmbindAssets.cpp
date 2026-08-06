//============================================================================
// Name        : PyrosEmbindAssets.cpp
// Description : Embind textures, shapes, models, fonts, animations, particles.
//============================================================================

#if defined(__EMSCRIPTEN__) || defined(EMSCRIPTEN)

#include <emscripten/bind.h>

#include <Pyros3D/Utils/Bindings/PyrosEmbindHelpers.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Capsule.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cone.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/TorusKnot.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/Assets/Renderable/Text/Text.h>
#include <Pyros3D/Assets/Font/Font.h>
#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>
#include <Pyros3D/Rendering/Components/Particles/ParticleSystem.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

#include <memory>

using namespace emscripten;
using namespace p3d;
using namespace p3d::Math;
using namespace p3d::embind_helpers;

namespace {

	std::shared_ptr<Texture> MakeTexture() { return std::make_shared<Texture>(); }

	std::shared_ptr<Cube> MakeCube(float w, float h, float d) { return std::make_shared<Cube>(w, h, d); }
	std::shared_ptr<Cube> MakeCubeS(float w, float h, float d, bool smooth) { return std::make_shared<Cube>(w, h, d, smooth); }
	std::shared_ptr<Cube> MakeCubeSF(float w, float h, float d, bool smooth, bool flip) { return std::make_shared<Cube>(w, h, d, smooth, flip); }
	std::shared_ptr<Cube> MakeCubeSFT(float w, float h, float d, bool smooth, bool flip, bool tb) { return std::make_shared<Cube>(w, h, d, smooth, flip, tb); }

	std::shared_ptr<Plane> MakePlane(float w, float h) { return std::make_shared<Plane>(w, h); }
	std::shared_ptr<Plane> MakePlaneS(float w, float h, bool smooth) { return std::make_shared<Plane>(w, h, smooth); }
	std::shared_ptr<Plane> MakePlaneSF(float w, float h, bool smooth, bool flip) { return std::make_shared<Plane>(w, h, smooth, flip); }
	std::shared_ptr<Plane> MakePlaneSFT(float w, float h, bool smooth, bool flip, bool tb) { return std::make_shared<Plane>(w, h, smooth, flip, tb); }

	std::shared_ptr<Sphere> MakeSphere(float r, int sw, int sh) { return std::make_shared<Sphere>(r, sw, sh); }
	std::shared_ptr<Sphere> MakeSphereS(float r, int sw, int sh, bool smooth) { return std::make_shared<Sphere>(r, sw, sh, smooth); }
	std::shared_ptr<Sphere> MakeSphereSH(float r, int sw, int sh, bool smooth, bool half) { return std::make_shared<Sphere>(r, sw, sh, smooth, half); }
	std::shared_ptr<Sphere> MakeSphereSHF(float r, int sw, int sh, bool smooth, bool half, bool flip) { return std::make_shared<Sphere>(r, sw, sh, smooth, half, flip); }
	std::shared_ptr<Sphere> MakeSphereSHFT(float r, int sw, int sh, bool smooth, bool half, bool flip, bool tb) { return std::make_shared<Sphere>(r, sw, sh, smooth, half, flip, tb); }

	std::shared_ptr<Capsule> MakeCapsule(float radius, float height, int rings, int sw, int sh)
	{
		return std::make_shared<Capsule>(radius, height, rings, sw, sh);
	}
	std::shared_ptr<Cone> MakeCone(float radius, float height, int sw, int sh, bool openEnded)
	{
		return std::make_shared<Cone>(radius, height, sw, sh, openEnded);
	}
	std::shared_ptr<Cylinder> MakeCylinder(float radius, float height, int sw, int sh, bool openEnded)
	{
		return std::make_shared<Cylinder>(radius, height, sw, sh, openEnded);
	}
	std::shared_ptr<Torus> MakeTorus(float radius, float tube) { return std::make_shared<Torus>(radius, tube); }
	std::shared_ptr<Torus> MakeTorusSeg(float radius, float tube, int sw, int sh) { return std::make_shared<Torus>(radius, tube, sw, sh); }
	std::shared_ptr<TorusKnot> MakeTorusKnot(float radius, float tube) { return std::make_shared<TorusKnot>(radius, tube); }
	std::shared_ptr<TorusKnot> MakeTorusKnotSeg(float radius, float tube, int sw, int sh) { return std::make_shared<TorusKnot>(radius, tube, sw, sh); }

	std::shared_ptr<Model> MakeModel(const std::string &path) { return std::make_shared<Model>(path); }
	std::shared_ptr<Model> MakeModelMerge(const std::string &path, bool merge) { return std::make_shared<Model>(path, merge); }

	std::shared_ptr<Text> MakeText(Font *font, const std::string &text, float cw, float ch, const Vec4 &color, bool dyn)
	{
		return std::make_shared<Text>(font, text, cw, ch, color, dyn);
	}
	void Text_Update(Text &t, const std::string &text, const Vec4 &color) { t.UpdateText(text, color); }
	void Text_UpdateDefault(Text &t, const std::string &text) { t.UpdateText(text); }

	std::shared_ptr<Font> MakeFont(const std::string &path, float size) { return std::make_shared<Font>(path, size); }
	std::shared_ptr<Texture> Font_GetTextureShared(Font &f) { return f.GetTextureShared(); }

	bool Texture_CreateEmpty(Texture &t, uint32 type, uint32 dataType, int32 w, int32 h)
	{
		return t.CreateEmptyTexture(type, dataType, w, h);
	}
	bool Texture_CreateEmptyFull(Texture &t, uint32 type, uint32 dataType, int32 w, int32 h,
		int mipmapping, uint32 level, uint32 msaa)
	{
		return t.CreateEmptyTexture(type, dataType, w, h, mipmapping != 0, level, msaa);
	}
	void Texture_SetRepeat2(Texture &t, uint32 wrapS, uint32 wrapT)
	{
		t.SetRepeat(wrapS, wrapT);
	}
	void Texture_SetRepeat3(Texture &t, uint32 wrapS, uint32 wrapT, int32 wrapR)
	{
		t.SetRepeat(wrapS, wrapT, wrapR);
	}

	std::shared_ptr<ParticleSystem> MakeParticleSystem(const ParticleSystemDesc &desc)
	{
		return std::make_shared<ParticleSystem>(desc);
	}

	SkeletonAnimationInstance *Skeleton_CreateInstance(SkeletonAnimation &a, RenderingComponent *rc)
	{
		return a.CreateInstance(rc);
	}
	void Skeleton_DestroyInstance(SkeletonAnimation &a, SkeletonAnimationInstance *inst)
	{
		a.DestroyInstance(inst);
	}
	void SkeletonInst_AddBone(SkeletonAnimationInstance &a, uint32 layer, const std::string &bone) { a.AddBone(layer, bone); }
	void SkeletonInst_AddBoneStr(SkeletonAnimationInstance &a, const std::string &layer, const std::string &bone) { a.AddBone(layer, bone); }
	void SkeletonInst_AddBoneAndChilds(SkeletonAnimationInstance &a, uint32 layer, const std::string &bone, bool inclusive) { a.AddBoneAndChilds(layer, bone, inclusive); }
	void SkeletonInst_RemoveBone(SkeletonAnimationInstance &a, uint32 layer, const std::string &bone) { a.RemoveBone(layer, bone); }
	void SkeletonInst_RemoveBoneStr(SkeletonAnimationInstance &a, const std::string &layer, const std::string &bone) { a.RemoveBone(layer, bone); }
	void SkeletonInst_DestroyLayer(SkeletonAnimationInstance &a, int id) { a.DestroyLayer(id); }
	void SkeletonInst_DestroyLayerStr(SkeletonAnimationInstance &a, const std::string &name) { a.DestroyLayer(name); }
	bool SkeletonInst_IsPaused(SkeletonAnimationInstance &a) { return a.IsPaused(); }
	bool SkeletonInst_IsPausedID(SkeletonAnimationInstance &a, int id) { return a.IsPaused(id); }

	TextureAnimationInstance *TextureAnim_CreateInstance(TextureAnimation &a, float speed)
	{
		return a.CreateInstance(speed);
	}
	void TextureAnim_DestroyInstance(TextureAnimation &a, TextureAnimationInstance *inst)
	{
		a.DestroyInstance(inst);
	}
	void TextureAnim_AddFrame(TextureAnimation &a, std::shared_ptr<Texture> tex)
	{
		a.AddFrame(tex);
	}
	std::shared_ptr<Texture> TextureAnimInst_GetTexture(TextureAnimationInstance &i)
	{
		return i.GetTextureShared();
	}

} // namespace

namespace p3d {
	void PyrosEmbindAssetsForceLink() {}
}

EMSCRIPTEN_BINDINGS(pyros3d_assets)
{
	class_<Texture>("Texture")
		.smart_ptr_constructor("Texture", &MakeTexture)
		.function("loadTexture", &Texture_LoadTexture)
		.function("loadTextureFull", &Texture_LoadTextureFull)
		.function("createEmptyTexture", &Texture_CreateEmpty)
		.function("createEmptyTextureFull", &Texture_CreateEmptyFull)
		.function("setMinMagFilter", &Texture::SetMinMagFilter)
		.function("setRepeat", &Texture_SetRepeat2)
		.function("setRepeat3", &Texture_SetRepeat3)
		.function("enableCompareMode", &Texture::EnableCompareMode)
		.function("setTransparency", &Texture::SetTransparency)
		.function("resize", &Texture_Resize2)
		.function("resizeLevel", &Texture_Resize3)
		.function("updateMipmap", &Texture::UpdateMipmap)
		.function("setTextureByteAlignment", &Texture::SetTextureByteAlignment)
		.function("getBindID", &Texture::GetBindID)
		.function("getWidth", &Texture::GetWidth)
		.function("getHeight", &Texture::GetHeight)
		.function("bind", &Texture::Bind)
		.function("unbind", &Texture::Unbind)
		.function("deleteTexture", &Texture::DeleteTexture)
		.class_function("getLastBindedUnit", &Texture::GetLastBindedUnit);

	class_<Cube, base<Renderable>>("Cube")
		.smart_ptr<std::shared_ptr<Cube>>("CubePtr")
		.constructor(&MakeCube)
		.constructor(&MakeCubeS)
		.constructor(&MakeCubeSF)
		.constructor(&MakeCubeSFT);

	class_<Capsule, base<Renderable>>("Capsule")
		.smart_ptr<std::shared_ptr<Capsule>>("CapsulePtr")
		.constructor(&MakeCapsule);

	class_<Cone, base<Renderable>>("Cone")
		.smart_ptr<std::shared_ptr<Cone>>("ConePtr")
		.constructor(&MakeCone);

	class_<Cylinder, base<Renderable>>("Cylinder")
		.smart_ptr<std::shared_ptr<Cylinder>>("CylinderPtr")
		.constructor(&MakeCylinder);

	class_<Plane, base<Renderable>>("Plane")
		.smart_ptr<std::shared_ptr<Plane>>("PlanePtr")
		.constructor(&MakePlane)
		.constructor(&MakePlaneS)
		.constructor(&MakePlaneSF)
		.constructor(&MakePlaneSFT);

	class_<Sphere, base<Renderable>>("Sphere")
		.smart_ptr<std::shared_ptr<Sphere>>("SpherePtr")
		.constructor(&MakeSphere)
		.constructor(&MakeSphereS)
		.constructor(&MakeSphereSH)
		.constructor(&MakeSphereSHF)
		.constructor(&MakeSphereSHFT);

	class_<Torus, base<Renderable>>("Torus")
		.smart_ptr<std::shared_ptr<Torus>>("TorusPtr")
		.constructor(&MakeTorus)
		.constructor(&MakeTorusSeg);

	class_<TorusKnot, base<Renderable>>("TorusKnot")
		.smart_ptr<std::shared_ptr<TorusKnot>>("TorusKnotPtr")
		.constructor(&MakeTorusKnot)
		.constructor(&MakeTorusKnotSeg);

	class_<Model, base<Renderable>>("Model")
		.smart_ptr<std::shared_ptr<Model>>("ModelPtr")
		.constructor(&MakeModel)
		.constructor(&MakeModelMerge);

	// Decal skipped — needs DecalVertex vector construction from JS; use Model meshes instead.

	class_<Font>("Font")
		.smart_ptr_constructor("Font", &MakeFont)
		.function("createText", &Font::CreateText)
		.function("getTexture", &Font_GetTextureShared)
		.function("getFontSize", &Font::GetFontSize);

	class_<Text, base<Renderable>>("Text")
		.smart_ptr<std::shared_ptr<Text>>("TextPtr")
		.constructor(&MakeText, allow_raw_pointers())
		.function("updateText", &Text_Update)
		.function("updateTextDefault", &Text_UpdateDefault);

	// Lua typo names preserved as primary JS type names for parity
	class_<SkeletonAnimation>("SekeletonAnimation")
		.constructor<>()
		.function("loadAnimation", &SkeletonAnimation::LoadAnimation)
		.function("getNumberAnimatons", &SkeletonAnimation::GetNumberAnimations) // Lua typo
		.function("getNumberAnimations", &SkeletonAnimation::GetNumberAnimations)
		.function("update", &SkeletonAnimation::Update)
		.function("createInstance", &Skeleton_CreateInstance, allow_raw_pointers())
		.function("destroyInstance", &Skeleton_DestroyInstance, allow_raw_pointers())
		.function("getAnimationIDByName", &SkeletonAnimation::GetAnimationIDByName);

	class_<SkeletonAnimationInstance>("SekeletonAnimationInstance")
		.constructor<SkeletonAnimation *, RenderingComponent *>(allow_raw_pointers())
		.function("getOwner", &SkeletonAnimationInstance::GetOwner, allow_raw_pointers())
		.function("play", &SkeletonAnimationInstance::Play)
		.function("changeProperties", &SkeletonAnimationInstance::ChangeProperties)
		.function("pause", &SkeletonAnimationInstance::Pause)
		.function("PauseAnimation", &SkeletonAnimationInstance::PauseAnimation)
		.function("resumeAnimation", &SkeletonAnimationInstance::ResumeAnimation)
		.function("resume", &SkeletonAnimationInstance::Resume)
		.function("stopAnimation", &SkeletonAnimationInstance::StopAnimation)
		.function("stop", &SkeletonAnimationInstance::Stop)
		.function("getAnimationCurrentProgress", &SkeletonAnimationInstance::GetAnimationCurrentProgress)
		.function("getAnimationDuration", &SkeletonAnimationInstance::GetAnimationDuration)
		.function("getAnimationCurrentTime", &SkeletonAnimationInstance::GetAnimationCurrentTime)
		.function("getAniamtionSpeed", &SkeletonAnimationInstance::GetAnimationSpeed) // Lua typo
		.function("getAnimationSpeed", &SkeletonAnimationInstance::GetAnimationSpeed)
		.function("getAnimationStartTime", &SkeletonAnimationInstance::GetAnimationStartTime)
		.function("getAnimationID", &SkeletonAnimationInstance::GetAnimationID)
		.function("getAnimationScale", &SkeletonAnimationInstance::GetAnimationScale)
		.function("createLayer", &SkeletonAnimationInstance::CreateLayer)
		.function("addBone", &SkeletonInst_AddBone)
		.function("addBoneStr", &SkeletonInst_AddBoneStr)
		.function("addBoneAndChilds", &SkeletonInst_AddBoneAndChilds)
		.function("removeBone", &SkeletonInst_RemoveBone)
		.function("removeBoneStr", &SkeletonInst_RemoveBoneStr)
		.function("destroyLayer", &SkeletonInst_DestroyLayer)
		.function("destroyLayerStr", &SkeletonInst_DestroyLayerStr)
		.function("isPaused", &SkeletonInst_IsPaused)
		.function("isPausedId", &SkeletonInst_IsPausedID);

	class_<TextureAnimation>("TextureAnimation")
		.constructor<>()
		.function("getNumberFrames", &TextureAnimation::GetNumberFrames)
		.function("addFrame", &TextureAnim_AddFrame)
		.function("update", &TextureAnimation::Update)
		.function("createInstance", &TextureAnim_CreateInstance, allow_raw_pointers())
		.function("destroyInstance", &TextureAnim_DestroyInstance, allow_raw_pointers());

	class_<TextureAnimationInstance>("TextureAnimationInstance")
		.constructor<TextureAnimation *, float>(allow_raw_pointers())
		.function("play", &TextureAnimationInstance::Play)
		.function("pause", &TextureAnimationInstance::Pause)
		.function("stop", &TextureAnimationInstance::Stop)
		.function("isPlaying", &TextureAnimationInstance::IsPlaying)
		.function("yoyo", &TextureAnimationInstance::YoYo)
		.function("getTexture", &TextureAnimInst_GetTexture)
		.function("getFrame", &TextureAnimationInstance::GetFrame)
		.function("getOwner", &TextureAnimationInstance::GetOwner, allow_raw_pointers());

	class_<ParticleSystemDesc>("ParticleSystemDesc")
		.constructor<>()
		.property("maxParticles", &ParticleSystemDesc::maxParticles)
		.property("texture", &ParticleSystemDesc::texture)
		.property("looping", &ParticleSystemDesc::looping)
		.property("emissionRate", &ParticleSystemDesc::emissionRate)
		.property("burstCount", &ParticleSystemDesc::burstCount)
		.property("minLifetime", &ParticleSystemDesc::minLifetime)
		.property("maxLifetime", &ParticleSystemDesc::maxLifetime)
		.property("direction", &ParticleSystemDesc::direction)
		.property("spreadAngle", &ParticleSystemDesc::spreadAngle)
		.property("minSpeed", &ParticleSystemDesc::minSpeed)
		.property("maxSpeed", &ParticleSystemDesc::maxSpeed)
		.property("gravity", &ParticleSystemDesc::gravity)
		.property("damping", &ParticleSystemDesc::damping)
		.property("startSize", &ParticleSystemDesc::startSize)
		.property("endSize", &ParticleSystemDesc::endSize)
		.property("sizeRandomJitter", &ParticleSystemDesc::sizeRandomJitter)
		.property("startColor", &ParticleSystemDesc::startColor)
		.property("endColor", &ParticleSystemDesc::endColor)
		.property("fadeInFraction", &ParticleSystemDesc::fadeInFraction)
		.property("fadeOutFraction", &ParticleSystemDesc::fadeOutFraction)
		.property("minRotationSpeed", &ParticleSystemDesc::minRotationSpeed)
		.property("maxRotationSpeed", &ParticleSystemDesc::maxRotationSpeed)
		.property("blendMode", &ParticleSystemDesc::blendMode)
		.property("boundingSphereRadius", &ParticleSystemDesc::boundingSphereRadius);

	class_<ParticleSystem, base<IComponent>>("ParticleSystem")
		.smart_ptr<std::shared_ptr<ParticleSystem>>("ParticleSystemPtr")
		.constructor(&MakeParticleSystem)
		.function("play", &ParticleSystem::Play)
		.function("stop", &ParticleSystem::Stop)
		.function("clear", &ParticleSystem::Clear)
		.function("getLiveParticleCount", &ParticleSystem::GetLiveParticleCount)
		.function("isPlaying", &ParticleSystem::IsPlaying)
		.function("setEmissionRate", &ParticleSystem::SetEmissionRate)
		.function("setBurstCount", &ParticleSystem::SetBurstCount)
		.function("setLifetime", &ParticleSystem::SetLifetime)
		.function("setDirection", &ParticleSystem::SetDirection)
		.function("setSpread", &ParticleSystem::SetSpread)
		.function("setSpeed", &ParticleSystem::SetSpeed)
		.function("setGravity", &ParticleSystem::SetGravity)
		.function("setDamping", &ParticleSystem::SetDamping)
		.function("setSizes", &ParticleSystem::SetSizes)
		.function("setColors", &ParticleSystem::SetColors)
		.function("setFade", &ParticleSystem::SetFade)
		.function("setRotationSpeed", &ParticleSystem::SetRotationSpeed)
		.function("setBlendMode", &ParticleSystem::SetBlendMode);
}

#endif /* EMSCRIPTEN */
