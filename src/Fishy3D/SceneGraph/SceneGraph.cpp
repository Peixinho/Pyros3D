//============================================================================
// Name        : SceneGraph.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#include "SceneGraph.h"
#include <assert.h>

namespace Fishy3D {

    SceneGraph::SceneGraph() 
    {
    
        // Rendering List Instance
        renderingList = SuperSmartPointer<RenderingList> (new RenderingList());
    
    }

    SceneGraph::~SceneGraph() 
    {
        //RemoveAllChilds();
    }

    SuperSmartPointer<RenderingList> SceneGraph::GetRenderingList()
    {
        return renderingList;
    }
    
    void SceneGraph::AddChild(SuperSmartPointer<GameObject> go)
    {
        bool found=false;

        if(GameObjectIDs.find(go->GetID()) != GameObjectIDs.end()) found=true;

        if (!found)
        {
            
            // Register SceneGraphStruct ID and SuperSmartPointer            
            GameObjectIDs[go->GetID()].GameObjectInstance = go;
            GameObjectIDs[go->GetID()].GameObjectInstance->RegisterComponents(this,true);
            
        }
    }
    void SceneGraph::AddChild(SuperSmartPointer<GameObject> parent, SuperSmartPointer<GameObject> go)
    {
        bool found=false;

        if(GameObjectIDs.find(parent->GetID()) != GameObjectIDs.end()) found=true;

        if (!found)
        {
            // Register SceneGraphStruct ID and SuperSmartPointer            
            GameObjectIDs[parent->GetID()].GameObjectInstance = parent;
        }
        
        GameObjectIDs[go->GetID()].GameObjectInstance = go;
        GameObjectIDs[go->GetID()].GameObjectParent = parent;
        GameObjectIDs[go->GetID()].GameObjectInstance->RegisterComponents(this,true);
        
    }
    bool SceneGraph::IsChild(SuperSmartPointer<GameObject> go)
    {
        for (std::map<StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++)
        {
            if (go.Get()==(*i).second.GameObjectInstance.Get()) {
                return true;
            }
        }        
        return false;
    }
    bool SceneGraph::IsChild(const std::string &goName)
    {
        StringID ID(MakeStringID(goName));
        if (GameObjectIDs.find(ID)==GameObjectIDs.end()) return false;
        else return true;
    }
    bool SceneGraph::IsChild(GameObject* go)
    {
        for (std::map<StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++)
        {
            if (go==(*i).second.GameObjectInstance.Get()) {
                return true;
            }
        }        
        return false;
    }
    
    void SceneGraph::RemoveChild(SuperSmartPointer<GameObject> go)
    {
        for (std::map<StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++)
        {
            if (go.Get()==(*i).second.GameObjectInstance.Get()) {
                // Unregister all Components
                (*i).second.GameObjectInstance->UnRegisterComponents(this,true);
                GameObjectIDs.erase(i);
                break;
            }
        }
    }
    void SceneGraph::RemoveChild(SuperSmartPointer<GameObject> parent, SuperSmartPointer<GameObject> go)
    {
        for (std::map<StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++)
        {
            if (parent.Get()==(*i).second.GameObjectParent.Get()) {                
                if (go.Get()==(*i).second.GameObjectInstance.Get())
                {
                    GameObjectIDs.erase(i);                    
                }
            }
        }
    }
    void SceneGraph::RemoveChilds(SuperSmartPointer<GameObject> go)
    {
        for (std::map<StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++)
        {
            if (go.Get()==(*i).second.GameObjectParent.Get()) {
                GameObjectIDs.erase(i);                
            }
        }
    }
    void SceneGraph::RemoveChild(const unsigned long &ID)
    {

        std::map<StringID, GameObjectID>::iterator i = GameObjectIDs.find(ID);
        if ((*i).first==ID) {
            GameObjectIDs.erase(i);
        }
        
    }   
    void SceneGraph::RemoveAllChilds()
    {
        GameObjectIDs.clear();
    }
 
    void SceneGraph::Register()
    {
         std::map <StringID, GameObjectID> GameList = GetList();
         for (std::map <StringID, GameObjectID>::iterator i=GameList.begin();i!=GameList.end();i++) 
         {                       
             
            (*i).second.GameObjectInstance->RegisterComponents(this);

         }
    }
    
    Matrix SceneGraph::GetParentMatrix(unsigned long ID)
    {
        if (GameObjectIDs[ID].GameObjectParent.Get()!=NULL)
            return GetParentMatrix(GameObjectIDs[ID].GameObjectParent->GetID()) * GameObjectIDs[ID].GameObjectInstance->GetLocalMatrix();
        else
            return GameObjectIDs[ID].GameObjectInstance->GetLocalMatrix();
    }
    
    void SceneGraph::Update(const float &Timer)
    {
        
        // Save Timer
        this->Timer = Timer;
        
        // Compute GameObject Matrices
        for (std::map <StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++)
        {
             if ((*i).second.GameObjectParent.Get()!=NULL) {
                // Update Transformation
                 (*i).second.GameObjectInstance->transformation.update();
                // Updates Game Object Transformation Based on Hierarchy
                 (*i).second.GameObjectInstance->UpdateMatrix(GetParentMatrix((*i).second.GameObjectParent->GetID()),(*i).second.GameObjectInstance->GetLocalMatrix());
             } else {
                 // Update Transformation
                 (*i).second.GameObjectInstance->transformation.update();
                 // Updates Game Object Transformation Based on Hierarchy
                 (*i).second.GameObjectInstance->UpdateMatrix((*i).second.GameObjectInstance->GetLocalMatrix());
             }
                          
            // Register components
            if ((*i).second.GameObjectInstance->ComponentsRemoved()==true)
            {
                // Unregisters
                (*i).second.GameObjectInstance->UnRegisterComponents(this);
            }
            if ((*i).second.GameObjectInstance->ComponentsAdded()==true)
            {
                // Registers
                (*i).second.GameObjectInstance->RegisterComponents(this);
            }
             // Updates GameObject
            (*i).second.GameObjectInstance->Update();
            // Updates all components in the GameObject
            (*i).second.GameObjectInstance->UpdateComponents();
        }
         
        // Compute Lights
        renderingList->ComputeLights();
        
    }
    
    // returns game object list
    std::map <StringID, GameObjectID> SceneGraph::GetList()
    {
        return GameObjectIDs;
    }   
    
    SuperSmartPointer<GameObject> SceneGraph::GetGameObjectByName(const std::string &goName)
    {
        StringID ID(MakeStringID(goName));
        return GameObjectIDs.find(ID)->second.GameObjectInstance;
    }
    SuperSmartPointer<GameObject> SceneGraph::GetGameObjectByID(const unsigned long &ID) 
    {
        return GameObjectIDs.find(ID)->second.GameObjectInstance;
    }
    std::string SceneGraph::GetGameObjectID(GameObject* go)
    {
        for (std::map <StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++) {
            if ((*i).second.GameObjectInstance.Get()==go) return (std::string)(*i).second.GameObjectInstance->GetName();
        }
        return (std::string)"";
    }
    std::string SceneGraph::GetGameObjectID(SuperSmartPointer<GameObject> go)
    {
        for (std::map <StringID, GameObjectID>::iterator i=GameObjectIDs.begin();i!=GameObjectIDs.end();i++) {
            if ((*i).second.GameObjectInstance.Get()==go.Get()) return (std::string)(*i).second.GameObjectInstance->GetName();
        }
        return (std::string)"";
    }      
    
    const float &SceneGraph::GetTimer() const
    {
        return Timer;
    }
    
}
