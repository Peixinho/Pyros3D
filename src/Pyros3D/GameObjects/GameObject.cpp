//============================================================================
// Name        : GameObject.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Ext/StringIDs/StringID.hpp>

namespace p3d {

    GameObject::GameObject(bool isStatic) 
    {
		// Static
		this->isStatic = isStatic;

        // Set Default Flags
        _IsLookingAtGameObject = false;
        _IsLookingAtPosition = false;
        _IsDirty = false;
        _IsUsingCustomMatrix = false;
        _Scale = Vec3(1.f,1.f,1.f);
        _HaveOwner = false;
        _ComponentsChanged = false;
        Scene = NULL;
    }
    
    // Destructor
    GameObject::~GameObject() {}
    
    // Virtual Function on Initialization
    void GameObject::Init() {}
    // Virtual Function To Update
    void GameObject::Update() {}
    // Virtual Function on Destroy
    void GameObject::Destroy() {}
    
    // Internal Update for Transformation
    bool GameObject::InternalUpdate()
    {
        // Update Transformation
		bool r = UpdateTransformation();

		Vec3 _min = GetWorldTransformation()*minBounds;
		Vec3 _max = GetWorldTransformation()*maxBounds;

		// Defining box vertex and Apply Transform
		Vec3 v[8];
		v[0] = GetWorldTransformation() * _min;
		v[1] = GetWorldTransformation() * Vec3(_min.x, _min.y, _max.z);
		v[2] = GetWorldTransformation() * Vec3(_min.x, _max.y, _max.z);
		v[3] = GetWorldTransformation() * _max;
		v[4] = GetWorldTransformation() * Vec3(_min.x, _max.y, _min.z);
		v[5] = GetWorldTransformation() * Vec3(_max.x, _min.y, _min.z);
		v[6] = GetWorldTransformation() * Vec3(_max.x, _max.y, _min.z);
		v[7] = GetWorldTransformation() * Vec3(_max.x, _min.y, _max.z);

		// Get new Min and Max
		Vec3 min = v[0];
		Vec3 max = v[0];
		for (uint32 i = 1; i<8; i++)
		{
			if (v[i].x<min.x) min.x = v[i].x;
			if (v[i].y<min.y) min.y = v[i].y;
			if (v[i].z<min.z) min.z = v[i].z;
			if (v[i].x>max.x) max.x = v[i].x;
			if (v[i].y>max.y) max.y = v[i].y;
			if (v[i].z>max.z) max.z = v[i].z;
		}

		minBoundsWorldSpace = min;
		maxBoundsWorldSpace = max;

		return r;
    }
    
