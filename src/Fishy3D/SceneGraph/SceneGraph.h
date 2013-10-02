//============================================================================
// Name        : SceneGraph.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SceneGraph
//============================================================================

#ifndef SCENEGRAPH_H
#define	SCENEGRAPH_H

#include <list>
#include <map>
#include "../Utils/Pointers/SuperSmartPointer.h"
#include "../Core/GameObjects/GameObject.h"
#include "../Utils/StringIDs/StringID.hpp"
#include "RenderingList/RenderingList.h"

namespace Fishy3D {

    struct GameObjectID
    {    
        SuperSmartPointer<GameObject> GameObjectInstance;
        SuperSmartPointer<GameObject> GameObjectParent;
    };

    class RenderingList;
    
    class SceneGraph {
    public:      

        SceneGraph();

        // check if gameobject is present
        bool IsChild(SuperSmartPointer<GameObject> go);
        bool IsChild(const std::string &goName);
        bool IsChild(GameObject* go);

        // adds gameObjecto to the renderList
        void AddChild(SuperSmartPointer<GameObject> go);
        void AddChild(SuperSmartPointer<GameObject> parent,SuperSmartPointer<GameObject> go);

        // removes gameObject from the renderList
        void RemoveChild(SuperSmartPointer<GameObject> go);
        void RemoveChild(SuperSmartPointer<GameObject> parent,SuperSmartPointer<GameObject> go);
        void RemoveChilds(SuperSmartPointer<GameObject> go);
        void RemoveChild(const unsigned long &ID);   
        void RemoveAllChilds();

        // register gameobjects components
        void Register();
        // updates gameobjects components
        void Update(const float &Timer);

        // Get Rendering List
        SuperSmartPointer<RenderingList> GetRenderingList();
        
        std::map <StringID, GameObjectID>  GetList();

        //void sortList(SortMode* sort);

        // returns game object instance
        SuperSmartPointer <GameObject> GetGameObjectByName(const std::string &goName);
        SuperSmartPointer <GameObject> GetGameObjectByID(const unsigned long &ID);    
        // returns game object ID name
        std::string GetGameObjectID(GameObject* go);
        std::string GetGameObjectID(SuperSmartPointer<GameObject> go);

        const float &GetTimer() const;
        
        virtual ~SceneGraph();           

    private:
        
        // Timer
        float Timer;
        
        // Rendering List
        SuperSmartPointer<RenderingList> renderingList;
        
        // Map String ID
        std::map<StringID, GameObjectID > GameObjectIDs; 

        // Aux Function return ParentMatrix
        // to Calculate World Matrix
        Matrix GetParentMatrix(unsigned long ID);

    };

}

#endif	/* SCENEGRAPH_H */