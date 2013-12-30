//============================================================================
// Name        : GameObject.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#ifndef GAMEOBJECT_H
#define	GAMEOBJECT_H

#include "../Core/Math/Math.h"
#include "../Core/Logs/Log.h"
#include "../Components/IComponent.h"
#include "../SceneGraph/SceneGraph.h"
#include <vector>
using namespace p3d::Math;

namespace p3d {
    
    // Circular Dependency
    class IComponent;
    
    class GameObject
    {
        friend class SceneGraph;
        
        public:
        
            // Constructor
            GameObject();
            
            // Destructor
            virtual ~GameObject();
            
            // On Init Virtual Function
            virtual void Init();
            // Virtual Function To Update GameObject
            virtual void Update();
            // Destroy Function
            virtual void Destroy();
            
            // Local Space
            Matrix GetLocalTransformation();
            
            Vec3 GetPosition();
            Vec3 GetRotation();
            Vec3 GetScale();
            Vec3 GetDirection();
        
            // World Space
            Matrix GetWorldTransformation();
            Vec3 GetWorldPosition();
            Vec3 GetWorldRotation();
            
            // Set Properties
            void SetPosition(const Vec3 &position);
            void SetRotation(const Vec3 &rotation);
            void SetScale(const Vec3 &scale);
            
            // Set TransformationMatrix
            void SetTransformationMatrix(const Matrix &transformation);
            
            // LookAt Methods
            void LookAt(GameObject* GO);
            void LookAt(const Vec3 &Position);
            
            // Components
            void Add(IComponent* Component);
            void Remove(IComponent* Component);
            
            // Parent
            void Add(GameObject* Child);
            void Remove(GameObject* Child);

        private:
            
            // Update Components
            void UpdateComponents();
            
            // Internal Update
            void InternalUpdate();
            
            // Update Transformation
            void UpdateTransformation(const uint32 &order = 0);
            
            // Properties
            Vec3 _Position;
            Vec3 _Rotation;
            Vec3 _Scale;
            
            // Thread and User Properties
            Matrix _LocalMatrixUserEntered;
            Matrix _WorldMatrix;
            bool _IsDirty, _IsUsingCustomMatrix;
            
            // Local Transformation Matrix
            Matrix _LocalMatrix;
            
            // Looking At
            bool _IsLookingAtGameObject, _IsLookingAtPosition;
            GameObject* _IsLookingAtGameObjectPTR;
            Vec3 _IsLookingAtPositionVec;
            
            // Components
            std::vector<IComponent*> Components;
            
        protected:
            
            // GameObject Owner
            GameObject* _Owner;
            bool _HaveOwner;
            
            // GameObject Childs
            std::vector<GameObject*> _Childs;
            
            // Components Add/Removed
            bool _ComponentsChanged;
            
            // Register and Unregister
            void RegisterComponents(SceneGraph* Scene);
            void UnregisterComponents(SceneGraph* Scene);
            // Scene Pointer
            SceneGraph* Scene;
    };
    
};

#endif	/* GAMEOBJECT_H */