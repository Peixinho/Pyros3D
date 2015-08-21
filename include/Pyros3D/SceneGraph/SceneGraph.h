//============================================================================
// Name        : SceneGraph.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#ifndef SCENEGRAPH_H
#define	SCENEGRAPH_H

#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Other/Export.h>

using namespace p3d::Math;

namespace p3d {

	void *UpdateTransformations(void* ptr);

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

            void AddGameObject(GameObject* Component);
            void RemoveGameObject(GameObject* Component);

			std::vector<GameObject*> &GetGameObjectList();

        private:

            // GameObject Dynamic List
            std::vector<GameObject*> _GameObjectListDynamic;
			// GameObject Static Lists
			std::vector<GameObject*> _GameObjectListStaticPrevious;
			std::vector<GameObject*> _GameObjectListStaticAfter;

            // Time
            f64 timer;
    };
    
};

#endif	/* SCENEGRAPH_H */