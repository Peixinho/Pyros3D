//============================================================================
// Name        : SkeletonAnimation.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animator Interface
//============================================================================

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>
#include <iterator>

namespace p3d {

	SkeletonAnimationInstance::~SkeletonAnimationInstance()
	{
		for (std::map<uint32, _SkeletonAnimation::AnimationLayer*>::iterator i = Layers.begin(); i != Layers.end(); i++)
			delete (*i).second;
	}

	SkeletonAnimationInstance::SkeletonAnimationInstance(SkeletonAnimation* owner, RenderingComponent* Component)
	{
		// Keep Rendering Component
		rcomp = Component;
		Component->SetActiveSkeletonAnimation(this);

		// Resize Vectors
		skeleton.resize(Component->GetSkeleton().size());
		Bones.resize(skeleton.size());
		bindPose.resize(skeleton.size());
		boneTransformation.resize(skeleton.size());
		ChannelBoneIDCache = std::vector<int32>(skeleton.size(), -1);

		// Get Skeleton
		for (std::map<uint32, Bone>::const_iterator a = Component->GetSkeleton().begin(); a != Component->GetSkeleton().end(); a++)
		{
			// Set Bones Transformation based on Bone ID
			bindPose[(*a).second.self] = (*a).second.bindPoseMat; // Copy BindPose
			skeleton[(*a).second.self] = (*a).second;
		}

		// Start at the BIND pose, not at identity. boneTransformation is the
		// per-bone LOCAL transform array; leaving it default-constructed made
		// Bones[] identity, so every SkinningBones entry below came out as a
		// bare BoneOffsetMatrix - the inverse of the bone's global bind
		// transform - and the mesh rendered inside-out until something
		// happened to call Play(). The bind local composed up the parent chain
		// is exactly the inverse of BoneOffsetMatrix, so seeding it here makes
		// the product identity and the rig sits in the pose it was modelled
		// in, which is what "no animation playing" should look like.
		boneTransformation = bindPose;

		// Multiply bones with its parent
		for (std::vector<Bone>::iterator a = skeleton.begin(); a != skeleton.end(); a++)
		{
			Bones[(*a).self] = GetParentMatrix((*a).parent, boneTransformation) * boneTransformation[(*a).self];
		}

		// Send SubMesh Bones to Material
		for (std::vector<RenderingMesh*>::iterator j = rcomp->GetMeshes().begin(); j != rcomp->GetMeshes().end(); j++)
		{
			(*j)->SkinningBones = std::vector<Matrix>((*j)->MapBoneIDs.size());
			for (std::map<int32, int32>::iterator k = (*j)->MapBoneIDs.begin(); k != (*j)->MapBoneIDs.end(); k++)
			{
				// Set list of Bones Matrices
				(*j)->SkinningBones[(*k).second] = Bones[(*k).first] * (*j)->BoneOffsetMatrix[(*k).first];
			}
		}

		// Owner
		Owner = owner;

		_paused = false;

		HaveLayers = false;

		// Set Default Affected Bones
		boneIDs.resize(skeleton.size());
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
			boneIDs[(*i).self] = 1;
	}

