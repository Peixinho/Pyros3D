//============================================================================
// Name        : SkeletonAnimation.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animator Interface
//============================================================================

#ifndef SKELETONANIMATION_H
#define SKELETONANIMATION_H

#include <Pyros3D/Other/Export.h>
#include <functional>
#include <map>
#include <Pyros3D/Ext/Signals/Signal.h>
#include <Pyros3D/Ext/Signals/Delegate.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h>

namespace p3d {

	namespace _SkeletonAnimation
	{

		struct AnimationLayer
		{
			AnimationLayer(const std::string &name)
			{
				ID = MakeStringID(name);
				Name = name;
				usingLayer = 0;
			}

			// Layer ID - Name converted to uint32
			uint32 ID;

			// Layer Name
			std::string Name;

			// List of Affected Bones
			std::vector<int32> boneIDs;

			uint32 usingLayer;

		};

		struct SkeletonAnimation
		{
			uint32 ID;
			f32 startTime; // 0-1
			Animation* animation;
			f32 speed;
			f32 scale;

			f32 _startTime; // Real Time
			f32 _startTimeClock;
			f32 _currentTime;

			// Pause
			bool _isPaused;
			bool _resumed;
			f32 _pauseStart;
			f32 _pauseTime;

			f32 _repetition;

			std::vector<Matrix> boneTransformationPerAnimation;

			// Layers
			bool HaveLayers;
			uint32 LayerID;
			AnimationLayer* Layer;

			// Fade of `scale` from fadeFrom to fadeTo over fadeDuration seconds
			// of the update clock, started on the first update that sees it
			// (fadeStart -1). Driven by CrossFade/FadeOut; 0 duration = none.
			f32 fadeFrom = 0.f, fadeTo = 0.f, fadeStart = -1.f, fadeDuration = 0.f;
			// Stop this clip once its fade has finished (a crossfade's outgoing
			// clips): it is invisible by then, and leaving it would keep it
			// sampled - and firing events - for ever.
			bool stopAfterFade = false;
			// Clip-local time at the previous update, for firing the events
			// passed since and for root motion. Unset until the first update.
			f32 _prevLocalTime = 0.f;
			bool _prevValid = false;
			// Root motion: the root bone's sampled (un-stripped) translation at
			// the previous update, in the bone's parent space.
			Vec3 _prevRootPos;
			int32 _rootBone = -2;      // -2 unresolved, -1 none
			int32 _rootChannel = -1;
			// Bone id of each of this clip's channels (-1: not a bone here),
			// filled on the first update.
			std::vector<int32> channelBones;
		};
	}

	class SkeletonAnimation;

	class PYROS3D_API SkeletonAnimationInstance {

		friend class SkeletonAnimation;

	public:

		SkeletonAnimationInstance(SkeletonAnimation* owner, RenderingComponent* Component);
		virtual ~SkeletonAnimationInstance();

		// Play, Stop, Pause
		int32 Play(const uint32 animation, const f32 startTime, const f32 repetition = 1, const f32 speed = 1.f, const f32 scale = 1.f, const std::string &LayerName = "");
		void ChangeProperties(const uint32 animationOrder, const f32 startTime, const f32 repetition = 1, const f32 speed = 1.f, const f32 scale = 1.f);
		void Pause();

		// Cross-fade to a clip: it starts (or carries on, keeping its phase)
		// at full weight UNDER the clips already playing on the same layer
		// ("" = the base, the whole skeleton), and those fade out over
		// `duration` seconds and are then stopped. With nothing on that layer
		// to fade from, the clip fades itself in over what is below it.
		//
		// This is what Play() with blend scales cannot do on its own: Play's
		// scale is the weight of the OTHER clips, ordering decides which clip
		// is on top, and nothing ever stopped a clip that had faded away.
		// `startTime` is normalised (0..1); negative keeps the phase of a clip
		// that is already playing and starts a new one at 0. Returns the new
		// play order, or -1 for an unknown clip. Orders of other clips shift
		// when the outgoing ones stop - address clips by name.
		int32 CrossFade(const uint32 animation, const f32 duration, const f32 repetition = -1.f,
			const f32 speed = 1.f, const f32 startTime = -1.f, const std::string &LayerName = "");
		// Fades a playing clip out and stops it.
		void FadeOut(const uint32 animationOrder, const f32 duration);
		// True when `animation` (a clip id) is playing and not on its way out.
		bool IsPlaying(const uint32 animation) const;

