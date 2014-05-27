//============================================================================
// Name        : AnimationManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animation Manager
//============================================================================

#ifndef ANIMATIONMANAGER_H
#define ANIMATIONMANAGER_H

#include "../Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h"
#include "../Utils/ModelLoaders/MultiModelLoader/ModelLoader.h"
#include "../Components/IComponent.h"
#include "../Rendering/Components/Rendering/RenderingComponent.h"
#include "../Other/Export.h"

namespace p3d {
    
    class PYROS3D_API AnimationManager {

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
