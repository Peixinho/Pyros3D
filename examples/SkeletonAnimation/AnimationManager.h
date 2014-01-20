//============================================================================
// Name        : AnimationManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animation Manager
//============================================================================

#ifndef ANIMATIONMANAGER_H
#define ANIMATIONMANAGER_H

#include "Pyros3D/Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h"
#include "Pyros3D/Utils/ModelLoaders/MultiModelLoader/ModelLoader.h"
#include "Pyros3D/Components/IComponent.h"
#include "Pyros3D/Rendering/Components/Rendering/RenderingComponent.h"

namespace p3d {
    
    class AnimationManager {

        public:
            
            AnimationManager();
            virtual ~AnimationManager();                
            
            void LoadAnimation(const std::string &AnimationFile, RenderingComponent* Component);
            void Update(const double &time);
            
            Matrix GetBoneMatrix(const uint32 &id);
            
        private:

            Matrix GetParentMatrix(const int32 &id, const std::vector<Matrix> &bones);
            
            AnimationLoader* animationLoader;
            RenderingComponent* renderingComponent;
            
            std::vector<Matrix> boneTransformation;
            std::vector<Matrix> Bones;
            std::map<StringID, Bone> skeleton;
            std::map<uint32, StringID> MapLocalToGlobalIDs;
    };

}
#endif  /* ANIMATIONMANAGER_H */