		// ---- clip events ---------------------------------------------
		// An event a clip passed. Collected during the update (which can run
		// on a worker thread) and handed out on the main thread by
		// DeliverPending(): to the callback if one is set, and to the queue
		// TakeEvents() empties. The queue is capped so an instance nobody
		// polls cannot grow without bound.
		struct FiredEvent {
			std::string clip;
			std::string name;
			f32 time;
		};
		std::vector<FiredEvent> TakeEvents();
		void SetEventCallback(const std::function<void(const FiredEvent&)> &cb) { eventCallback = cb; }

		// ---- feet on the ground --------------------------------------
		// Where the ground is under a foot, for a body on uneven ground. After
		// the clips (and the bone aims), the hips come down as far as the
		// lowest foot needs, each leg reaches its foot as much higher than the
		// clip put it as the ground there is over the model's own floor (0
		// along GetModelUp() in model space), and the foot turns flat to the
		// ground's normal.
		// Model space, like every bone query; `weight` 0 = the clip alone.
		// `upperBone` is the thigh: thigh -> shin -> foot is a two-bone chain.
		// Holds until changed or cleared; set it each frame for a body that
		// moves. The hips that come down are the thigh's parent.
		void SetFootGround(const int32 upperBone, const int32 footBone, const f32 height,
			const Vec3 &normal, const f32 weight = 1.f);
		// With the up axis given (model space) rather than found from the skeleton.
		void SetFootGroundAlong(const int32 upperBone, const int32 footBone, const f32 height,
			const Vec3 &normal, const Vec3 &up, const f32 weight);
		// The same from the world: the ground's height (world y) right under
		// the foot as the body stands now, and its normal in world space -
		// what a ray down or a terrain lookup gives. False when the body has no
		// object to be placed by.
		bool SetFootGroundWorld(const int32 upperBone, const int32 footBone, const f32 groundY,
			const Vec3 &normal, const f32 weight = 1.f);
		void ClearFootGround(const int32 footBone = -1);   // -1: every foot
		bool HasFootGround() const { return !footGrounds.empty(); }
		// How far down the hips may go, as a share of the longest leg (0.45).
		void SetFootGroundMaxDrop(const f32 shareOfLeg) { footMaxDrop = shareOfLeg; }
		// Called by the update after the clips; public for the editor's preview.
		void ApplyFootGround();
		// Which way is up in model space, from the skeleton itself: the axis
		// it is tallest along in its bind pose, pointing away from `footBone`
		// when one is given (else from the end with fewer bones). A Y-up
		// model says +Y, a Z-up one (the stock Bip01 human) +Z. (0, 1, 0) for
		// no skeleton.
		Vec3 GetModelUp(const int32 footBone = -1) const;
		// The moments a bone comes down in a clip - for footsteps: the clip
		// sampled through THIS skeleton, the bone's height in model space, and
		// each time it drops into the lowest `band` share of its range (0.2 =
		// the bottom fifth; a share, so the model's units do not matter).
		// Seconds, in order. Empty when the clip does not lift the bone.
		std::vector<f32> FindContacts(const uint32 animation, const std::string &bone, const f32 band = 0.2f) const;
		// The same for a clip that is not (or not yet) in the owner's list -
		// the one an editor is changing.
		std::vector<f32> FindContacts(const Animation &clip, const std::string &bone, const f32 band = 0.2f) const;

		// ---- root motion ---------------------------------------------
		// Model-space travel of the root bone (ANIM_FLAG_ROOT_MOTION clips)
		// since the last call, already taken out of the pose. Horizontal
		// (x, z) only: height stays in the pose, so a jump still jumps.
		Vec3 ConsumeRootMotion();
		// When on, DeliverPending() moves the owning GameObject by the root
		// motion itself (through its rotation and scale) instead of leaving
		// it for ConsumeRootMotion(). Off by default: a physics-driven
		// character must route it through its body.
		void SetApplyRootMotion(const bool apply) { applyRootMotion = apply; }
		bool GetApplyRootMotion() const { return applyRootMotion; }

