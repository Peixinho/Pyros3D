//============================================================================
// Name        : AnimationManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Animation Manager
//============================================================================

#ifndef ANIMATIONMANAGER_H
#define	ANIMATIONMANAGER_H

#include "../Utils/Pointers/SuperSmartPointer.h"
#include "../Utils/ModelLoaders/MultiModelLoader/AnimationLoader.h"
#include "../Utils/ModelLoaders/MultiModelLoader/ModelLoader.h"
#include "../Components/IComponent.h"
#include "../Components/RenderingComponents/RenderingModelComponent/RenderingModelComponent.h"

namespace Fishy3D {
    
    class AnimationManager {
    public:
        
        AnimationManager();
        virtual ~AnimationManager();                
        
        void LoadAnimation(const std::string &AnimationFile, const SuperSmartPointer<IComponent> &RenderingComponent);
        void Update(const double &time);
        
        Matrix GetBoneMatrix(const unsigned id);
        
    private:

        Matrix GetParentMatrix(const unsigned &id, std::vector<Matrix> &bones);
        
        SuperSmartPointer<AnimationLoader> animationLoader;
        SuperSmartPointer<IComponent> renderingComponent;
        
        std::vector<Matrix> boneTransformation;
        std::vector<Matrix> Bones;
        std::map<StringID, Bone> skeleton;
        std::map<unsigned, StringID> MapLocalToGlobalIDs;
    };

}
#endif	/* ANIMATIONMANAGER_H */