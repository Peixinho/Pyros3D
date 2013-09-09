//============================================================================
// Name        : SceneGraph.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#include "SceneGraph.h"
#include "../Utils/Thread/Thread.h"
#include <string.h>

namespace p3d {
    
    bool SceneGraph::_ThreadIsUpdating = false;
    bool SceneGraph::_ThreadSync = true;
    std::vector<GameObject*> SceneGraph::_GameObjectList;
    
    SceneGraph::SceneGraph() 
    {
        echo("SUCCESS: Scene Created");
    }
    
    void SceneGraph::Add(GameObject* GO)
    {
        if (GO->Scene==NULL)
        {
            bool found = false;
            for (std::vector<GameObject*>::iterator i=_GameObjectList.begin();i!=_GameObjectList.end();i++)
            {
                if (*i==GO)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                _GameObjectList.push_back(GO);
                // Set Scene Pointer
                GO->Scene = this;
                echo("SUCCESS: GameObject Added to Scene");
            } else {
                echo("ERROR: Component Already Added in the Scene");
            }
        } else {
            echo("ERROR: GameObject Already on a Scene");
        }
    }
    
    void SceneGraph::Remove(GameObject* GO)
    {
        bool found = false;
        for (std::vector<GameObject*>::iterator i=_GameObjectList.begin();i!=_GameObjectList.end();i++)
        {
            if (*i==GO)
            {
                // Unregister Components
                (*i)->UnregisterComponents(this);
                // Erase From List
                _GameObjectList.erase(i);
                // Erase Scene Pointer
                GO->Scene = NULL;
                // Set Flag
                found = true;
                break;
            }
        }
        if (!found) echo("GameObject Not Found in Scene");
        else echo("SUCCESS: GameObject Removed from Scene");
    }
    
    void SceneGraph::Update()
    {
        for (std::vector<GameObject*>::iterator i=_GameObjectList.begin();i!=_GameObjectList.end();i++)
        {
            // Update GameObject - User Change
            (*i)->Update();
			// Update Transformation
			(*i)->InternalUpdate();
			(*i)->CloneTransform();
            // Register Components
            (*i)->RegisterComponents(this);
            // Update Components
            (*i)->UpdateComponents();
        }
        
        if (!_ThreadIsUpdating && !_ThreadSync)
        {
            // Copy GameObjects to Thread
            _GameObjectList.resize(_GameObjectList.size());
            
            // Register Thread
            ThreadID = Thread::AddThread(UpdateTransformations);
                    
        } else {
            if (_ThreadIsUpdating && !_ThreadSync)
            {
                // Remove Thread
				Thread::RemoveThread(ThreadID);

				// Copy From Thread to GameObjects
				_ThreadSync = true;
				for (std::vector<GameObject*>::iterator i=_GameObjectList.begin();i!=_GameObjectList.end();i++)
				{
					// Copy from Thread
					(*i)->CloneTransform();
				}
				_ThreadSync = false;
            }
        }
    }
  
    // Thread Function
    void* SceneGraph::UpdateTransformations(void*)
    {
        // Set Flag
        _ThreadIsUpdating = true;
        for (std::vector<GameObject*>::iterator i=_GameObjectList.begin();i!=_GameObjectList.end();i++)
        {
            (*i)->InternalUpdate();
        }
        // Unset Flag
        _ThreadIsUpdating = false;

        return NULL;
    }
    
};