		// ---- property tracks -----------------------------------------
		// The value every playing clip's PropertyTrack blends to this frame,
		// keyed by target + property. False when no playing clip animates it
		// - the consumer then shows its own rest value.
		struct TrackValue {
			Vec4 value;
			std::string text;
			bool hasValue = false;
			bool hasText = false;
		};
		bool GetTrackValue(const std::string &target, const std::string &property, TrackValue &out) const;
		bool HasTrackValues() const { return !trackValues.empty(); }

		// Main thread, after the update: events to their listeners, root
		// motion to the owner when SetApplyRootMotion is on. RenderingComponent
		// calls it; anything driving UpdateInstance itself should too.
		void DeliverPending();

		// One property of a playing clip, without starting it again -
		// ChangeProperties() sets the start time too, so calling it every
		// frame to fade a clip holds the clip still. The scale is the blend
		// against what is under it (0: all this clip, 1: none of it), which
		// is what a cross-fade runs; the speed changes from the frame the
		// clip is on, so a walk can keep pace with the feet.
		void SetAnimationScale(const uint32 animationOrder, const f32 scale);
		void SetAnimationSpeed(const uint32 animationOrder, const f32 speed);
		void PauseAnimation(const uint32 animationOrder);
		void ResumeAnimation(const uint32 animationOrder);
		void Resume();
		void StopAnimation(const uint32 animationOrder);
		void Stop();
		bool IsPaused(const uint32 animationOrder);
		bool IsPaused();
		f32 GetAnimationCurrentProgress(const uint32 animationOrder);
		f32 GetAnimationDuration(const uint32 animationOrder);
		f32 GetAnimationCurrentTime(const uint32 animationOrder);
		f32 GetAnimationSpeed(const uint32 animationOrder);
		f32 GetAnimationStartTime(const uint32 animationOrder);
		f32 GetAnimationStartTimeProgress(const uint32 animationOrder);
		uint32 GetAnimationID(const uint32 animationOrder);
		f32 GetAnimationScale(const uint32 animationOrder);

		// Get Animation Position
		int32 GetAnimationPositionInVector(const uint32 animation);

		// Real enumeration - AnimationsToPlay had per-order getters
		// (GetAnimationCurrentTime(order) etc, above) but no way to know
		// how many were actually playing or iterate them; needed to save
		// "everything currently playing" rather than one animation at a
		// time. animationOrder IS the AnimationsToPlay index directly
		// (confirmed via Play()'s own "return AnimationsToPlay.size()-1;
		// // Return Order").
		uint32 GetNumberPlayingAnimations() const { return (uint32)AnimationsToPlay.size(); }
		// Empty string if this playing animation isn't using a layer.
		std::string GetAnimationLayerName(const uint32 animationOrder) const
		{
			return AnimationsToPlay[animationOrder].HaveLayers && AnimationsToPlay[animationOrder].Layer
				? AnimationsToPlay[animationOrder].Layer->Name : std::string();
		}

		// Real enumeration of Layers (CreateLayer/AddBone* only ever let
		// you write layer state, never read it back). Iterates the
		// internal map in a stable (key-sorted) order - `index` is that
		// enumeration position, not the layer's own ID.
		uint32 GetNumberLayers() const { return (uint32)Layers.size(); }
		std::string GetLayerName(const uint32 index) const;
		// Bone IDs actually affected by (added to) this layer - i.e.
		// entries in the layer's internal per-bone flag array equal to 1
		// (see AddBone()'s implementation). Resolve to bone names via the
		// owning RenderingComponent's GetSkeleton() before calling
		// AddBone() again on load - IDs are only meaningful relative to
		// the same skeleton.
		std::vector<int32> GetLayerAffectedBoneIDs(const uint32 index) const;

		// The SkeletonAnimation asset this instance plays animations
		// from - needed to recover its source path (GetOwner()->GetPath())
		// for serialization; Owner was already stored, just never exposed.
		SkeletonAnimation* GetOwner() const { return Owner; }

		// ---- Direct pose access (editor / IK / ragdoll) --------------
		// Everything below reads or writes the same per-bone local
		// transform array Update() drives (boneTransformation), so a
		// caller can pose a skeleton without any clip playing at all -
		// which is exactly what the Animation Editor does when it scrubs
		// or when the user drags a bone gizmo. None of it existed before:
		// the only way to move a bone was to Play() a clip and let
		// Update() sample it, and the resulting matrices were all
		// protected.