    // Updates the Transformation Matrix
    bool GameObject::UpdateTransformation(const uint32 order)
    {
		bool wasDirty = false;

        if (_IsDirty)
        {
			wasDirty = true;

            if (_IsUsingCustomMatrix) {
                
                _LocalMatrix = _LocalMatrixUserEntered;
                _IsUsingCustomMatrix = false;
                
                // Set Properties
                _Position = _LocalMatrix.GetTranslation();
                _Rotation = _LocalMatrix.GetEulerFromRotationMatrix();
                _Scale = _LocalMatrix.GetScale();
                
            } else {
                
                // Set Local Matrix Identity
                _LocalMatrix.identity();

                // apply translation
                _LocalMatrix.Translate(_Position.x,_Position.y,_Position.z);

                // apply rotation
                Quaternion q;
                q.SetRotationFromEuler(_Rotation, order);
                _LocalMatrix *= q.ConvertToMatrix();
                
                // apply scale
                Matrix scaleMatrix;                
                scaleMatrix.Scale(_Scale.x,_Scale.y,_Scale.z);
                _LocalMatrix *= scaleMatrix;
                
            }
        }
        
        // Apply this ONLY if is Looking At
        if (_IsLookingAtGameObject || _IsLookingAtPosition)
        {
			wasDirty = true;

            Vec3 target;
            if (_IsLookingAtGameObject==true)
                target=_IsLookingAtGameObjectPTR->GetWorldPosition();                    
            else if (_IsLookingAtPosition==true) {
                target=_IsLookingAtPositionVec;
            } else {
                target = Vec3::ZERO;
            }

            // Transformation    
            Matrix TransfMatrix = _LocalMatrix;

            // Translation value
            Vec3 Translation = TransfMatrix.GetTranslation();

            // Look at Position (Right Handed)
            TransfMatrix.LookAt(Translation,target,Vec3::UP);

            TransfMatrix=TransfMatrix.Inverse();

            TransfMatrix.Translate(Translation);

            _LocalMatrix = TransfMatrix;

            // apply scale
            Matrix scaleMatrix;                
            scaleMatrix.Scale(_Scale.x,_Scale.y,_Scale.z);
            _LocalMatrix *= scaleMatrix;
            
            // Save Rotation After LookAt
            _Rotation = _LocalMatrix.GetRotation(_Scale).GetEulerFromRotationMatrix();
            
        }
        _IsDirty = false;
        
        if (_HaveOwner)
        {
            _Owner->UpdateTransformation();
            _WorldMatrix = _Owner->_WorldMatrix * _LocalMatrix;
			wasDirty = true;
        } else {
			_WorldMatrix = _LocalMatrix;
		}

		// Set Bounding Sphere Scale
		BoundingSphereRadiusWorldSpace = BoundingSphereRadius * Max(_Scale.x, Max(_Scale.y, _Scale.z));

		return wasDirty;
    }
    // Gets Transformation Matrix
    const Matrix &GameObject::GetWorldTransformation() const
    {
        return _WorldMatrix;
    }
    const Matrix &GameObject::GetLocalTransformation() const
    {
        return _LocalMatrix;
    }
    const Vec3 GameObject::GetDirection() const
    {
        return Vec3(_WorldMatrix.m[8],_WorldMatrix.m[9],_WorldMatrix.m[10]);
    }
    void GameObject::SetTransformationMatrix(const Matrix& transformation)
    {
        _LocalMatrixUserEntered = transformation;
        _Position = transformation.GetTranslation();
        _Scale = transformation.GetScale();
        _Rotation = transformation.GetRotation(_Scale).GetEulerFromRotationMatrix();
        _IsDirty = true;
        _IsUsingCustomMatrix = true;
    }
    // Properties Getters and Setters
    void GameObject::SetPosition(const Vec3 &position)
    {
        _IsDirty = true;
        _Position = position;
    }
    // Sets Rotation
    void GameObject::SetRotation(const Vec3 &rotation)
    {
        _IsDirty = true;
        _Rotation = rotation;
    }
    // Sets Scale
    void GameObject::SetScale(const Vec3 &scale)
    {
        _IsDirty = true;
        _Scale = scale;
    }
    // Gets Position
    const Vec3 &GameObject::GetPosition() const
    {
        return _Position;
    }
    // Gets Rotation
    const Vec3 &GameObject::GetRotation() const
    {
        return _Rotation;
    }
    // Gets Scale
    const Vec3 &GameObject::GetScale() const
    {
        return _Scale;
    }
    // Gets Position
    const Vec3 GameObject::GetWorldPosition() const
    {
        return _WorldMatrix.GetTranslation();
    }
    // Gets Rotation
    const Vec3 GameObject::GetWorldRotation() const
    {
        return _WorldMatrix.GetRotation(_Scale).GetEulerFromRotationMatrix();
    }
    // Look At a Given Game Object
    void GameObject::LookAt(GameObject* GO)
    {
        _IsLookingAtGameObject = true;
        _IsLookingAtPosition = false;
        _IsLookingAtGameObjectPTR = GO;
        _IsLookingAtPositionVec = Vec3::ZERO;
    }
    // Look At a Given Position
    void GameObject::LookAt(const Vec3 &Position)
    {
        _IsLookingAtPosition = true;
        _IsLookingAtGameObject = false;
        _IsLookingAtGameObjectPTR = NULL;
        _IsLookingAtPositionVec = Position;
    }
    
