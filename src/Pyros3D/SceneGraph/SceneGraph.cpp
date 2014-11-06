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
			std::vector<GameObject*> *vec = (GO->IsStatic()?&_GameObjectListStaticPrevious:&_GameObjectListDynamic);

			bool found = false;
			for (std::vector<GameObject*>::iterator i=vec->begin();i!=vec->end();i++)
			{
				if (*i==GO)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				vec->push_back(GO);
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
		std::vector<GameObject*> *vec = (GO->IsStatic()?&_GameObjectListStaticAfter:&_GameObjectListDynamic);

        bool found = false;
        for (std::vector<GameObject*>::iterator i=vec->begin();i!=vec->end();i++)
        {
            if (*i==GO)
            {
                // Unregister Components
                (*i)->UnregisterComponents(this);
                // Erase From List
                vec->erase(i);
                // Erase Scene Pointer
                GO->Scene = NULL;
                // Set Flag
                found = true;
                break;
            }
        }
        if (!found && GO->IsStatic())
		{
			vec = &_GameObjectListStaticPrevious;
			for (std::vector<GameObject*>::iterator i=vec->begin();i!=vec->end();i++)
			{
				if (*i==GO)
				{
					// Unregister Components
					(*i)->UnregisterComponents(this);
					// Erase From List
					vec->erase(i);
					// Erase Scene Pointer
					GO->Scene = NULL;
					// Set Flag
					found = true;
					break;
				}
			}
		}
		if (!found) echo("GameObject Not Found in Scene");
        else echo("SUCCESS: GameObject Removed from Scene");
    }
    
    void SceneGraph::Update(const f64 &Timer)
    {
        // Save Time
        timer = Timer;
        
		// Update Dynamic Objects Every Frame
		for (std::vector<GameObject*>::iterator i=_GameObjectListDynamic.begin();i!=_GameObjectListDynamic.end();i++)
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

		// Update Static Once
		for (std::vector<GameObject*>::iterator i=_GameObjectListStaticPrevious.begin();i!=_GameObjectListStaticPrevious.end();i++)
        {
            // Update GameObject - User Change
            (*i)->Update();
            // Register Components
            (*i)->RegisterComponents(this);
            // Update Components
            (*i)->UpdateComponents();
            // Update Transforms Not Using Threads
            (*i)->InternalUpdate();
			// Add to After and Remove From Previous
			_GameObjectListStaticAfter.push_back((*i));
			i = _GameObjectListStaticPrevious.erase(i);
			if (i==_GameObjectListStaticPrevious.end()) break;
        }
    }
    
    const f64 &SceneGraph::GetTime() const 
    {
        return timer;
    }
};