		// Number of bones, i.e. the valid range of every `boneId` below.
		uint32 GetNumberBones() const { return (uint32)skeleton.size(); }
		// Bone records in id order (skeleton[i].self == i), so a caller can
		// walk names/parents to draw or list the hierarchy. The
		// RenderingComponent's own GetSkeleton() is a map keyed by StringID
		// and is not ordered by bone id, which makes it useless for
		// parent-child traversal without rebuilding this exact vector.
		const std::vector<Bone> &GetSkeletonBones() const { return skeleton; }
		// Bone's local bind transform (Bone::bindPoseMat), the neutral pose
		// a clip's keys replace.
		const Matrix &GetBindPoseLocal(const int32 boneId) const { return bindPose[boneId]; }
		// Current local transform of one bone - what a keyframe stores.
		const Matrix &GetBoneLocalTransform(const int32 boneId) const { return boneTransformation[boneId]; }
		// Current *model space* transform (local composed with every
		// parent), which is what you need to draw a bone or place a gizmo
		// on it. Multiply by the owning GameObject's world matrix for world
		// space. GetBoneMatrix() computed this already but was protected.
		const Matrix &GetBoneGlobalTransform(const int32 boneId) const { return Bones[boneId]; }

		// Writes one bone's local transform. Does NOT recompute the
		// hierarchy or the skinning matrices - call RefreshSkinning() once
		// after a batch of these, rather than paying for a full skeleton
		// walk per bone while dragging a gizmo.
		void SetBoneLocalTransform(const int32 boneId, const Matrix &local) { boneTransformation[boneId] = local; }
		// Whole-pose version; `localTransforms` must be GetNumberBones()
		// long. Recomputes and uploads (no separate RefreshSkinning()
		// needed).
		void ApplyPose(const std::vector<Matrix> &localTransforms);
		// Copies the current pose out, sized GetNumberBones().
		void CapturePose(std::vector<Matrix> &out) const { out = boneTransformation; }
		// Re-composes Bones[] from boneTransformation[] through the parent
		// chain and pushes the result into every mesh's SkinningBones -
		// the tail of Update(), factored out so a poser can reuse it.
		void RefreshSkinning();
		// The first half of RefreshSkinning only: recomposes the model-space
		// Bones[] without touching the meshes' SkinningBones. An IK solver
		// rotates one bone, reads the resulting model-space position of the
		// next, and repeats - so it needs the hierarchy recomposed many times
		// per solve but the skinning matrices uploaded only once at the end.
		void RefreshHierarchy();

		// Drive one bone from a WORLD-space transform - a physics body, a
		// tracked prop, anything outside the skeleton's own space.
		//
		// This is what a ragdoll needs and what nothing could express: the
		// pose API is all bone-local, and a caller holding a rigid body knows
		// only where it is in the world. Converting means going through the
		// owning GameObject's world matrix (which carries the model's scale -
		// a 0.1-scaled character is the normal case) and then through the
		// bone's parent chain, and getting either wrong silently produces a
		// mesh folded inside out rather than an error.
		//
		// Recomposes the hierarchy so a following call reads correct parent
		// transforms; call RefreshSkinning() once after the batch to upload.
		// Returns false for an unknown bone or a rig with no owner.
		// The pose it has, kept: while held, the clips that are playing do not
		// pose it (they keep their place and take it up again when it is let
		// go). For whatever poses a skeleton from outside and then stops - a
		// ragdoll come to rest: without this its bones had to be written again
		// every frame for ever, or the clip under them stood the body up.
		void SetPoseHeld(const bool held) { poseHeld = held; }
		bool IsPoseHeld() const { return poseHeld; }
		bool poseHeld = false;

		bool SetBoneWorldTransform(const int32 boneId, const Vec3 &worldPosition,
			const Quaternion &worldRotation);