	int32 SkeletonAnimationInstance::Play(const uint32 animation, const f32 startTime, const f32 repetition, const f32 speed, const f32 scale, const std::string &LayerName)
	{
		_SkeletonAnimation::SkeletonAnimation Anim;

		// Anim.animation below is a raw pointer *into* Owner->animations, so
		// an out-of-range id doesn't fail here - it silently stores a wild
		// pointer that only blows up later, in Update()'s `Animation Anim =
		// *(*_Anim).animation;`, with a backtrace pointing at the copy
		// constructor rather than at whoever asked for a clip that doesn't
		// exist. Reject it up front and say so.
		if (animation >= Owner->GetNumberAnimations())
		{
			echo("ERROR: SkeletonAnimationInstance::Play - animation id out of range (asked for " + std::to_string(animation) + ", only " + std::to_string(Owner->GetNumberAnimations()) + " loaded)");
			return -1;
		}

		if (GetAnimationPositionInVector(animation) == -1)
		{
			Anim.ID = animation;
			Anim.startTime = startTime; // 0-1
			Anim._startTime = startTime*Owner->animations[animation].Duration; // RealTime
			Anim.animation = &Owner->animations[animation];
			Anim.speed = speed;
			Anim.scale = scale;
			Anim._startTimeClock = -1.f;
			Anim._isPaused = false;
			Anim._resumed = false;
			Anim._pauseStart = -1.f;
			Anim._pauseTime = 0.f;
			Anim._repetition = repetition;
			Anim._currentTime = 0.f;
			Anim.boneTransformationPerAnimation = bindPose;

			// Layers
			if (LayerName.size() > 0)
			{
				HaveLayers = true;

				uint32 LayerID = MakeStringID(LayerName);
				Anim.HaveLayers = true;
				Anim.LayerID = LayerID;
				Anim.Layer = Layers[LayerID];

				// Mark Layer as being used
				Anim.Layer->usingLayer++;

				// Remove this Affected Bones From Other Animations
				for (std::vector<int32>::iterator i = Anim.Layer->boneIDs.begin(); i != Anim.Layer->boneIDs.end(); i++)
				{
					// Set to -1
					if ((*i) == 1)
						boneIDs[i - Anim.Layer->boneIDs.begin()] = -1;
				}

			}
			else {
				Anim.HaveLayers = false;
				Anim.Layer = NULL;
				Anim.LayerID = 0;
			}

			// Add Animation to Queue
			AnimationsToPlay.push_back(Anim);

			return AnimationsToPlay.size() - 1; // Return Order
		}
		return -1; // Already Exists
	}
	void SkeletonAnimationInstance::ChangeProperties(const uint32 animationOrder, const f32 startTime, const f32 repetition, const f32 speed, const f32 scale)
	{
		if (AnimationsToPlay.size() > animationOrder)
		{
			_SkeletonAnimation::SkeletonAnimation *Anim = &AnimationsToPlay[animationOrder];
			Anim->startTime = startTime; // 0-1
			Anim->_startTime = startTime*Owner->animations[Anim->ID].Duration; // RealTime
			Anim->speed = speed;
			Anim->scale = scale;
			if (!Anim->_isPaused)
			{
				Anim->_startTimeClock = -1.f;
				Anim->_isPaused = false;
				Anim->_resumed = false;
				Anim->_pauseStart = -1.f;
				Anim->_pauseTime = 0.f;
			}
			Anim->_repetition = repetition;
		}
		else echo("ERROR: Animation Not Found");
	}
	void SkeletonAnimationInstance::StopAnimation(const uint32 animationOrder)
	{
		if (animationOrder >= AnimationsToPlay.size()) return;

		// Play() leaves Layer NULL for a clip started without a layer name,
		// which is the common case - dereferencing it unconditionally here
		// crashed on stopping any ordinary clip.
		_SkeletonAnimation::AnimationLayer* Layer = AnimationsToPlay[animationOrder].Layer;
		if (Layer)
		{
			// Mark Layer as NOT being used by this
			Layer->usingLayer--;

			// Insert Removed Bones if there isn't any Layer
			if (Layer->usingLayer == 0)
			{
				for (std::vector<int32>::iterator i = Layer->boneIDs.begin(); i != Layer->boneIDs.end(); i++)
					if ((*i) == 1)
						boneIDs[i - Layer->boneIDs.begin()] = 1;
			}
		}

		// Remove Layer if Any
		AnimationsToPlay.erase(AnimationsToPlay.begin() + animationOrder);

		// Same reasoning as Stop(): Update() skips an instance with nothing
		// left playing, so the last clip's final pose would stick otherwise.
		if (AnimationsToPlay.empty())
			ResetToBindPose();
	}

	void SkeletonAnimationInstance::Stop()
	{
		AnimationsToPlay.clear();
		// Update() no longer touches an instance with nothing playing, so
		// without this the rig would freeze in whatever pose the last clip
		// left it in. "Stopped" should read as neutral, and neutral is the
		// bind pose - the same thing a freshly constructed instance shows.
		ResetToBindPose();
	}

	void SkeletonAnimationInstance::PauseAnimation(const uint32 animationOrder)
	{
		if (!AnimationsToPlay[animationOrder]._isPaused)
			AnimationsToPlay[animationOrder]._isPaused = true;
	}

	void SkeletonAnimationInstance::Pause()
	{
		if (!_paused)
		{
			for (std::vector<_SkeletonAnimation::SkeletonAnimation>::iterator _Anim = AnimationsToPlay.begin(); _Anim != AnimationsToPlay.end(); _Anim++)
			{
				if (!(*_Anim)._isPaused)
					(*_Anim)._isPaused = true;
			}
			_paused = true;
		}
	}

	void SkeletonAnimationInstance::ResumeAnimation(const uint32 animationOrder)
	{
		if (AnimationsToPlay[animationOrder]._isPaused)
		{
			AnimationsToPlay[animationOrder]._isPaused = false;
			AnimationsToPlay[animationOrder]._resumed = true;
		}
	}

