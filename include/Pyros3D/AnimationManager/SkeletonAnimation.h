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
		// Puts every bone back to its bind transform and uploads. Note this
		// is NOT the same as "no animation playing": an instance with no
		// clip playing sits at identity per bone, not at bindPose (see the
		// constructor), which is the pose the mesh was authored in only
		// when bindPoseMat is identity.
		void ResetToBindPose();
		// Poses the skeleton from `anim` sampled at `time` seconds and
		// uploads, without touching playback state (no Play(), no clock,
		// no AnimationsToPlay entry). Bones the clip has no channel for are
		// left at identity, matching what a lone Play() of that clip would
		// show. This is the editor's scrub.
		void ApplyAnimationAtTime(const Animation &anim, const f32 time);

		// Local transform a single channel produces at `time`, i.e. one
		// bone's keyframe interpolation. Extracted verbatim from
		// SkeletonAnimation::Update()'s inner loop (which now calls it) so
		// the editor's scrubbing and the runtime's playback can never drift
		// apart. Empty key lists yield the identity component rather than
		// indexing out of bounds - authored clips routinely carry channels
		// with rotation keys and nothing else.
		static Matrix SampleChannel(const Channel &ch, const f32 time);

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

		// Cache
		std::vector<int32> ChannelBoneIDCache;

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

		// Instance
		SkeletonAnimationInstance* CreateInstance(RenderingComponent* Component);

		// Destroy Instance
		void DestroyInstance(SkeletonAnimationInstance* Instance);

		// Get Animation ID By Name
		const int32 GetAnimationIDByName(const std::string &name) const;

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