		// ---- post-pose modifiers (runtime IK, ragdoll, look-at) -------
		// Called from inside SkeletonAnimation::Update(), after the playing
		// clips have written the pose and the hierarchy has been composed,
		// but BEFORE the skinning matrices are uploaded. So a modifier sees
		// final model-space bone transforms and anything it writes is what
		// gets drawn.
		//
		// This exists because ordering here cannot be left to convention.
		// Update() overwrites the whole pose every frame, so a solver that
		// runs from an ordinary component tick is correct or not depending
		// on whether that component happens to update after the script that
		// calls Update() - which nothing enforces and which changes when
		// objects are reordered. Hanging the solver off Update() itself
		// makes the order structural: there is no way to run it too early.
		typedef void(*PoseModifier)(SkeletonAnimationInstance* instance, void* userData);
		// userData identifies the registration; adding twice with the same
		// userData replaces rather than duplicates.
		void AddPoseModifier(PoseModifier fn, void* userData);
		void RemovePoseModifier(void* userData);
		// Runs every registered modifier then recomposes. Called by Update();
		// exposed so a caller posing by hand can get the same treatment.
		void RunPoseModifiers();
		bool HasPoseModifiers() const { return !poseModifiers.empty(); }
		// A rotation laid over one bone AFTER the clips have posed it, about
		// the bone's own joint and in MODEL space - "turn the head 30 degrees
		// about the model's up axis", whatever the clip has the neck doing.
		// It is how a character looks where the player is looking: the walk
		// cycle keeps the body, and the head, neck and spine each take a share
		// of the view's yaw and pitch on top of it.
		//
		// Applied by Update() every frame, between the clips and the pose
		// modifiers, in the order the bones were given - give parents before
		// children. It is NOT applied while nothing is playing: with no clip
		// to put the bone back first, it would wind round a little more each
		// frame.
		void SetBoneAim(const int32 boneId, const Quaternion &rotation);
		void ClearBoneAim(const int32 boneId);
		void ApplyBoneAims();
		// Scale what is skinned to a bone AND everything below it, about the
		// bone's joint, without touching the pose: the joints stay where the
		// clip put them (so anything attached to them does too), only the
		// skin is drawn smaller or larger. Near zero it takes a part out of
		// the picture - the head of a body seen through its own eyes, a limb
		// that has come off. Stays until cleared.
		void SetBoneScale(const int32 boneId, const Vec3 &scale);
		// The same, but only for what is drawn to be looked at: in shadow maps
		// the bone keeps its size. A head scaled away so that the camera inside
		// it sees out is still on the shoulders of the shadow.
		void SetBoneScaleInView(const int32 boneId, const Vec3 &scale);
		// And for the view only, the skin of this ONE bone - what hangs below
		// it keeps its size. A chest scaled away from round a camera that sits
		// in it leaves the arms that grow from it where they are.
		void SetBoneSkinScaleInView(const int32 boneId, const Vec3 &scale);
		void ClearBoneScale(const int32 boneId);
		// Puts every bone back to its bind transform and uploads. This IS
		// what "no animation playing" now looks like: the constructor seeds
		// boneTransformation from bindPose, Stop()/StopAnimation() return to
		// it, and SkeletonAnimation::Update() skips instances with nothing
		// playing rather than rewriting them to identity. (It used to do all
		// three at identity, which rendered a skinned mesh through a bare
		// inverse-bind matrix and silently stomped externally posed bones.)
		void ResetToBindPose();
		// Poses the skeleton from `anim` sampled at `time` seconds and
		// uploads, without touching playback state (no Play(), no clock,
		// no AnimationsToPlay entry). Bones the clip has no channel for are
		// left at identity, matching what a lone Play() of that clip would
		// show. This is the editor's scrub.
		void ApplyAnimationAtTime(const Animation &anim, const f32 time);

		// Local transform a single channel produces at `time`, i.e. one
		// bone's keyframe interpolation. Extracted from
		// SkeletonAnimation::Update()'s inner loop (which now calls it) so
		// the editor's scrubbing and the runtime's playback can never drift
		// apart. Empty key lists yield the identity component rather than
		// indexing out of bounds - authored clips routinely carry channels
		// with rotation keys and nothing else.
		//
		// Each key's InterpolationMode reshapes the span parameter, so
		// rotations still Slerp along the shortest arc and only their speed
		// along it changes. `applyScale` comes from the owning clip's
		// ANIM_FLAG_APPLY_SCALE - pass Animation::HasFlag() rather than a
		// literal, or a clip will sample differently here than it plays.
		// `bindLocal`, when given, supplies any component the channel does not
		// key. A channel is not required to carry all three: the Assimp
		// importer writes every component, but an editor-authored one carries
		// only what the animator actually keyed - and for a 2D cutout that is
		// rotation alone, because a limb's offset from its joint is the
		// character's SHAPE, not its animation.
		//
		// Without this a rotation-only channel sampled to translation (0,0,0)
		// and the bone collapsed onto its parent's origin - an arm animated
		// correctly but drawn growing out of the middle of the chest. NULL
		// keeps the old behaviour (identity for anything unkeyed) for any
		// caller that has no bind pose to hand.
		static Matrix SampleChannel(const Channel &ch, const f32 time, const bool applyScale = false,
			const Matrix* bindLocal = NULL);

