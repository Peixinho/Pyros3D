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
        struct SkeletonAnimation
        {
            uint32 ID;
            f32 startTime;
            Animation* animation;
            f32 speed;
            f32 scale;

            f32 _startTimeClock;
            f32 _currentTime;

            // Pause
            bool _isPaused;
            bool _resumed;
            f32 _pauseStart;
            f32 _pauseTime;

            f32 _repetition;
        };
    }
    
    class SkeletonAnimation;

    class PYROS3D_API SkeletonAnimationInstance {

        friend class SkeletonAnimation;

        public:
            
            SkeletonAnimationInstance(SkeletonAnimation* owner, RenderingComponent* Component);
            virtual ~SkeletonAnimationInstance() {}

            // Play, Stop, Pause
            uint32 Play(const uint32 animation, const f32 startTime, const f32 repetition = 1, const f32 speed = 1.f, const f32 scale = 1.f);
            void Pause();
            void PauseAnimation(const uint32 animationOrder);
            void ResumeAnimation(const uint32 animationOrder);
            void Resume();
            void StopAnimation(const uint32 animationOrder);
            void Stop();
            bool IsPaused(const uint32 animationOrder);
            bool IsPaused();
            f32 GetAnimationCurrentTime(const uint32 animationOrder);
            f32 GetAnimationSpeed(const uint32 animationOrder);
            f32 GetAnimationStartTime(const uint32 animationOrder);
            f32 GetAnimationID(const uint32 animationOrder);
            f32 GetAnimationScale(const uint32 animationOrder);

            int32 GetAnimationPositionInVector(const uint32 animation);

        protected:

            // Keep Rendering Component PTR
            RenderingComponent* rcomp;

            // Keep Owner PTR
            SkeletonAnimation* Owner;

            // Animations Playing
            std::vector<_SkeletonAnimation::SkeletonAnimation> AnimationsToPlay;

            // Skeleton info and Shit
            std::vector<Matrix> boneTransformation;
            std::vector<Matrix> Bones;
            std::map<StringID, Bone> skeleton;
            std::map<uint32, StringID> MapLocalToGlobalIDs;

            Matrix GetParentMatrix(const int32 id, const std::vector<Matrix> &bones);
            Matrix GetBoneMatrix(const uint32 id);

            bool _paused;
    };

    class PYROS3D_API SkeletonAnimation 
    {

        friend class SkeletonAnimationInstance;
    
        private:

            // internal timer
            f32 timer;
            // frames
            std::vector<Texture*> Frames;
            // Instances
            std::vector<SkeletonAnimationInstance*> Instances;
            // Animations
            std::vector<Animation> animations;
            // Animation Loader
            AnimationLoader* animationLoader;

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
    };

}
#endif  /* SKELETONANIMATION_H */