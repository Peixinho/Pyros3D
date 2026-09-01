//============================================================================
// Name        : PyrosLuaAssets.cpp
// Description : Textures, shapes, models, fonts, animations, particles.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/AnimationManager/Components/IKComponent.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>

namespace p3d {

	void RegisterLuaAssetsEarly(sol::state* lua)
	{
		{
			// Cube - shared_ptr via sol::factories
			lua->new_usertype<Cube>("Cube",
				sol::factories(
					[](float width, float height, float depth, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Cube>(width, height, depth, smooth, flip, TangentBitangent); },
					[](float width, float height, float depth, bool smooth, bool flip) { return std::make_shared<Cube>(width, height, depth, smooth, flip); },
					[](float width, float height, float depth, bool smooth) { return std::make_shared<Cube>(width, height, depth, smooth); },
					[](float width, float height, float depth) { return std::make_shared<Cube>(width, height, depth); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Capsule - shared_ptr via sol::factories
			lua->new_usertype<Capsule>("Capsule",
				sol::factories(
					[](float radius, float height, int numRings, int segmentsW, int segmentsH, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH, smooth, flip, TangentBitangent); },
					[](float radius, float height, int numRings, int segmentsW, int segmentsH, bool smooth, bool flip) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH, smooth, flip); },
					[](float radius, float height, int numRings, int segmentsW, int segmentsH, bool smooth) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH, smooth); },
					[](float radius, float height, int numRings, int segmentsW, int segmentsH) { return std::make_shared<Capsule>(radius, height, numRings, segmentsW, segmentsH); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Cone - shared_ptr via sol::factories
			lua->new_usertype<Cone>("Cone",
				sol::factories(
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip, TangentBitangent); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded, smooth); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded) { return std::make_shared<Cone>(radius, height, segmentsW, segmentsH, openEnded); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Cylinder - shared_ptr via sol::factories
			lua->new_usertype<Cylinder>("Cylinder",
				sol::factories(
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip, TangentBitangent); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth, bool flip) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded, smooth, flip); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded, bool smooth) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded, smooth); },
					[](float radius, float height, int segmentsW, int segmentsH, bool openEnded) { return std::make_shared<Cylinder>(radius, height, segmentsW, segmentsH, openEnded); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Plane - shared_ptr via sol::factories
			lua->new_usertype<Plane>("Plane",
				sol::factories(
					[](float width, float height, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Plane>(width, height, smooth, flip, TangentBitangent); },
					[](float width, float height, bool smooth, bool flip) { return std::make_shared<Plane>(width, height, smooth, flip); },
					[](float width, float height, bool smooth) { return std::make_shared<Plane>(width, height, smooth); },
					[](float width, float height) { return std::make_shared<Plane>(width, height); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Sphere - shared_ptr via sol::factories
			lua->new_usertype<Sphere>("Sphere",
				sol::factories(
					[](float radius, int segmentsW, int segmentsH, bool smooth, bool HalfSphere, bool flip, bool TangentBitangent) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth, HalfSphere, flip, TangentBitangent); },
					[](float radius, int segmentsW, int segmentsH, bool smooth, bool HalfSphere, bool flip) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth, HalfSphere, flip); },
					[](float radius, int segmentsW, int segmentsH, bool smooth, bool HalfSphere) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth, HalfSphere); },
					[](float radius, int segmentsW, int segmentsH, bool smooth) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH, smooth); },
					[](float radius, int segmentsW, int segmentsH) { return std::make_shared<Sphere>(radius, segmentsW, segmentsH); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Torus - shared_ptr via sol::factories
			lua->new_usertype<Torus>("Torus",
				sol::factories(
					[](float radius, float tube, int segmentsW, int segmentsH, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH, smooth, flip, TangentBitangent); },
					[](float radius, float tube, int segmentsW, int segmentsH, bool smooth, bool flip) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH, smooth, flip); },
					[](float radius, float tube, int segmentsW, int segmentsH, bool smooth) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH, smooth); },
					[](float radius, float tube, int segmentsW, int segmentsH) { return std::make_shared<Torus>(radius, tube, segmentsW, segmentsH); },
					[](float radius, float tube, int segmentsW) { return std::make_shared<Torus>(radius, tube, segmentsW); },
					[](float radius, float tube) { return std::make_shared<Torus>(radius, tube); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// TorusKnot - shared_ptr via sol::factories
			lua->new_usertype<TorusKnot>("TorusKnot",
				sol::factories(
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale, bool smooth, bool flip, bool TangentBitangent) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale, smooth, flip, TangentBitangent); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale, bool smooth, bool flip) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale, smooth, flip); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale, bool smooth) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale, smooth); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q, int heightscale) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q, heightscale); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p, float q) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p, q); },
					[](float radius, float tube, int segmentsW, int segmentsH, float p) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH, p); },
					[](float radius, float tube, int segmentsW, int segmentsH) { return std::make_shared<TorusKnot>(radius, tube, segmentsW, segmentsH); },
					[](float radius, float tube, int segmentsW) { return std::make_shared<TorusKnot>(radius, tube, segmentsW); },
					[](float radius, float tube) { return std::make_shared<TorusKnot>(radius, tube); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Model - shared_ptr via sol::factories
			lua->new_usertype<Model>("Model",
				sol::factories(
					[](const std::string &path) { return std::make_shared<Model>(path); },
					[](const std::string &path, bool mergeMeshes) { return std::make_shared<Model>(path, mergeMeshes); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
		}
		{
			// Decals - shared_ptr via sol::factories
			lua->new_usertype<Decal>("Decal",
				sol::factories(
					[](std::vector<DecalVertex> vertices, bool haveBones) { return std::make_shared<Decal>(vertices, haveBones); },
					[](std::vector<DecalVertex> vertices) { return std::make_shared<Decal>(vertices); }
				),
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
			// Text - shared_ptr via sol::factories
			lua->new_usertype<Text>("Text",
				sol::factories(
					[](Font* font, const std::string &text, float charWidth, float charHeight, const Vec4 &color, bool DynamicText) { return std::make_shared<Text>(font, text, charWidth, charHeight, color, DynamicText); }
				),
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


	}

	void RegisterLuaAssetsMid(sol::state* lua)
	{
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
				// Bone queries. The pose API existed in C++ (the editor and
				// IKSolver use it) but nothing was exposed to script, so a
				// scene could not ask where a hand actually is - which is
				// exactly what an IK handle needs in order to sit on it.
				"getNumberBones", &SkeletonAnimationInstance::GetNumberBones,
				"getBoneIdByName", [](SkeletonAnimationInstance& self, const std::string& name) -> int32 {
					const std::vector<Bone>& bones = self.GetSkeletonBones();
					for (size_t i = 0; i < bones.size(); i++)
						if (bones[i].name == name) return bones[i].self;
					return -1;
				},
				// MODEL space - multiply by the owning GameObject's world
				// matrix for world space.
				"getBonePosition", [](SkeletonAnimationInstance& self, int32 boneId) -> Vec3 {
					if (boneId < 0 || (uint32)boneId >= self.GetNumberBones()) return Vec3();
					return self.GetBoneGlobalTransform(boneId).GetTranslation();
				},
				// IK from script. The solver already existed and was reachable
				// only from C++ and the editor, so a game could not aim a
				// hand or plant a foot - the two things IK is actually for.
				// Target is model space (what getBonePosition returns);
				// the pole is left to the solver, which is what a planar
				// chain wants - see SceneEditor::AgentIKSolve2D.
				"solveIK", [](SkeletonAnimationInstance& self, int32 rootBone,
					int32 effectorBone, const Vec3& target) -> bool {
					return IKSolver::Solve(&self, rootBone, effectorBone, target,
						Vec3(0.f, 0.f, 0.f));
				},
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
				"getFrameShared", &TextureAnimation::GetFrameShared,
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
				// shared_ptr - SetColorMap/etc. expect shared ownership;
				// binding the raw GetTexture() made sol invent a bogus
				// shared_ptr from Texture* and SEGV in SetColorMap.
				"getTexture", &TextureAnimationInstance::GetTextureShared,
				"getFrame", &TextureAnimationInstance::GetFrame,
				"getOwner", &TextureAnimationInstance::GetOwner
				);
		}

	}

	void RegisterLuaAssetsLate(sol::state* lua)
	{
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

			lua->new_usertype<ParticleSystem>("ParticleSystem",
				sol::factories(
					[](const ParticleSystemDesc &desc) { return std::make_shared<ParticleSystem>(desc); }
				),
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

	}

	void RegisterLuaAssets(sol::state* lua)
	{
		RegisterLuaAssetsEarly(lua);
		RegisterLuaAssetsMid(lua);
		RegisterLuaAssetsLate(lua);
	}

} // namespace p3d

#endif
