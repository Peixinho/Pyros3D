//============================================================================
// Name        : PyrosLuaAssets.cpp
// Description : Textures, shapes, models, fonts, animations, particles.
//============================================================================

#ifdef LUA_BINDINGS

#include <Pyros3D/Assets/Renderable/Trees/Tree.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaBindings.h>
#include <Pyros3D/AnimationManager/Components/IKComponent.h>
#include <Pyros3D/Utils/Bindings/PyrosLuaHelpers.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Card.h>
#include <Pyros3D/Assets/AssetPreload.h>
#include <map>
#include <memory>

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
			// Card.new(left, right, bottom, top[, crossed]): an upright quad
			// standing on its base - what an impostor's picture is put on
			// (see Card.h); crossed, the default, adds a second at right angles.
			lua->new_usertype<Card>("Card",
				sol::factories(
					[](float left, float right, float bottom, float top, bool crossed) { return std::make_shared<Card>(left, right, bottom, top, crossed); },
					[](float left, float right, float bottom, float top) { return std::make_shared<Card>(left, right, bottom, top); }
				),
				sol::base_classes, sol::bases<Renderable>()
				);
			// TreeMesh.new(species, seed, part[, detail]): one part (TreePart.Wood / Leaves) of
			// a tree grown from a species (TreeSpecies.*) and a seed - see Tree.h.
			lua->new_usertype<TreeMesh>("TreeMesh",
				sol::factories(
					[](uint32 species, uint32 seed, uint32 part, uint32 detail) { return std::make_shared<TreeMesh>(species, seed, part, detail); },
					[](uint32 species, uint32 seed, uint32 part) { return std::make_shared<TreeMesh>(species, seed, part, 0); }
				),
				sol::base_classes, sol::bases<Renderable>(),
				"getHeight", &TreeMesh::GetHeight,
				"getCrownRadius", &TreeMesh::GetCrownRadius,
				"getTrunkRadius", &TreeMesh::GetTrunkRadius,
				"barkTexture", [](uint32 species) { return TreeMesh::BarkTexture(species); },
				"leafTexture", [](uint32 species) { return TreeMesh::LeafTexture(species); },
				"speciesName", [](uint32 species) { return std::string(TreeMesh::SpeciesName(species)); },
				"hasLeaves", [](uint32 species) { return TreeMesh::HasLeaves(species); }
				);
			(*lua)["TreeSpecies"] = lua->create_table_with("Oak", TreeSpecies::Oak, "Birch", TreeSpecies::Birch, "Spruce", TreeSpecies::Spruce,
				"ScotsPine", TreeSpecies::ScotsPine, "Palm", TreeSpecies::Palm, "Dead", TreeSpecies::Dead, "Bush", TreeSpecies::Bush, "Count", TreeSpecies::Count);
			(*lua)["TreePart"] = lua->create_table_with("Wood", TreePart::Wood, "Leaves", TreePart::Leaves);
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
					// (one Model for a file, whoever asks: Model::LoadShared. Two
					// things made from the same file are then one set of buffers
					// and can be drawn together. Model.newUnique reads its own.)
					[](const std::string &path) { return Model::LoadShared(path); },
					[](const std::string &path, bool mergeMeshes) { return Model::LoadShared(path, mergeMeshes); }
				),
				"newUnique", sol::overload(
					[](const std::string &path) { return std::make_shared<Model>(path); },
					[](const std::string &path, bool mergeMeshes) { return std::make_shared<Model>(path, mergeMeshes); }
				),
				// The model's own box, in its own units: how big it is and where
				// its middle lies - what a stand-in for it has to be the size of.
				"getBoundingMin", [](Model &m) { return m.GetBoundingMinValue(); },
				"getBoundingMax", [](Model &m) { return m.GetBoundingMaxValue(); },
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
		// preload(path) / preload({ path, ... }): read now, kept until the scene
		// goes - so that the first use of it later is not a halt. Models and
		// textures. preloaded() says how many are held.
		lua->set_function("preload", sol::overload(
			[](const std::string &path) { return AssetPreload::Add(path); },
			[](sol::table list) {
				bool all = true;
				for (size_t i = 1; i <= list.size(); i++) { sol::optional<std::string> p = list[i]; if (p) all = AssetPreload::Add(*p) && all; }
				return all;
			}));
		lua->set_function("preloaded", []() { return (int)AssetPreload::Count(); });

		{
			// Skeleton Animation
			sol::constructors<sol::types<>> con;
			lua->new_usertype<SkeletonAnimation>("SekeletonAnimation",
				con,
				// SekeletonAnimation.shared(path): the one set of clips read from
				// that file, for everybody who plays them (each still makes its
				// own instance) - read once while anybody holds it.
				"shared", [](const std::string &path) {
					static std::map<std::string, std::weak_ptr<SkeletonAnimation> > cache;
					std::map<std::string, std::weak_ptr<SkeletonAnimation> >::iterator it = cache.find(path);
					if (it != cache.end()) { if (std::shared_ptr<SkeletonAnimation> hit = it->second.lock()) return hit; cache.erase(it); }
					std::shared_ptr<SkeletonAnimation> made = std::make_shared<SkeletonAnimation>();
					made->LoadAnimation(path);
					cache[path] = made;
					return made;
				},
				"loadAnimation", &SkeletonAnimation::LoadAnimation,
				"getNumberAnimatons", &SkeletonAnimation::GetNumberAnimations,
				"update", &SkeletonAnimation::Update,
				"createInstance", &SkeletonAnimation::CreateInstance,
				"destroyInstance", &SkeletonAnimation::DestroyInstance,
				"getAnimationIDByName", &SkeletonAnimation::GetAnimationIDByName,
				// addEvent(clip, time, name): an event added at run time (a
				// footstep a game found with inst:findContacts). False if the
				// clip already has it or there is no such clip.
				"addEvent", [](SkeletonAnimation& self, const std::string& clip, const float time, const std::string& name) -> bool {
					const int32 id = self.GetAnimationIDByName(clip);
					return id >= 0 && self.AddEvent((uint32)id, time, name);
				}
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
				// Ragdoll surface: drive bones from world-space transforms
				// (physics bodies), then upload once.
				"setBoneWorld", &SkeletonAnimationInstance::SetBoneWorldTransform,
				"holdPose", &SkeletonAnimationInstance::SetPoseHeld,
				"isPoseHeld", &SkeletonAnimationInstance::IsPoseHeld,
				"refreshSkinning", &SkeletonAnimationInstance::RefreshSkinning,
				// Turn a bone on top of whatever clip is posing it: the head
				// toward where the player looks. Model space, about the joint.
				"setBoneAim", &SkeletonAnimationInstance::SetBoneAim,
				// Feet on uneven ground: setFootGround(thigh, foot, groundY, normal [, weight]) -
				// the world height of the ground under that foot and its normal (a ray
				// down, a terrain lookup); the hips come down for the lower foot, the
				// legs reach, the feet lie flat. Set it each frame; clearFootGround([foot]).
				"setFootGround", [](SkeletonAnimationInstance& self, int32 upper, int32 foot, float groundY, const Vec3& normal, sol::optional<float> weight) -> bool {
					return self.SetFootGroundWorld(upper, foot, groundY, normal, weight.value_or(1.f));
				},
				"clearFootGround", [](SkeletonAnimationInstance& self, sol::optional<int32> foot) { self.ClearFootGround(foot.value_or(-1)); },
				// how far the hips may come down, a share of the leg (0.45); 0 = the legs alone
				// reach, nothing above the hips moves (a first-person body: the gun stays put)
				"setFootGroundMaxDrop", &SkeletonAnimationInstance::SetFootGroundMaxDrop,
				"clearBoneAim", &SkeletonAnimationInstance::ClearBoneAim,
				// Scale the skin of a bone and everything below it about its
				// joint; the pose is untouched. Near zero hides the part.
				"setBoneScale", &SkeletonAnimationInstance::SetBoneScale,
				"setBoneScaleInView", &SkeletonAnimationInstance::SetBoneScaleInView,
				// (for the view only, and this bone's own skin only: what is below it keeps its size)
				"setBoneSkinScaleInView", &SkeletonAnimationInstance::SetBoneSkinScaleInView,
				"clearBoneScale", &SkeletonAnimationInstance::ClearBoneScale,
				"getBoneLocal", &SkeletonAnimationInstance::GetBoneLocalTransform,
				"resetToBindPose", &SkeletonAnimationInstance::ResetToBindPose,
				"getBonePosition", [](SkeletonAnimationInstance& self, int32 boneId) -> Vec3 {
					if (boneId < 0 || (uint32)boneId >= self.GetNumberBones()) return Vec3();
					return self.GetBoneGlobalTransform(boneId).GetTranslation();
				},
				// The skeleton itself, not just one bone's position. A script
				// that wants to build something FROM a rig - a ragdoll, a
				// hitbox set, an attachment socket - has to be able to walk
				// the hierarchy and read the names, and none of that was
				// reachable: getBoneIdByName could only confirm a name the
				// script already guessed. Guessed names are exactly why the
				// first ragdoll here only worked on one model.
				"getBoneName", [](SkeletonAnimationInstance& self, int32 boneId) -> std::string {
					const std::vector<Bone>& bones = self.GetSkeletonBones();
					if (boneId < 0 || (size_t)boneId >= bones.size()) return std::string();
					return bones[boneId].name;
				},
				"getBoneParent", [](SkeletonAnimationInstance& self, int32 boneId) -> int32 {
					const std::vector<Bone>& bones = self.GetSkeletonBones();
					if (boneId < 0 || (size_t)boneId >= bones.size()) return -1;
					return bones[boneId].parent;
				},
				// Full MODEL-space transform, rotation included - multiply by
				// the owning GameObject's world matrix for world space.
				// getBonePosition() throws the rotation away, and a rest pose
				// with no orientation in it cannot tell a body which way the
				// limb it stands for is pointing.
				"getBoneGlobal", [](SkeletonAnimationInstance& self, int32 boneId) -> Matrix {
					if (boneId < 0 || (uint32)boneId >= self.GetNumberBones()) return Matrix();
					return self.GetBoneGlobalTransform(boneId);
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
				// Play a clip BY NAME, which is how one is referred to
				// everywhere else (the editor, autoPlay2D, the scene file) -
				// Play() takes an index, and indices are positional, so adding
				// a clip in the editor silently renumbers every Play() call in
				// every script.
				//
				// It also passes the right `scale`. Play()'s scale is the
				// weight of the OTHER clips, not of this one: 1 - the obvious
				// value for "play this fully" - means "discard this clip in
				// favour of what was blended before it", so the rig sits in
				// bind pose and the clip appears not to run at all. See
				// BlendWeightToScale in AnimationEditorDocument.h; this is the
				// same inversion, and it has already cost one script.
				//
				// Returns false when the rig has no clip of that name, rather
				// than playing something arbitrary.
				// startTime is normalized (0..1 of the clip's duration) and
				// exists so a crowd does not march in lockstep. Every pooled
				// character calling playClip("walk") got startTime 0 and the
				// same speed, so twelve enemies walked as one body - there
				// was no way to phase-offset a clip from Lua at all.
				"playClip", [](SkeletonAnimationInstance& self, const std::string& name,
					sol::optional<f32> repetition, sol::optional<f32> speed,
					sol::optional<f32> startTime) -> bool {
					SkeletonAnimation* owner = self.GetOwner();
					if (!owner) return false;
					const int32 id = owner->GetAnimationIDByName(name);
					if (id < 0) return false;
					f32 start = startTime ? *startTime : 0.f;
					if (start < 0.f) start = 0.f;
					if (start > 1.f) start = 1.f;
					// "Play this clip" means instead of the others on the whole
					// skeleton. Leaving them playing made the OLDER clip the one
					// on top at full weight, so switching walk -> wave -> walk
					// showed the wave for ever (Showcase2D's character did).
					// Clips on a layer (an upper-body aim) are left alone;
					// crossFade() is the one that blends.
					for (int32 o = (int32)self.GetNumberPlayingAnimations() - 1; o >= 0; o--)
						if (self.GetAnimationID((uint32)o) != (uint32)id && self.GetAnimationLayerName((uint32)o).empty())
							self.StopAnimation((uint32)o);
					self.Play((uint32)id, start,
						repetition ? *repetition : -1.f,   // -1 = loop forever
						speed ? *speed : 1.f,
						0.f, "");
					return true;
				},
				// Cross-fade to a clip by name: what changing from a walk to a
				// run should be - playClip() cuts, this blends over `duration`
				// and stops the outgoing clip when it is gone.
				// opts: { repetition, speed, startTime, layer }.
				"crossFade", [](SkeletonAnimationInstance& self, const std::string& name,
					sol::optional<f32> duration, sol::optional<sol::table> opts) -> bool {
					SkeletonAnimation* owner = self.GetOwner();
					if (!owner) return false;
					const int32 id = owner->GetAnimationIDByName(name);
					if (id < 0) return false;
					f32 repetition = -1.f, speed = 1.f, start = -1.f;
					std::string layer;
					if (opts)
					{
						repetition = opts->get_or("repetition", -1.f);
						speed = opts->get_or("speed", 1.f);
						start = opts->get_or("startTime", -1.f);
						layer = opts->get_or("layer", std::string());
					}
					return self.CrossFade((uint32)id, duration ? *duration : 0.25f, repetition, speed, start, layer) >= 0;
				},
				"fadeOut", [](SkeletonAnimationInstance& self, const std::string& name, sol::optional<f32> duration) -> bool {
					SkeletonAnimation* owner = self.GetOwner();
					if (!owner) return false;
					const int32 id = owner->GetAnimationIDByName(name);
					const int32 order = id < 0 ? -1 : self.GetAnimationPositionInVector((uint32)id);
					if (order < 0) return false;
					self.FadeOut((uint32)order, duration ? *duration : 0.25f);
					return true;
				},
				"isPlaying", [](SkeletonAnimationInstance& self, const std::string& name) -> bool {
					SkeletonAnimation* owner = self.GetOwner();
					const int32 id = owner ? owner->GetAnimationIDByName(name) : -1;
					return id >= 0 && self.IsPlaying((uint32)id);
				},
				// findContacts(clip, bone [, band]): the seconds into the clip at
				// which the bone comes down (into the lowest `band` share of its
				// height, 0.2 if not given) - where a footstep goes.
				"findContacts", [](SkeletonAnimationInstance& self, const std::string& clip, const std::string& bone, sol::optional<float> band, sol::this_state ts) -> sol::table {
					sol::state_view lua(ts);
					sol::table out = lua.create_table();
					SkeletonAnimation* owner = self.GetOwner();
					const int32 id = owner ? owner->GetAnimationIDByName(clip) : -1;
					if (id < 0) return out;
					const std::vector<f32> t = self.FindContacts((uint32)id, bone, band.value_or(0.2f));
					for (size_t i = 0; i < t.size(); i++) out[i + 1] = t[i];
					return out;
				},
				// Events the clips passed since the last call: { {clip=, name=, time=}, ... }.
				"takeEvents", [](SkeletonAnimationInstance& self, sol::this_state ts) -> sol::table {
					sol::state_view lua(ts);
					sol::table out = lua.create_table();
					std::vector<SkeletonAnimationInstance::FiredEvent> evs = self.TakeEvents();
					for (size_t i = 0; i < evs.size(); i++)
						out[i + 1] = lua.create_table_with("clip", evs[i].clip, "name", evs[i].name, "time", evs[i].time);
					return out;
				},
				// onEvent(function(name, clip, time) ... end), nil to stop. Called
				// on the main thread, after the frame's animation.
				"onEvent", [](SkeletonAnimationInstance& self, sol::object f) {
					if (f.valid() && f.get_type() == sol::type::function)
					{
						sol::protected_function fn = f;
						self.SetEventCallback([fn](const SkeletonAnimationInstance::FiredEvent& ev) {
							sol::protected_function_result r = fn(ev.name, ev.clip, ev.time);
							if (!r.valid()) { sol::error e = r; echo(std::string("ERROR: onEvent - ") + e.what()); }
						});
					}
					else self.SetEventCallback(nullptr);
				},
				// Root motion (clips flagged for it): the model-space travel since
				// the last call - or, with setApplyRootMotion(true), the engine
				// moves the object itself.
				"consumeRootMotion", &SkeletonAnimationInstance::ConsumeRootMotion,
				"setApplyRootMotion", &SkeletonAnimationInstance::SetApplyRootMotion,
				"getApplyRootMotion", &SkeletonAnimationInstance::GetApplyRootMotion,
				"play", &SkeletonAnimationInstance::Play,
				"changeProperties", &SkeletonAnimationInstance::ChangeProperties,
				// Fade or re-pace a playing clip where it is: see the header.
				"setAnimationScale", &SkeletonAnimationInstance::SetAnimationScale,
				"setAnimationSpeed", &SkeletonAnimationInstance::SetAnimationSpeed,
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
				// Opt-in GPU simulation. Ignored where the backend has no
				// compute, so a script can ask for it unconditionally and
				// still run on web - check IsGPUSimulated() for the truth.
				"gpuSimulation", &ParticleSystemDesc::gpuSimulation,
				"minLifetime", &ParticleSystemDesc::minLifetime,
				"maxLifetime", &ParticleSystemDesc::maxLifetime,
				"direction", &ParticleSystemDesc::direction,
				"spreadAngle", &ParticleSystemDesc::spreadAngle,
				"minSpeed", &ParticleSystemDesc::minSpeed,
				"maxSpeed", &ParticleSystemDesc::maxSpeed,
				"gravity", &ParticleSystemDesc::gravity,
				"damping", &ParticleSystemDesc::damping,
				"collide2D", &ParticleSystemDesc::collide2D,
				"bounce", &ParticleSystemDesc::bounce,
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
				// ps:setCollide2D(true, 0.4): stop at the 2D world's bodies,
				// keeping that much of the speed into a surface.
				"setCollide2D", sol::overload(
					[](ParticleSystem &p, const bool on) { p.SetCollide2D(on); },
					[](ParticleSystem &p, const bool on, const f32 bounce) { p.SetCollide2D(on, bounce); },
					[](ParticleSystem &p, const bool on, const f32 bounce, const uint32 mask) { p.SetCollide2D(on, bounce, mask); }),
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
