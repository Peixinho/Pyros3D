//============================================================================
// Name        : SceneGraph.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <string.h>

namespace p3d {
    
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
    
    void SceneGraph::Update(const f64 &Timer)
    {
        // Save Time
        timer = Timer;
        
        for (std::vector<GameObject*>::iterator i=_GameObjectList.begin();i!=_GameObjectList.end();i++)
        {
            // Update GameObject - User Change
            (*i)->Update();
            // Register Components
            (*i)->RegisterComponents(this);
            // Update Components
            (*i)->UpdateComponents();
            // Update Transforms Not Using Threads
            (*i)->InternalUpdate();
            
        }
    }
    
    const f64 &SceneGraph::GetTime() const 
    {
        return timer;
    }
    
};