		// Layers
		uint32 CreateLayer(const std::string &name);
		// Add Bone
		void AddBone(const uint32 LayerID, const std::string &bone);
		void AddBone(const std::string &LayerName, const std::string &bone);
		// Add Bone and Childs
		void AddBoneAndChilds(const uint32 LayerID, const std::string &bone, bool inclusive = true);
		void AddBoneAndChilds(const std::string &LayerName, const std::string &bone, bool inclusive = true);
		// Remove Bone
		void RemoveBone(const uint32 LayerID, const std::string &bone);
		void RemoveBone(const std::string &LayerName, const std::string &bone);
		// Remove Bone And Childs
		void RemoveBoneAndChilds(const uint32 LayerID, const std::string &bone);
		void RemoveBoneAndChilds(const std::string &LayerName, const std::string &bone);
		// Destroy Layer
		void DestroyLayer(const uint32 LayerID);
		void DestroyLayer(const std::string &LayerName);

	protected:

		// Keep Rendering Component PTR
		RenderingComponent* rcomp;

		// Keep Owner PTR
		SkeletonAnimation* Owner;

		// Animations Playing
		std::vector<_SkeletonAnimation::SkeletonAnimation> AnimationsToPlay;

		// Skeleton info and Shit
		std::vector<Matrix> bindPose;
		std::vector<Matrix> boneTransformation;
		std::vector<Matrix> Bones;
		std::vector<Bone> skeleton;

		Matrix GetParentMatrix(const int32 id, const std::vector<Matrix> &bones);
		Matrix GetBoneMatrix(const int32 id);

		bool _paused;

		// Layers
		std::map<uint32, _SkeletonAnimation::AnimationLayer*> Layers;

	private:
		// Get Bones
		void GetBoneChilds(std::vector<int32> &boneIDs, const std::vector<Bone> &Skeleton, const uint32 id, bool add = true);

		// Have Layers
		bool HaveLayers;

		// Default Bones Affected
		std::vector<int32> boneIDs;

		// Post-pose modifiers, in registration order.
		struct PoseModifierEntry {
			PoseModifier fn;
			void* userData;
		};
		std::vector<PoseModifierEntry> poseModifiers;

		// Per-bone aim rotations, in registration order. See SetBoneAim.
		struct BoneAim {
			int32 bone;
			Quaternion rotation;
		};
		std::vector<BoneAim> boneAims;

		// See SetBoneScale.
		struct BoneScale {
			int32 bone;
			Vec3 scale;
			bool viewOnly = false;
			bool alone = false;       // this bone's own skin, not that of the bones below it
		};
		std::vector<BoneScale> boneScales;

		// See SetFootGround.
		struct FootGround {
			int32 upper, foot;
			f32 height;          // along `up`
			Vec3 normal;
			Vec3 up;             // model space
			f32 weight;
		};
		mutable std::vector<Vec3> bindAt;    // bind pose, model space (GetModelUp)
		mutable int modelUpAxis = -1;
		mutable f32 modelUpMean = 0.f, modelUpSign = 1.f;
		std::vector<FootGround> footGrounds;
		f32 footMaxDrop = 0.45f;
		// Bones -> every submesh's skinning matrices, with the scales applied.
		void UploadSkinning();

		// Crossfade/events/root motion/tracks state - see the public API.
		std::vector<FiredEvent> pendingEvents;     // written by the update
		std::vector<FiredEvent> eventQueue;        // main thread, TakeEvents
		std::function<void(const FiredEvent&)> eventCallback;
		Vec3 rootMotionPending;                    // written by the update
		Vec3 rootMotionAccum;                      // main thread, Consume
		bool applyRootMotion = false;
		std::map<std::string, TrackValue> trackValues;
		void SampleTracks();
		void AdvanceFades(const f32 time);
		void CollectEventsAndRootMotion(_SkeletonAnimation::SkeletonAnimation &entry,
			const f32 localTime, const bool visible, const f32 weight);

	};

	class PYROS3D_API SkeletonAnimation
	{

		friend class SkeletonAnimationInstance;

