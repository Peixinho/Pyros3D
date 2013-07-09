//============================================================================
// Name        : SceneGraph.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#ifndef SCENEGRAPH_H
#define	SCENEGRAPH_H

#include "../Core/Logs/Log.h"
#include "../Core/Math/Math.h"
#include "../GameObjects/GameObject.h"

using namespace p3d::Math;

namespace p3d {

    class GameObject;
    
    class SceneGraph
    {
        
        public:
            
            SceneGraph();
            
            // Update
            void Update();
            // Add Child to Scene
            void Add(GameObject* GO);
            // Remove Child from Scene
            void Remove(GameObject* GO);
            
        private:
            
            // GameObject List
            static std::vector<GameObject*> _GameObjectList;
            
            // Update Transformations Thread
            static bool _ThreadIsUpdating, _ThreadSync;
            static void *UpdateTransformations(void*);
            uint32 ThreadID;
    };
    
};

#endif	/* SCENEGRAPH_H */