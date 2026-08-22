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
		// Mark Layer as NOT being used by this
		AnimationsToPlay[animationOrder].Layer->usingLayer--;

		// Insert Removed Bones if there isn't any Layer
		if (AnimationsToPlay[animationOrder].Layer->usingLayer == 0)
		{
			for (std::vector<int32>::iterator i = AnimationsToPlay[animationOrder].Layer->boneIDs.begin(); i != AnimationsToPlay[animationOrder].Layer->boneIDs.end(); i++)
				if ((*i) == 1)
					boneIDs[i - AnimationsToPlay[animationOrder].Layer->boneIDs.begin()] = 1;
		}

		// Remove Layer if Any
		AnimationsToPlay.erase(AnimationsToPlay.begin() + animationOrder);
	}

	void SkeletonAnimationInstance::Stop()
	{
		AnimationsToPlay.clear();
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
						Matrix trafo = SkeletonAnimationInstance::SampleChannel(Anim.Channels[a], currentTime);

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
							(*_Anim).boneTransformationPerAnimation[(*i)->ChannelBoneIDCache[a]] = trafo;
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

	Matrix SkeletonAnimationInstance::SampleChannel(const Channel &ch, const f32 time)
	{
		Vec3 curPosition, curScale;
		Quaternion curRotation;

		// Empty key lists were never possible for clips coming out of the
		// Assimp importer (it writes a key per channel per component), but
		// an editor-authored clip only carries the components the user
		// actually keyed, so each of the three blocks below has to cope
		// with having nothing to sample.
		if (!ch.positions.empty())
		{
			size_t posIndex = 0;
			while (1)
			{
				if (posIndex + 1 >= ch.positions.size() || ch.positions[posIndex + 1].Time > time)
					break;
				posIndex++;
			}
			curPosition = ch.positions[posIndex].Pos;

			// Position Interpolation
			if (posIndex + 1 < ch.positions.size())
			{
				const size_t posIndexNext = posIndex + 1;
				const f32 span = ch.positions[posIndexNext].Time - ch.positions[posIndex].Time;
				// Two keys at the same time is a divide by zero (NaN
				// position, and from there a NaN bone matrix that silently
				// collapses the whole mesh) - reachable in the editor by
				// dropping a key onto an existing one.
				const f32 slerp_delta = (span > 0.f ? (time - ch.positions[posIndex].Time) / span : 0.f);
				curPosition = curPosition.Lerp(ch.positions[posIndexNext].Pos, slerp_delta);
			}
		}

		if (!ch.rotations.empty())
		{
			size_t rotIndex = 0;
			while (1)
			{
				if (rotIndex + 1 >= ch.rotations.size() || ch.rotations[rotIndex + 1].Time > time)
					break;
				rotIndex++;
			}
			curRotation = ch.rotations[rotIndex].Rot;

			// Rotation Interpolation
			if (rotIndex + 1 < ch.rotations.size())
			{
				const size_t rotIndexNext = rotIndex + 1;
				const f32 span = ch.rotations[rotIndexNext].Time - ch.rotations[rotIndex].Time;
				const f32 slerp_delta = (span > 0.f ? (time - ch.rotations[rotIndex].Time) / span : 0.f);
				curRotation = curRotation.Slerp(ch.rotations[rotIndexNext].Rot, slerp_delta);
			}
		}

		if (!ch.scales.empty())
		{
			size_t scaleIndex = 0;
			while (1)
			{
				if (scaleIndex + 1 >= ch.scales.size() || ch.scales[scaleIndex + 1].Time > time)
					break;
				scaleIndex++;
			}
			curScale = ch.scales[scaleIndex].Scale;

			// Scale Interpolation
			if (scaleIndex + 1 < ch.scales.size())
			{
				const size_t scaleIndexNext = scaleIndex + 1;
				const f32 span = ch.scales[scaleIndexNext].Time - ch.scales[scaleIndex].Time;
				const f32 slerp_delta = (span > 0.f ? (time - ch.scales[scaleIndex].Time) / span : 0.f);
				curScale = curScale.Lerp(ch.scales[scaleIndexNext].Scale, slerp_delta);
			}
		}

		Matrix trafo = curRotation.ConvertToMatrix();
		trafo.Translate(curPosition);
		// Scale stays deliberately unapplied, exactly as playback has
		// always done it (the .Scale() call was already commented out in
		// Update()); keying scale in the editor therefore round-trips
		// through the file but does not deform the mesh. Left as-is rather
		// than "fixed" here: turning it on would change how every existing
		// clip in every project renders.
		return trafo;
	}

	void SkeletonAnimationInstance::RefreshSkinning()
	{
		// Multiply bones with its parent - Tree
		for (std::vector<Bone>::iterator a = skeleton.begin(); a != skeleton.end(); a++)
			Bones[(*a).self] = GetParentMatrix((*a).parent, boneTransformation) * boneTransformation[(*a).self];

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

			pose[boneId] = SampleChannel(anim.Channels[a], time);
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