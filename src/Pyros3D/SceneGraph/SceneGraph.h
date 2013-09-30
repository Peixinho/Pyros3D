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
            void Update(const f64 &Timer);
            // Add Child to Scene
            void Add(GameObject* GO);
            // Remove Child from Scene
            void Remove(GameObject* GO);
            
            // Get Time
            const f64 &GetTime() const;
            
        private:
            
            // GameObject List
            std::vector<GameObject*> _GameObjectList;
            
            // Update Transformations Thread
            bool _ThreadIsUpdating, _ThreadSync;
            static void *UpdateTransformations(SceneGraph* Scene);
            uint32 ThreadID;
            
            // Time
            f64 timer;
    };
    
};

#endif	/* SCENEGRAPH_H */