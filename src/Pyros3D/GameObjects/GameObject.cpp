//============================================================================
// Name        : GameObject.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : GameObject
//============================================================================

#include "GameObject.h"
#include "../Ext/StringIDs/StringID.hpp"
namespace p3d {

    GameObject::GameObject() 
    {
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
    void GameObject::InternalUpdate()
    {
        // Update Transformation
        UpdateTransformation();
        
    }
    
    // Updates the Transformation Matrix
    void GameObject::UpdateTransformation(const uint32 &order)
    {
        if (_IsDirty)
        {
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

            // Save Rotation After LookAt
            _Rotation = _LocalMatrix.GetRotation(_Scale).GetEulerFromRotationMatrix();
            
        }
        _IsDirty = false;
        
        if (_HaveOwner)
        {
            _Owner->UpdateTransformation();
            _WorldMatrix = _Owner->_LocalMatrix * _LocalMatrix;
        } else _WorldMatrix = _LocalMatrix;
    }
    // Gets Transformation Matrix
    Matrix GameObject::GetWorldTransformation()
    {
        return _WorldMatrix;
    }
    Matrix GameObject::GetLocalTransformation()
    {
        return _LocalMatrix;
    }
    Vec3 GameObject::GetDirection()
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
    Vec3 GameObject::GetPosition()
    {
        return _Position;
    }
    // Gets Rotation
    Vec3 GameObject::GetRotation()
    {
        return _Rotation;
    }
    // Gets Scale
    Vec3 GameObject::GetScale()
    {
        return _Scale;
    }
    // Gets Position
    Vec3 GameObject::GetWorldPosition()
    {
        return _WorldMatrix.GetTranslation();
    }
    // Gets Rotation
    Vec3 GameObject::GetWorldRotation()
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
                // Erase it!
                Components.erase(i);
                // Change Flag
                _ComponentsChanged = true;
                
                found = true;
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
	bool GameObject::HaveTag(const uint32 &tag)
	{
		return (TagsList.find(tag)!=TagsList.end());
	}
};