	void SkeletonAnimationInstance::Resume()
	{
		if (_paused)
		{
			for (std::vector<_SkeletonAnimation::SkeletonAnimation>::iterator _Anim = AnimationsToPlay.begin(); _Anim != AnimationsToPlay.end(); _Anim++)
			{
				if ((*_Anim)._isPaused)
				{
					(*_Anim)._isPaused = false;
					(*_Anim)._resumed = true;
				}
			}
			_paused = false;
		}
	}

	bool SkeletonAnimationInstance::IsPaused(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder]._isPaused;
	}

	bool SkeletonAnimationInstance::IsPaused()
	{
		return _paused;
	}

	f32 SkeletonAnimationInstance::GetAnimationCurrentTime(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder]._currentTime;
	}
	f32 SkeletonAnimationInstance::GetAnimationCurrentProgress(const uint32 animationOrder)
	{
		if (AnimationsToPlay[animationOrder]._currentTime > 0)
			return AnimationsToPlay[animationOrder]._currentTime / GetAnimationDuration(animationOrder);
		else return 0;
	}
	f32 SkeletonAnimationInstance::GetAnimationDuration(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].animation->Duration;
	}
	f32 SkeletonAnimationInstance::GetAnimationSpeed(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].speed;
	}
	f32 SkeletonAnimationInstance::GetAnimationStartTimeProgress(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].startTime;
	}
	f32 SkeletonAnimationInstance::GetAnimationStartTime(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder]._startTime;
	}
	uint32 SkeletonAnimationInstance::GetAnimationID(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].ID;
	}
	f32 SkeletonAnimationInstance::GetAnimationScale(const uint32 animationOrder)
	{
		return AnimationsToPlay[animationOrder].scale;
	}

	std::string SkeletonAnimationInstance::GetLayerName(const uint32 index) const
	{
		std::map<uint32, _SkeletonAnimation::AnimationLayer*>::const_iterator it = Layers.begin();
		std::advance(it, index);
		return it != Layers.end() ? it->second->Name : std::string();
	}
	std::vector<int32> SkeletonAnimationInstance::GetLayerAffectedBoneIDs(const uint32 index) const
	{
		std::vector<int32> result;
		std::map<uint32, _SkeletonAnimation::AnimationLayer*>::const_iterator it = Layers.begin();
		std::advance(it, index);
		if (it == Layers.end()) return result;
		const std::vector<int32> &boneIDs = it->second->boneIDs;
		for (size_t i = 0; i < boneIDs.size(); i++)
			if (boneIDs[i] == 1) result.push_back((int32)i);
		return result;
	}

	int32 SkeletonAnimationInstance::GetAnimationPositionInVector(const uint32 animation)
	{
		for (uint32 i = 0; i < AnimationsToPlay.size(); i++)
		{
			if (AnimationsToPlay[i].ID == animation)
				return i;
		}
		return -1; // Not Found
	}

	uint32 SkeletonAnimationInstance::CreateLayer(const std::string &name)
	{
		uint32 id = MakeStringID(name);
		if (Layers.find(id) == Layers.end())
		{
			Layers[id] = new _SkeletonAnimation::AnimationLayer(name);
			Layers[id]->boneIDs.resize(skeleton.size());
		}
		return id;
	}

	void SkeletonAnimationInstance::AddBone(const uint32 LayerID, const std::string &bone)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).name.compare(bone) == 0)
			{
				Layers[LayerID]->boneIDs[(*i).self] = 1;
				break;
			}
		}
	}
	void SkeletonAnimationInstance::AddBone(const std::string &LayerName, const std::string &bone)
	{
		AddBone(MakeStringID(LayerName), bone);
	}
	void SkeletonAnimationInstance::AddBoneAndChilds(const uint32 LayerID, const std::string &bone, bool inclusive)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).name.compare(bone) == 0)
			{
				if (inclusive)
					Layers[LayerID]->boneIDs[(*i).self] = 1;
				GetBoneChilds(Layers[LayerID]->boneIDs, skeleton, (*i).self, true);
				break;
			}
		}
	}
	void SkeletonAnimationInstance::AddBoneAndChilds(const std::string &LayerName, const std::string &bone, bool inclusive)
	{
		AddBoneAndChilds(MakeStringID(LayerName), bone, inclusive);
	}
	void SkeletonAnimationInstance::RemoveBone(const uint32 LayerID, const std::string &bone)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).name.compare(bone) == 0)
			{
				Layers[LayerID]->boneIDs[(*i).self] = -1;
				break;
			}
		}
	}
	void SkeletonAnimationInstance::RemoveBone(const std::string &LayerName, const std::string &bone)
	{
		RemoveBone(MakeStringID(LayerName), bone);
	}
	void SkeletonAnimationInstance::RemoveBoneAndChilds(const uint32 LayerID, const std::string &bone)
	{
		GetBoneChilds(Layers[LayerID]->boneIDs, skeleton, skeleton[MakeStringID(bone)].self, false);
	}
	void SkeletonAnimationInstance::RemoveBoneAndChilds(const std::string &LayerName, const std::string &bone)
	{
		RemoveBoneAndChilds(MakeStringID(LayerName), bone);
	}
	void SkeletonAnimationInstance::DestroyLayer(const uint32 LayerID)
	{
		// Delete Layer
		delete Layers[LayerID];
	}
	void SkeletonAnimationInstance::DestroyLayer(const std::string &LayerName)
	{
		DestroyLayer(MakeStringID(LayerName));
	}

	void SkeletonAnimationInstance::GetBoneChilds(std::vector<int32> &boneIDs, const std::vector<Bone> &Skeleton, const uint32 id, bool add)
	{
		for (std::vector<Bone>::iterator i = skeleton.begin(); i != skeleton.end(); i++)
		{
			if ((*i).parent == id)
			{
				if (add)
					boneIDs[(*i).self] = 1;
				else if (!add)
					boneIDs[(*i).self] = 1;

				GetBoneChilds(boneIDs, Skeleton, (*i).self);
			}
		}
	}

	void SkeletonAnimation::LoadAnimation(const std::string& AnimationFile)
	{
		// Not stored before this - the path was used once to load, then
		// discarded, same "no recoverable source" gap Model/Texture/Font
		// had before their own path tracking was added.
		Path = AnimationFile;
		// See GetPaths() - Path alone loses every file but the last.
		Paths.push_back(AnimationFile);

		// Loads Animation
		animationLoader = new AnimationLoader();
		animationLoader->Load(AnimationFile);

		// Copy Animations
		for (std::vector<Animation>::iterator i = animationLoader->animations.begin(); i != animationLoader->animations.end(); i++)
		{
			animations.push_back((*i));
		}

		// Deletes Animation Loader
		delete animationLoader;
	}

	const uint32 SkeletonAnimation::GetNumberAnimations() const
	{
		// Returns number of loaded animations
		return animations.size();
	}

	const int32 SkeletonAnimation::GetAnimationIDByName(const std::string &name) const
	{
		for (uint32 i = 0; i < animations.size(); i++)
		{
			if (animations[i].AnimationName.compare(name) == 0) return i;
		}
		return -1;
	}

	const int32 SkeletonAnimation::GetAnimationIDByGuid(const std::string &guid) const
	{
		// An empty guid is "no identity recorded", not a clip to match - v0
		// clips all carry one, so matching on it would return clip 0 for
		// every lookup.
		if (guid.empty()) return -1;
		for (uint32 i = 0; i < animations.size(); i++)
		{
			if (animations[i].Guid.compare(guid) == 0) return i;
		}
		return -1;
	}

	const std::string &SkeletonAnimation::GetAnimationGuid(const uint32 id) const
	{
		static const std::string empty;
		return (id < animations.size() ? animations[id].Guid : empty);
	}

	const std::string &SkeletonAnimation::GetAnimationName(const uint32 id) const
	{
		static const std::string empty;
		return (id < animations.size() ? animations[id].AnimationName : empty);
	}

	const int32 SkeletonAnimation::ResolveAnimationID(const std::string &guid, const std::string &name, const int32 fallbackIndex) const
	{
		const int32 byGuid = GetAnimationIDByGuid(guid);
		if (byGuid >= 0) return byGuid;

		if (!name.empty())
		{
			const int32 byName = GetAnimationIDByName(name);
			if (byName >= 0) return byName;
		}

		if (fallbackIndex >= 0 && (uint32)fallbackIndex < animations.size())
			return fallbackIndex;

		return -1;
	}

	SkeletonAnimationInstance* SkeletonAnimation::CreateInstance(RenderingComponent* Component)
	{
		SkeletonAnimationInstance* i = new SkeletonAnimationInstance(this, Component);
		Instances.push_back(i);

		return i;
	}

	// Destroy Instance
	void SkeletonAnimation::DestroyInstance(SkeletonAnimationInstance* Instance)
	{
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
		{
			if ((*i) == Instance)
			{
				delete Instance;
				Instances.erase(i);
				break;
			}
		}
	}

	void SkeletonAnimation::Update(const f32 time)
	{
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
		{
			// Nothing playing and nothing to modify: leave the pose alone.
			//
			// The "Multiply Bones" loop below derives each bone's local
			// transform from the playing list, so with an empty list it fell
			// through to `trafo = Matrix()` and rewrote every bone to IDENTITY
			// on every single frame. That did two bad things: it collapsed a
			// skinned mesh that simply had no clip playing (identity local
			// instead of the bind local), and it stomped any pose written from
			// outside - SetBoneLocalTransform/ApplyPose - the very next tick,
			// which is why an IK solver or the editor's posing could never
			// hold on an instance owned by a running scene.
			//
			// Pose modifiers still run with nothing playing, though: a
			// character standing on uneven ground wants its feet planted
			// whether or not a clip happens to be driving it.
			if ((*i)->AnimationsToPlay.empty())
			{
				if (!(*i)->poseModifiers.empty())
				{
					(*i)->RunPoseModifiers();
					(*i)->RefreshSkinning();
				}
				continue;
			}

			for (std::vector<_SkeletonAnimation::SkeletonAnimation>::iterator _Anim = (*i)->AnimationsToPlay.begin(); _Anim != (*i)->AnimationsToPlay.end(); _Anim++)
			{
				if ((*_Anim)._isPaused)
				{
					if ((*_Anim)._pauseStart == -1)
						(*_Anim)._pauseStart = time;
				}
				else {

					if ((*_Anim)._resumed)
					{
						(*_Anim)._resumed = false;
						(*_Anim)._pauseTime += time - (*_Anim)._pauseStart;
						(*_Anim)._pauseStart = -1;
					}

					Animation Anim = *(*_Anim).animation;

					if ((*_Anim)._startTimeClock == -1.f)
					{
						(*_Anim)._startTimeClock = time;
						if ((*_Anim).speed < 0 && (*_Anim)._startTime == 0)
							(*_Anim)._startTime = (*_Anim).animation->Duration;
					}

					// Calculate Current Time
					f32 currentTime = time - (*_Anim)._startTimeClock - (*_Anim)._pauseTime + (*_Anim)._startTime / (*_Anim).speed;

					// Check if Ended
					if (
						(currentTime*(*_Anim).speed>0 && currentTime*(*_Anim).speed > (*_Anim).animation->Duration)
						||
						(currentTime*(*_Anim).speed < 0 && currentTime*(*_Anim).speed < 0)
						) // Ended
					{
						if (currentTime*(*_Anim).speed > 0)
						{
							if ((*_Anim)._repetition == -1)
							{
								(*_Anim).startTime = 0;
								(*_Anim)._startTimeClock = time;
								(*_Anim)._startTime = 0;
								(*_Anim)._pauseTime = 0;
							}
							else if ((*_Anim)._repetition > 0) {
								(*_Anim)._repetition--;
								if ((*_Anim)._repetition > 0)
								{
									(*_Anim).startTime = 0;
									(*_Anim)._startTimeClock = time;
									(*_Anim)._startTime = 0;
									(*_Anim)._pauseTime = 0;
								}
							}
						}
						else if (currentTime*(*_Anim).speed < 0)
						{
							if ((*_Anim)._repetition == -1)
							{
								(*_Anim).startTime = 1;
								(*_Anim)._startTimeClock = time;
								(*_Anim)._startTime = (*_Anim).animation->Duration;
								(*_Anim)._pauseTime = 0;
							}
							else if ((*_Anim)._repetition > 0) {
								(*_Anim)._repetition--;
								if ((*_Anim)._repetition > 0)
								{
									(*_Anim).startTime = 1;
									(*_Anim)._startTimeClock = time;
									(*_Anim)._startTime = (*_Anim).animation->Duration;
									(*_Anim)._pauseTime = 0;
								}
							}
						}
					}

					// Save Current Time to Animation Info
					(*_Anim)._currentTime = currentTime *= (*_Anim).speed;

					// Transform bones from animation
					for (uint32 a = 0; a<Anim.Channels.size(); a++)
					{
						// Interpolation moved to SampleChannel() so the
						// editor can evaluate a clip at an arbitrary time
						// (scrubbing a timeline) through the exact same code
						// the runtime plays it with.
						// The bone this channel drives has to be resolved
						// BEFORE sampling now, because its bind pose is what
						// supplies any component the channel does not key.
						if ((*i)->ChannelBoneIDCache[a] == -1)
						{
							for (std::vector<Bone>::iterator b = (*i)->skeleton.begin(); b != (*i)->skeleton.end(); b++)
							{
								if ((*b).name.compare(Anim.Channels[a].NodeName) == 0)
								{
									(*i)->ChannelBoneIDCache[a] = (*b).self;
									break;
								}
							}
						}

						// A channel naming a node that is not a bone of this
						// skeleton leaves the cache at -1, and indexing the
						// pose vector with that is an out-of-bounds *write*.
						// Assimp-exported clips routinely carry channels for
						// armature/helper nodes, and the Animation Editor
						// makes it easy to point a clip at a mesh it wasn't
						// authored for, so this has to be survivable rather
						// than merely unlikely.
						if ((*i)->ChannelBoneIDCache[a] >= 0)
						{
							const int32 bid = (*i)->ChannelBoneIDCache[a];
							const Matrix* bind = ((size_t)bid < (*i)->bindPose.size())
								? &(*i)->bindPose[bid] : NULL;
							(*_Anim).boneTransformationPerAnimation[bid] =
								SkeletonAnimationInstance::SampleChannel(Anim.Channels[a], currentTime,
									Anim.HasFlag(ANIM_FLAG_APPLY_SCALE), bind);
						}
					}
				}
			}

			// Multiply Bones
			for (std::vector<Bone>::iterator a = (*i)->skeleton.begin(); a != (*i)->skeleton.end(); a++)
			{
				Matrix trafo = ((*i)->AnimationsToPlay.size() > 1 ? (*i)->bindPose[(*a).self] : Matrix());
				for (std::vector<_SkeletonAnimation::SkeletonAnimation>::reverse_iterator b = (*i)->AnimationsToPlay.rbegin(); b != (*i)->AnimationsToPlay.rend(); b++)
				{
					// Blending
					if ((*i)->AnimationsToPlay.size() > 1)
					{
						// Regular Bleding Animations
						if (!(*b).HaveLayers)
						{
							if ((*i)->boneIDs[(*a).self] == 1)
								trafo = SCALE((*b).boneTransformationPerAnimation[(*a).self], trafo, (*b).scale);
						}

						// Layered
						else if ((*b).Layer->boneIDs[(*a).self] == 1)
						{
							if ((*b).Layer->usingLayer > 1)
							{
								trafo = SCALE((*b).boneTransformationPerAnimation[(*a).self], trafo, (*b).scale);
							}
							else
							{
								trafo = (*b).boneTransformationPerAnimation[(*a).self];
							}
						}
					}

					// Normal Playback of One Animation
					else {
						trafo = (*b).boneTransformationPerAnimation[(*a).self];
					}
				}
				// Apply Final Transformation to Bones
				(*i)->boneTransformation[(*a).self] = trafo;
			}

			// Multiply bones with its parent - Tree
			for (std::vector<Bone>::iterator a = (*i)->skeleton.begin(); a != (*i)->skeleton.end(); a++)
			{
				(*i)->Bones[(*a).self] = (*i)->GetParentMatrix((*a).parent, (*i)->boneTransformation) * (*i)->boneTransformation[(*a).self];
			}

			// Runtime IK and friends run HERE - after the clips have written
			// the pose and the hierarchy is composed, before the result is
			// uploaded. See AddPoseModifier: doing this from a component tick
			// instead would be correct only by accident of update order.
			(*i)->RunPoseModifiers();

			// Send SubMesh Bones to Material
			for (std::vector<RenderingMesh*>::iterator j = (*i)->rcomp->GetMeshes().begin(); j != (*i)->rcomp->GetMeshes().end(); j++)
			{
				for (std::map<int32, int32>::iterator k = (*j)->MapBoneIDs.begin(); k != (*j)->MapBoneIDs.end(); k++)
				{
					// Set list of Bones Matrices
					(*j)->SkinningBones[(*k).second] = ((*i)->Bones[(*k).first] * (*j)->BoneOffsetMatrix[(*k).first]);
				}
			}
		}
	}

	void SkeletonAnimation::SetAnimations(const std::vector<Animation> &clips)
	{
		// Stop first: every playing entry holds a raw Animation* into the
		// vector about to be replaced.
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
			(*i)->Stop();

		animations = clips;
		Path.clear();
		Paths.clear();
	}

	const std::vector<Animation> SkeletonAnimation::GetAnimations() const
	{
		return animations;
	}

	SkeletonAnimation::~SkeletonAnimation()
	{
		for (std::vector<SkeletonAnimationInstance*>::iterator i = Instances.begin(); i != Instances.end(); i++)
		{
			delete (*i);
		}
	}

	// Get Parent Matrix
	Matrix SkeletonAnimationInstance::GetParentMatrix(const int32 id, const std::vector<Matrix> &bones)
	{
		if (id != -1)
		{
			if (skeleton[id].parent > -1)
			{
				return GetParentMatrix(skeleton[id].parent, bones) * bones[id];
			}
			return bones[id];
		}
		else return Matrix();
	}

	Matrix SkeletonAnimationInstance::GetBoneMatrix(const int32 id)
	{
		return Bones[skeleton[id].self];
	}


	// ---- Direct pose access ------------------------------------------------

	namespace {
		// Index of the last key at or before `time`, for a key vector already
		// sorted by time. Linear scan, matching what this did inline before -
		// channels carry tens of keys, not thousands.
		template<typename T>
		size_t KeyIndexAt(const std::vector<T> &keys, const f32 time)
		{
			size_t index = 0;
			while (index + 1 < keys.size() && keys[index + 1].Time <= time)
				index++;
			return index;
		}

		// Normalised, mode-remapped position within the span starting at
		// `index`. Zero when `index` is the last key.
		template<typename T>
		f32 SpanDelta(const std::vector<T> &keys, const size_t index, const f32 time)
		{
			if (index + 1 >= keys.size()) return 0.f;
			const f32 span = keys[index + 1].Time - keys[index].Time;
			// Two keys at the same time is a divide by zero (NaN value, and
			// from there a NaN bone matrix that silently collapses the whole
			// mesh) - reachable in the editor by dropping a key onto an
			// existing one.
			const f32 t = (span > 0.f ? (time - keys[index].Time) / span : 0.f);
			return Ease(t, keys[index].Mode, keys[index].OutTangent, keys[index + 1].InTangent);
		}
	}

	Matrix SkeletonAnimationInstance::SampleChannel(const Channel &ch, const f32 time, const bool applyScale,
		const Matrix* bindLocal)
	{
		Vec3 curPosition, curScale(1.f, 1.f, 1.f);
		Quaternion curRotation;

		// Anything the channel does not key comes from the bind pose, not from
		// identity - see the header. Read up front so each block below only
		// has to overwrite what it actually has keys for.
		if (bindLocal)
		{
			Matrix b = *bindLocal;         // the accessors are non-const
			curPosition = b.GetTranslation();
			curRotation = b.ConvertToQuaternion();
			curScale = b.GetScale();
		}

		// Empty key lists were never possible for clips coming out of the
		// Assimp importer (it writes a key per channel per component), but
		// an editor-authored clip only carries the components the user
		// actually keyed, so each of the three blocks below has to cope
		// with having nothing to sample.
		if (!ch.positions.empty())
		{
			const size_t posIndex = KeyIndexAt(ch.positions, time);
			curPosition = ch.positions[posIndex].Pos;
			if (posIndex + 1 < ch.positions.size())
				curPosition = curPosition.Lerp(ch.positions[posIndex + 1].Pos, SpanDelta(ch.positions, posIndex, time));
		}

		if (!ch.rotations.empty())
		{
			const size_t rotIndex = KeyIndexAt(ch.rotations, time);
			curRotation = ch.rotations[rotIndex].Rot;
			if (rotIndex + 1 < ch.rotations.size())
				curRotation = curRotation.Slerp(ch.rotations[rotIndex + 1].Rot, SpanDelta(ch.rotations, rotIndex, time));
		}

		if (!ch.scales.empty())
		{
			const size_t scaleIndex = KeyIndexAt(ch.scales, time);
			curScale = ch.scales[scaleIndex].Scale;
			if (scaleIndex + 1 < ch.scales.size())
				curScale = curScale.Lerp(ch.scales[scaleIndex + 1].Scale, SpanDelta(ch.scales, scaleIndex, time));
		}

		Matrix trafo = curRotation.ConvertToMatrix();
		// Scale before the translation is written, since Matrix::Scale()
		// multiplies the diagonal (which would also scale a translation
		// already sitting in the last column) while Matrix::Translate()
		// overwrites that column outright.
		//
		// Off by default and opted into per clip via ANIM_FLAG_APPLY_SCALE.
		// Scale keys have always round-tripped through the file while the
		// sampler dropped them, so any clip authored since then may carry
		// scale keys nobody ever saw applied; turning this on globally would
		// change how those clips deform with no way to tell which.
		if (applyScale)
			trafo.Scale(curScale);
		trafo.Translate(curPosition);
		return trafo;
	}

	void SkeletonAnimationInstance::AddPoseModifier(PoseModifier fn, void* userData)
	{
		if (!fn) return;
		for (size_t i = 0; i < poseModifiers.size(); i++)
		{
			if (poseModifiers[i].userData == userData)
			{
				poseModifiers[i].fn = fn;
				return;
			}
		}
		PoseModifierEntry e;
		e.fn = fn;
		e.userData = userData;
		poseModifiers.push_back(e);
	}

	void SkeletonAnimationInstance::RemovePoseModifier(void* userData)
	{
		for (size_t i = 0; i < poseModifiers.size(); i++)
		{
			if (poseModifiers[i].userData == userData)
			{
				poseModifiers.erase(poseModifiers.begin() + i);
				return;
			}
		}
	}

	void SkeletonAnimationInstance::RunPoseModifiers()
	{
		if (poseModifiers.empty()) return;

		// Each modifier expects composed model-space transforms to read from,
		// and a previous modifier may have invalidated them.
		RefreshHierarchy();
		for (size_t i = 0; i < poseModifiers.size(); i++)
		{
			poseModifiers[i].fn(this, poseModifiers[i].userData);
			RefreshHierarchy();
		}
	}

	void SkeletonAnimationInstance::RefreshHierarchy()
	{
		// Multiply bones with its parent - Tree
		for (std::vector<Bone>::iterator a = skeleton.begin(); a != skeleton.end(); a++)
			Bones[(*a).self] = GetParentMatrix((*a).parent, boneTransformation) * boneTransformation[(*a).self];
	}

	void SkeletonAnimationInstance::RefreshSkinning()
	{
		RefreshHierarchy();

		// Send SubMesh Bones to Material
		for (std::vector<RenderingMesh*>::iterator j = rcomp->GetMeshes().begin(); j != rcomp->GetMeshes().end(); j++)
		{
			for (std::map<int32, int32>::iterator k = (*j)->MapBoneIDs.begin(); k != (*j)->MapBoneIDs.end(); k++)
				(*j)->SkinningBones[(*k).second] = (Bones[(*k).first] * (*j)->BoneOffsetMatrix[(*k).first]);
		}
	}

	void SkeletonAnimationInstance::ApplyPose(const std::vector<Matrix> &localTransforms)
	{
		if (localTransforms.size() != boneTransformation.size())
		{
			echo("ERROR: SkeletonAnimationInstance::ApplyPose - pose has " + std::to_string(localTransforms.size()) + " bones, skeleton has " + std::to_string(boneTransformation.size()));
			return;
		}
		boneTransformation = localTransforms;
		RefreshSkinning();
	}

	void SkeletonAnimationInstance::ResetToBindPose()
	{
		boneTransformation = bindPose;
		RefreshSkinning();
	}

	void SkeletonAnimationInstance::ApplyAnimationAtTime(const Animation &anim, const f32 time)
	{
		// Bones this clip has no channel for keep their BIND transform, not
		// identity - that is what a single Play() of the same clip produces,
		// because Play() seeds boneTransformationPerAnimation with bindPose
		// and Update() only overwrites the entries a channel actually
		// drives. Matching that exactly is the whole point: the editor's
		// scrub has to look like the runtime's playback, and seeding
		// identity here (as this first did) collapsed every unkeyed bone to
		// the origin - which reads as a folded-up heap for a fresh clip and
		// silently disagreed with what the game would show.
		std::vector<Matrix> pose = bindPose;

		for (uint32 a = 0; a < anim.Channels.size(); a++)
		{
			int32 boneId = -1;
			for (std::vector<Bone>::const_iterator b = skeleton.begin(); b != skeleton.end(); b++)
			{
				if ((*b).name.compare(anim.Channels[a].NodeName) == 0)
				{
					boneId = (*b).self;
					break;
				}
			}
			if (boneId < 0) continue; // channel for a node this skeleton doesn't have

			// Same bind-pose fallback the runtime uses, so a scrub and a
			// playback of the same clip cannot disagree.
			pose[boneId] = SampleChannel(anim.Channels[a], time, anim.HasFlag(ANIM_FLAG_APPLY_SCALE),
				(size_t)boneId < bindPose.size() ? &bindPose[boneId] : NULL);
		}

		ApplyPose(pose);
	}

	Matrix SkeletonAnimation::SCALE(const Matrix &in, const Matrix &prev, const f32 s)
	{
		Vec3 Translation = in.GetTranslation().Lerp(prev.GetTranslation(), s);
		Quaternion Rotation = in.ConvertToQuaternion().Slerp(prev.ConvertToQuaternion(), s);
		Vec3 Scale = in.GetScale().Lerp(prev.GetScale(), s);

		Matrix Out;
		Out.Translate(Translation);
		Out *= Rotation.ConvertToMatrix();
		//Out.Scale(Scale);

		return Out;
	}

}