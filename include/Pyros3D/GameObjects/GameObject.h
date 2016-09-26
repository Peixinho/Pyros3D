//============================================================================
// Name        : GameObject.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#ifndef GAMEOBJECT_H
#define	GAMEOBJECT_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Components/IComponent.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Other/Export.h>
#include <vector>
#include <map>
using namespace p3d::Math;

namespace p3d {
    
    // Circular Dependency
    class PYROS3D_API IComponent;
    
    class PYROS3D_API GameObject
    {
        friend class SceneGraph;
        
        public:
        
            // Constructor
            GameObject(bool isStatic = false);
            
            // Destructor
            virtual ~GameObject();
            
            // On Init Virtual Function
            virtual void Init();
            // Virtual Function To Update GameObject
            virtual void Update();
            // Destroy Function
            virtual void Destroy();
            
            // Local Space
            const Matrix &GetLocalTransformation() const;
            
            const Vec3 &GetPosition() const;
            const Vec3 &GetRotation() const;
            const Vec3 &GetScale() const;
            const Vec3 GetDirection() const;
        
            // World Space
            const Matrix &GetWorldTransformation() const;
            const Vec3 GetWorldPosition() const;
            const Vec3 GetWorldRotation() const;
            
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
			GameObject* GetParent() { return _Owner; }
			bool HaveParent() { return _HaveOwner; }

			// Tags
			void AddTag(const std::string &tag);
			void RemoveTag(const std::string &tag);
			bool HaveTag(const uint32 tag);
			bool HaveTag(const std::string &tag);

			// Static
			bool IsStatic() { return isStatic; }

            // Helpers
            void AddComponent(IComponent* Component);
            void AddGameObject(GameObject* Component);
            void RemoveComponent(IComponent* Component);
            void RemoveGameObject(GameObject* Component);
            void LookAtGameObject(GameObject* GO);
            void LookAtVec(const Vec3 &center);

			// Not transformed boundings
			const Vec3 &GetBoundingMinValue() const { return minBounds; }
			const Vec3 &GetBoundingMaxValue() const { return maxBounds; }
			const Vec3 &GetBoundingSphereCenter() const { return BoundingSphereCenter; }
			const float &GetBoundingSphereRadius() const { return BoundingSphereRadius; }

			// World Space boundings
			const Vec3 GetBoundingMinValueWorldSpace() const { return minBoundsWorldSpace; }
			const Vec3 GetBoundingMaxValueWorldSpace() const { return maxBoundsWorldSpace; }
			const float GetBoundingSphereRadiusWorldSpace() const { return BoundingSphereRadiusWorldSpace; }

			// Get Components List
			const std::vector<IComponent*> &GetComponents() const { return Components; }

        private:
            
            // Update Components
            void UpdateComponents();
            
            // Internal Update
            bool InternalUpdate();
            
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
            
			// Update Transformation
			bool UpdateTransformation(const uint32 order = 0);

			// Static
			bool isStatic;

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

			// Tags
			std::map<uint32, std::string> TagsList;

			// Bounds of the Component
			f32 BoundingSphereRadius;
			Vec3 BoundingSphereCenter;
			Vec3 maxBounds, minBounds;

			f32 BoundingSphereRadiusWorldSpace;
			Vec3 maxBoundsWorldSpace, minBoundsWorldSpace;
    };
    
};

#endif	/* GAMEOBJECT_H */