	private:

		// internal timer
		f32 timer;
		// Instances
		std::vector<SkeletonAnimationInstance*> Instances;
		// Animations
		std::vector<Animation> animations;
		// Animation Loader
		AnimationLoader* animationLoader;

		Matrix SCALE(const Matrix &in, const Matrix &prev, const f32 s);

	public:

		// Constructor
		SkeletonAnimation() {}

		// Destructor
		virtual ~SkeletonAnimation();

		// Loads and Adds Animations to List
		void LoadAnimation(const std::string& AnimationFile);

		// Replaces the clip list with one held in memory. LoadAnimation() was
		// the only way to populate a SkeletonAnimation, which makes it
		// impossible to play clips that are not (yet) a file on disk - the
		// case for anything being authored in the editor, where the whole
		// point is to see unsaved edits play. Paths are cleared, since these
		// clips did not come from one; a caller that wants the serializer to
		// be able to rebuild the container must save first.
		//
		// Stops every instance before swapping: Play() stores a raw
		// Animation* into `animations`, so reallocating that vector under a
		// playing instance leaves dangling pointers - the same hazard
		// Play()'s own range check guards against.
		void SetAnimations(const std::vector<Animation> &clips);

		// Get Number of Animations
		const uint32 GetNumberAnimations() const;

		// Get Access to Animations Available
		const std::vector<Animation> GetAnimations() const;

		// Void Update
		void Update(const f32 time);
		// One instance only - what a RenderingComponent drives each frame.
		// Update() advanced every instance, and every component sharing
		// this animation called it: N components cost N x N.
		void UpdateInstance(SkeletonAnimationInstance* instance, const f32 time);

		// Instance
		SkeletonAnimationInstance* CreateInstance(RenderingComponent* Component);

		// Destroy Instance
		void DestroyInstance(SkeletonAnimationInstance* Instance);

		// Get Animation ID By Name
		const int32 GetAnimationIDByName(const std::string &name) const;

		// An event added to a clip at run time, kept in time order (a game
		// marking its own footsteps, so a rebuilt clip file needs no editing).
		// False when the clip already has that name at that time - one set of
		// clips is shared by everybody who loads the file - or there is no
		// such clip. Add before the clip plays: playback reads the list on the
		// update's thread.
		bool AddEvent(const uint32 animation, const f32 time, const std::string &name);

		// Stable identity of a clip, as stored in the .p3da. Empty for clips
		// that came from a v0 file and have not been re-saved since.
		const int32 GetAnimationIDByGuid(const std::string &guid) const;
		const std::string &GetAnimationGuid(const uint32 id) const;
		const std::string &GetAnimationName(const uint32 id) const;

		// Resolves a saved reference to a clip back to a runtime id, most
		// stable key first: guid, then name, then the raw index.
		//
		// The index alone used to be the ONLY key, and a clip's index is just
		// its position in this vector - so deleting a clip renumbered every
		// later one and any scene that had saved "play clip 2" silently
		// started playing a different animation. The fallbacks are what keeps
		// scenes saved before guids existed working: those have no guid and
		// no name recorded, and land on the index exactly as before.
		//
		// Returns -1 if nothing matches, which callers must treat as "do not
		// play" rather than passing on to Play().
		const int32 ResolveAnimationID(const std::string &guid, const std::string &name, const int32 fallbackIndex) const;

		// Source file path - not stored before this, LoadAnimation()'s
		// argument was used once then discarded. This is the path of the
		// *last* LoadAnimation() call only; one SkeletonAnimation routinely
		// holds several files' clips concatenated into `animations` (the
		// Skeleton Animation demo loads walk/alert/run/walk to get ids 0-3),
		// so use GetPaths() for anything that needs to rebuild it.
		const std::string &GetPath() const { return Path; }

		// Every LoadAnimation() path, in call order - the order *is* the
		// data, since a clip's id is just its index in the concatenated
		// `animations` vector. Serializing only GetPath() and reloading it
		// once (as SceneSerializer did) rebuilt a 1-clip animation whose
		// saved Play(id) calls then indexed `animations` out of bounds,
		// storing dangling Animation* that crashed the next Update().
		const std::vector<std::string> &GetPaths() const { return Paths; }

	private:

		std::string Path;
		std::vector<std::string> Paths;
	};

}
#endif  /* SKELETONANIMATION_H */
