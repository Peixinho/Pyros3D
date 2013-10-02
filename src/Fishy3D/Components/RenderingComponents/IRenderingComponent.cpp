//============================================================================
// Name        : IRenderingComponents.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Components Interface
//============================================================================


#include "IRenderingComponent.h"

namespace Fishy3D {

    IRenderingComponent::IRenderingComponent() {}
    IRenderingComponent::IRenderingComponent(const std::string& Name, SuperSmartPointer<IMaterial> Material, bool SNormals) : IComponent(Name), Active(true)
    {
        
        // smooth normals
        smoothNormals = SNormals;
        
        // triangles by default
        drawType = DrawingType::Triangles;       
        
        // Material
        material = Material;
        
        // SubComponents
        haveSubComponents = false;
        
        // Culling
        _culling = true;
        
        // Cast Shadows
        CastShadows = false;
    
        // Skinning
        haveBones = false;
        
    }
    IRenderingComponent::~IRenderingComponent() 
    {
        // Destroy all SubComponents
        if (haveSubComponents==true) 
        {
            for (unsigned int i=0;i<SubComponents.size();i++)
            {
                SubComponents[i].Dispose();
            }
        } else {
            // Destroy Index Buffer
            if (IndexBuffer.Get()!=NULL) 
            {
                IndexBuffer.Dispose();
            }

            // Attributes Buffer
            if (Buffers.size()>0)
                for (std::vector<SuperSmartPointer <AttributeBuffer> >::iterator i=Buffers.begin();i!=Buffers.end();i++)
                {
                    for (std::list<SuperSmartPointer <VertexAttribute> >::iterator k=(*i)->Attributes.begin();k!=(*i)->Attributes.end();k++)
                    {
                        (*k).Dispose();
                    }
                    // destroy buffer
                    (*i)->Buffer.Dispose();

                }

            // Clears Attributes Map
            Buffers.clear();
            

            #if _DEBUG

            if (IndexBoundingBuffer.Get()!=NULL)
            {
                IndexBoundingBuffer.Dispose();
                BoundingBuffer->Buffer.Dispose();
            }
            if (IndexNormalsBuffer.Get()!=NULL)
            {
                IndexNormalsBuffer.Dispose();
                NormalsBuffer->Buffer.Dispose();
            }    
            
            #endif
            
        }
    }
    
    void IRenderingComponent::StartCastingShadows()
    {
        CastShadows = true;
    }
    
    void IRenderingComponent::StopCastingShadows()
    {
        CastShadows = false;
    }
    
    bool IRenderingComponent::IsCastingShadows()
    {
        return CastShadows;
    }    
    
    void IRenderingComponent::AddBuffer(SuperSmartPointer<AttributeBuffer> Buffer)
    {
        Buffers.push_back(Buffer);
    }
    
    void IRenderingComponent::SetMaterial(SuperSmartPointer<IMaterial> material)
    {
   
        // Reset Attribute IDs
        this->material = material;        
        for (std::vector< SuperSmartPointer <AttributeBuffer> >::iterator i=Buffers.begin();i!=Buffers.end();i++)
        {
        for (std::list<SuperSmartPointer <VertexAttribute> >::iterator k=(*i)->Attributes.begin();k!=(*i)->Attributes.end();k++)
                (*k)->ID = -2;
        }
        for (std::list<Uniform::Uniform>::iterator k = material->GlobalUniforms.begin();k!=material->GlobalUniforms.end();k++) 
        {
            (*k).Handle = -2;
        }
        for (std::list<Uniform::Uniform>::iterator k = material->ModelUniforms.begin();k!=material->ModelUniforms.end();k++) 
        {
            (*k).Handle = -2;
        }
        for (std::map<StringID,Uniform::Uniform>::iterator k = material->UserUniforms.begin();k!=material->UserUniforms.end();k++) 
        {
            (*k).second.Handle = -2;
        }  
        
    }
    IMaterial* IRenderingComponent::GetMaterial()
    {
        return material.Get();
    }
    
