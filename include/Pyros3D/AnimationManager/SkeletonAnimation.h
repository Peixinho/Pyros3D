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
