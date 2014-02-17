//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#include "RenderingComponent.h"
#ifdef ANDROID
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #include "GL/glew.h"
#endif
#define GLCHECK() { int32 error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace p3d {
    
    // Initialize Rendering Components and Meshes vector
    std::vector<IComponent*> RenderingComponent::Components;
    std::map<SceneGraph*, std::vector<RenderingMesh*> > RenderingComponent::MeshesOnScene;
    
    RenderingComponent::RenderingComponent(Renderable* renderable, IMaterial* Material)
    {
        // By Default Is Casting Shadows
        isCastingShadows = true;

        for (uint32 i=0;i<renderable->Geometries.size();i++)
        {
            // Rendering Mesh Instance
            RenderingMesh* r_submesh  = new RenderingMesh();
            
            // Save Geometry Pointer
            r_submesh->Geometry = renderable->Geometries[i];
            // Get Geometry Specific Stuff
            r_submesh->Material = renderable->Geometries[i]->Material;
            if (renderable->Geometries[i]->materialProperties.haveBones)
            {
                r_submesh->MapBoneIDs = renderable->Geometries[i]->MapBoneIDs;
                r_submesh->BoneOffsetMatrix = renderable->Geometries[i]->BoneOffsetMatrix;
            }
            if (Material!=NULL) 
            {
                r_submesh->Material = Material;
            }

            // Own this Mothafuckah!
            r_submesh->renderingComponent = this;
            
            // Push Mesh
            Meshes.push_back(r_submesh);
        }

        // Keep Skeleton
        skeleton = renderable->GetSkeleton();
        hasBones = (skeleton.size()>0?true:false);
    }
    
    void RenderingComponent::Register(SceneGraph* Scene)
    {
        if (!Registered)
        {
            // Add Self to Components vector
            Components.push_back(this);

            // Add Meshes to Rendering Meshes
            for (std::vector<RenderingMesh*>::iterator i=Meshes.begin();i!=Meshes.end();i++)
            {
                // Add Mesh
                MeshesOnScene[Scene].push_back((*i));
            }
            
            Registered = true;
        }
    }
    void RenderingComponent::Unregister(SceneGraph* Scene)
    {
        if (Registered)
        {
            // Remove from Components vector
            for (std::vector<IComponent*>::iterator i=Components.begin();i!=Components.end();i++)
            {
                if ((*i)==this)
                {
                    Components.erase(i);
                    break;
                }
            }
            // Remove from Meshes vector
            for (std::vector<RenderingMesh*>::iterator i=Meshes.begin();i!=Meshes.end();i++)
            {
                for (std::vector<RenderingMesh*>::iterator k=MeshesOnScene[Scene].begin();k!=MeshesOnScene[Scene].end();k++)
                {
                    if ((*k)==(*i))
                    {
                        MeshesOnScene[Scene].erase(k);
                        break;
                    }
                }
            }
            
            Registered = false;
            
        }
    }
    
    std::vector<IComponent*> &RenderingComponent::GetComponents()
    {
        return Components;
    }
    
    std::vector<RenderingMesh*> &RenderingComponent::GetRenderingMeshes(SceneGraph* scene)
    {
        return MeshesOnScene[scene];
    }
    
    std::vector<RenderingMesh*> &RenderingComponent::GetMeshes()
    {
        return Meshes;
    }
    
    void RenderingComponent::SetCullingGeometry(const uint32& Geometry)
    {
        // Set Culling Geometry to all  Meshes
        CullingGeometry = Geometry;
        for (std::vector<RenderingMesh*>::iterator i=Meshes.begin();i!=Meshes.end();i++)
        {
            (*i)->CullingGeometry = Geometry;
        }
    }
    
    void RenderingComponent::EnableCastShadows()
    {
        isCastingShadows = true;
    }
    void RenderingComponent::DisableCastShadows()
    {
        isCastingShadows = false;
    }
    bool RenderingComponent::IsCastingShadows()
    {
        return isCastingShadows;
    }
    RenderingComponent::~RenderingComponent()
    {
        for (std::vector<RenderingMesh*>::iterator i=Meshes.begin();i!=Meshes.end();i++)
        {
            // Delete Mesh
            delete (*i);
        }
        // Clear Meshes List
        Meshes.clear();
    }
};
