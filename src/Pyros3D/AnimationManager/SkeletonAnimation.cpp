//============================================================================
// Name        : SkeletonAnimation.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animator Interface
//============================================================================

#include <Pyros3D/AnimationManager/SkeletonAnimation.h>

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
						Channel ch = Anim.Channels[a];
						Vec3 curPosition, curScale;
						Quaternion curRotation;

						size_t posIndex = 0;
						size_t posIndexNext = 0;
						while (1)
						{
							if (posIndex + 1 >= ch.positions.size() || ch.positions[posIndex + 1].Time > currentTime)
								break;
							posIndex++;
						}
						curPosition = ch.positions[posIndex].Pos;

						// Position Interpolation
						if (posIndex + 1 < ch.positions.size())
						{
							posIndexNext = posIndex + 1;
							f32 slerp_delta = (currentTime - ch.positions[posIndex].Time) / (ch.positions[posIndexNext].Time - ch.positions[posIndex].Time);
							//slerp_delta = 1 - (ch.positions[posIndexNext].Time - currentTime);
							curPosition = curPosition.Lerp(ch.positions[posIndexNext].Pos, slerp_delta);
						}

						size_t rotIndex = 0;
						size_t rotIndexNext = 0;
						while (1)
						{
							if (rotIndex + 1 >= ch.rotations.size() || ch.rotations[rotIndex + 1].Time > currentTime)
								break;
							rotIndex++;
						}
						curRotation = ch.rotations[rotIndex].Rot;

						// Rotation Interpolation
						if (rotIndex + 1 < ch.rotations.size())
						{
							rotIndexNext = rotIndex + 1;
							f32 slerp_delta = (currentTime - ch.rotations[rotIndex].Time) / (ch.rotations[rotIndexNext].Time - ch.rotations[rotIndex].Time);
							//slerp_delta = 1 - (ch.rotations[rotIndexNext].Time - currentTime);
							curRotation = curRotation.Slerp(ch.rotations[rotIndexNext].Rot, slerp_delta);
						}

						size_t scaleIndex = 0;
						size_t scaleIndexNext = 0;
						while (1)
						{
							if (scaleIndex + 1 >= ch.scales.size() || ch.scales[scaleIndex + 1].Time > currentTime)
								break;
							scaleIndex++;
						}
						curScale = ch.scales[scaleIndex].Scale;

						// Rotation Interpolation
						if (scaleIndex + 1 < ch.scales.size())
						{
							scaleIndexNext = scaleIndex + 1;
							f32 slerp_delta = (currentTime - ch.scales[scaleIndex].Time) / (ch.scales[scaleIndexNext].Time - ch.scales[scaleIndex].Time);
							//slerp_delta = 1 - (ch.scales[scaleIndexNext].Time - currentTime);
							curScale = curScale.Lerp(ch.scales[scaleIndexNext].Scale, slerp_delta);
						}

						Matrix trafo = curRotation.ConvertToMatrix();
						trafo.Translate(curPosition);
						//trafo.Scale(curScale);

						if ((*i)->ChannelBoneIDCache[a] == -1)
						{
							uint32 idBone = 0;
							for (std::vector<Bone>::iterator b = (*i)->skeleton.begin(); b != (*i)->skeleton.end(); b++)
							{
								if ((*b).name.compare(Anim.Channels[a].NodeName) == 0)
								{
									(*i)->ChannelBoneIDCache[a] = (*b).self;
									break;
								}
							}
						}

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