    void IRenderingComponent::SendBuffers()
    {       
        
        // send index buffer
        IndexBuffer = SuperSmartPointer<GeometryBuffer>(new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static));
        IndexBuffer->Init( &IndexData[0], sizeof(unsigned int)*IndexData.size());

        // create and send vertex buffers
        for (std::vector<SuperSmartPointer <AttributeBuffer> >::iterator i=Buffers.begin();i!=Buffers.end();i++)
        {
            (*i)->SendBuffer();                              
        }
        
        // creates and sends bounding box and normals
        #if _DEBUG
        SendDebugVBOS();
        #endif

    }
    void IRenderingComponent::SendDebugVBOS()
    {
        // DEBUG
        #if _DEBUG
        if (tIndexBounding.size()>0) 
        {
            IndexBoundingBuffer = SuperSmartPointer<GeometryBuffer>(new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static));
            IndexBoundingBuffer->Init( &tIndexBounding[0], sizeof(unsigned int)*tIndexBounding.size());
            BoundingBuffer = SuperSmartPointer <AttributeBuffer> (new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static));
            BoundingBuffer->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tPositionBoundingBox[0],tPositionBoundingBox.size());
            BoundingBuffer->SendBuffer();
        }
        if (tIndexNormals.size()>0)
        {
            IndexNormalsBuffer= SuperSmartPointer<GeometryBuffer>(new GeometryBuffer(Buffer::Type::Index, Buffer::Draw::Static));
            IndexNormalsBuffer->Init( &tIndexNormals[0], sizeof(unsigned int)*tIndexNormals.size());
            NormalsBuffer = SuperSmartPointer <AttributeBuffer> (new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static));
            NormalsBuffer->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tPositionNormals[0],tPositionNormals.size());
            NormalsBuffer->SendBuffer();
        }
        #endif
        
    }
    void IRenderingComponent::Register(void* ptr)
    {
        SceneGraph* scene = static_cast<SceneGraph*> (ptr);
        scene->GetRenderingList()->AddRenderingComponent(this);
    }
    void IRenderingComponent::UnRegister(void* ptr)
    {
        SceneGraph* scene = static_cast<SceneGraph*> (ptr);
        scene->GetRenderingList()->RemoveRenderingComponent(this);
    }
    
    bool IRenderingComponent::HaveSmoothNormals() { return smoothNormals; }
    void IRenderingComponent::SetDrawingType(const unsigned int &type) { drawType = type; }
    unsigned int IRenderingComponent::GetDrawingType() { return drawType; }    
    const vec3 &IRenderingComponent::GetBoundingMin() const { return minBounds; }    
    const vec3 &IRenderingComponent::GetBoundingMax() const {  return maxBounds; }
    const vec3 &IRenderingComponent::GetBoundingSphereCenter() const {  return boundSphereCenter; }
    const float &IRenderingComponent::GetBoundingSphereRadius() const {  return boundSphereRadius; }
    
    void IRenderingComponent::Activate()
    {
        Active = true;
        // Activate all SubComponents
        if (HaveSubComponents()==true)
        {
            for (std::vector <SuperSmartPointer<IComponent> >::iterator i = GetSubComponents().begin();i!=GetSubComponents().end();i++)
            {
                IRenderingComponent* rc = (IRenderingComponent*)(*i).Get();
                rc->Activate();
            }
        }
    }
    void IRenderingComponent::Deactivate()
    {
        Active = false;
        // Deactivate all SubComponents
        if (HaveSubComponents()==true)
        {
            for (std::vector <SuperSmartPointer<IComponent> >::iterator i = GetSubComponents().begin();i!=GetSubComponents().end();i++)
            {
                IRenderingComponent* rc = (IRenderingComponent*)(*i).Get();
                rc->Deactivate();
            }
        }
    }
    bool IRenderingComponent::IsActive()
    {
        return Active;
    }
    
    void IRenderingComponent::DeactivateCulling()
    {
        _culling = false;
    }
    void IRenderingComponent::ActivateCulling()
    {
        _culling = true;
    }
    bool IRenderingComponent::IsCullingActive()
    {
        return _culling;
    }
    bool IRenderingComponent::HaveBones()
    {
        return haveBones;
    }
}