    void GameObject::Add(IComponent* Component)
    {
        if (Component->GetOwner()!=NULL)
        {
            echo("ERROR: Component Is Already in Another GameObject");
        } else {
            bool found=false;
            for (std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
            {
                if ((*i)==Component) 
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                // Add Component to List
                Components.push_back(Component);
                // Own this Shit!
                Component->Owner = this;
                // Change Flag
                _ComponentsChanged = true;

				if (Components.size() == 1)
				{
					minBounds = Component->minBounds;
					maxBounds = Component->maxBounds;
					BoundingSphereRadius = Component->BoundingSphereRadius;
					BoundingSphereCenter = Component->BoundingSphereCenter;
				}
				else {
					if (minBounds.x > Component->minBounds.x) minBounds.x = Component->minBounds.x;
					if (minBounds.y > Component->minBounds.y) minBounds.y = Component->minBounds.y;
					if (minBounds.z > Component->minBounds.z) minBounds.z = Component->minBounds.z;
					if (maxBounds.x < Component->maxBounds.x) maxBounds.x = Component->maxBounds.x;
					if (maxBounds.y < Component->maxBounds.y) maxBounds.y = Component->maxBounds.y;
					if (maxBounds.z < Component->maxBounds.z) maxBounds.z = Component->maxBounds.z;
					if (BoundingSphereRadius < Component->BoundingSphereRadius)
					{
						BoundingSphereRadius = Component->BoundingSphereRadius;
						BoundingSphereCenter = Component->BoundingSphereCenter;
					}
				}
                echo("SUCCESS: Component Added to GameObject");
            } else {
                echo("ERROR: Component Already in GameObject");
            }
        }
    }
    void GameObject::Remove(IComponent* Component)
    {
        bool found=false;
        for (std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
        {
            if ((*i)==Component) 
            {
                // Fire the Mothafucka!
                Component->Owner = NULL;

				// Unregister Component
				Component->Unregister(Scene);

                // Erase it!
                Components.erase(i);
                // Change Flag
                _ComponentsChanged = true;
                
                found = true;

				// Recheck Bounding
				minBounds = maxBounds = BoundingSphereCenter = Vec3();
				BoundingSphereRadius = 0;

				for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
				{
					IComponent* Component = (*i);
					if (BoundingSphereRadius < Component->BoundingSphereRadius)
					{
						BoundingSphereRadius = Component->BoundingSphereRadius;
						BoundingSphereCenter = Component->BoundingSphereCenter;
					}
					if (minBounds.x > Component->minBounds.x) minBounds.x = Component->minBounds.x;
					if (minBounds.y > Component->minBounds.y) minBounds.y = Component->minBounds.y;
					if (minBounds.z > Component->minBounds.z) minBounds.z = Component->minBounds.z;
					if (maxBounds.x < Component->maxBounds.x) maxBounds.x = Component->maxBounds.x;
					if (maxBounds.y < Component->maxBounds.y) maxBounds.y = Component->maxBounds.y;
					if (maxBounds.z < Component->maxBounds.z) maxBounds.z = Component->maxBounds.z;
				}

                break;
            }
        }
        if (found)
        {
            echo("SUCCESS: Component Removed from GameObject");
        } else {
            echo("ERROR: Component Not Found in GameObject");
        }
    }
    void GameObject::UpdateComponents()
    {
        for (std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
        {
            (*i)->Update();
        }
	}
    
    void GameObject::Add(GameObject* Child)
    {
        if (!Child->_HaveOwner)
        {
            Child->_Owner = this;
            Child->_HaveOwner = true;

            bool found = false;
            for (std::vector<GameObject*>::iterator i=_Childs.begin();i!=_Childs.end();i++)
            {
                if ((*i)==Child)
                {
                    found = true;
                    break;
                }
            }
            if (found) echo("ERROR: GameObject Already Added");
            else {
                _Childs.push_back(Child);
                echo("SUCCESS: GameObject added as a Child");
            }
        } else {
            echo("ERROR: GameObject Already have a Father");
        }
    }
    void GameObject::Remove(GameObject* Child)
    {
        if (Child->_HaveOwner)
        {
            Child->_Owner = NULL;
            Child->_HaveOwner = false;

            bool found = false;
            for (std::vector<GameObject*>::iterator i=_Childs.begin();i!=_Childs.end();i++)
            {
                if ((*i)==Child) 
                {
                    _Childs.erase(i);
                    found = true;
                    echo("SUCCESS: GameObject Removed as a Child");
                    break;
                }
            }
            if (!found) echo("ERROR: GameObject Not Found");
            else {
                _Childs.push_back(Child);
            }
        } else {
            echo("ERROR: GameObject Don't have a Father");
        }
    }
    
    void GameObject::RegisterComponents(SceneGraph* Scene)
    {
        if (_ComponentsChanged)
        {
            for(std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
            {
                (*i)->Register(Scene);
            }
            _ComponentsChanged = false;
        }
    }
    void GameObject::UnregisterComponents(SceneGraph* Scene)
    {
		_ComponentsChanged = true;
        for(std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
        {
            (*i)->Unregister(Scene);
        }
    }
  
	void GameObject::AddTag(const std::string &tag)
	{
		uint32 tagID = MakeStringID(tag);
		TagsList[tagID] = tag;
	}
	void GameObject::RemoveTag(const std::string &tag)
	{
		uint32 tagID = MakeStringID(tag);
		TagsList.erase(tagID);
	}
	bool GameObject::HaveTag(const std::string &tag)
	{
		return HaveTag(MakeStringID(tag));
	}
	bool GameObject::HaveTag(const uint32 tag)
	{
		return (TagsList.find(tag)!=TagsList.end());
	}

    // Helpers
    void GameObject::AddComponent(IComponent* Component) { Add(Component); }
    void GameObject::AddGameObject(GameObject* GO) { Add(GO); }
    void GameObject::RemoveComponent(IComponent* Component) { Remove(Component); }
    void GameObject::RemoveGameObject(GameObject* GO) { Remove(GO); }
    void GameObject::LookAtGameObject(GameObject* GO) { LookAt(GO); }
    void GameObject::LookAtVec(const Vec3 &center) { LookAt(center); }
};