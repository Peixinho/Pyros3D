//============================================================================
// Name        : RenderingComponent
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Component For Rendering
//============================================================================

#include "RenderingComponent.h"

namespace p3d {
    
    // Initialize Rendering Components and Meshes vector
    std::vector<IComponent*> RenderingComponent::Components;
    std::map<SceneGraph*, std::vector<RenderingMesh*> > RenderingComponent::MeshesOnScene;
    
    RenderingComponent::RenderingComponent(const uint32 &AssetID, IMaterial* Material) 
    {
        // Keep Asset ID
        this->AssetID = AssetID;

        // Materials vector
        std::map <uint32, IMaterial*> Materialsvector;
        
        // Get Model from AssetManager
        Renderables::Renderable* renderable = static_cast<Renderables::Renderable*> (AssetManager::UseAsset(AssetID)->AssetPTR);
        for (uint32 i=0;i<renderable->Geometries.size();i++)
        {
            // Rendering Mesh Instance
            RenderingMesh* r_submesh  = new RenderingMesh();
            
            // Save Geometry Pointer
            r_submesh->Geometry = renderable->Geometries[i];
                        
            if (Material!=NULL) 
            {
                r_submesh->Material = Material;
                r_submesh->OwnMaterial = false;
            } else {
                if (Materialsvector.find(renderable->Geometries[i]->materialProperties.id)==Materialsvector.end())
                {
                    // From Properties
                    uint32 options = 0;
                    // Get Material Options
                    if (renderable->Geometries[i]->materialProperties.haveColor) options = options | ShaderUsage::Color;
                    if (renderable->Geometries[i]->materialProperties.haveSpecular) options = options | ShaderUsage::SpecularColor;
                    if (renderable->Geometries[i]->materialProperties.haveColorMap) options = options | ShaderUsage::Texture;
                    if (renderable->Geometries[i]->materialProperties.haveSpecularMap) options = options | ShaderUsage::SpecularMap;
                    if (renderable->Geometries[i]->materialProperties.haveNormalMap) options = options | ShaderUsage::BumpMapping;
//                    options = options | ShaderUsage::Diffuse | ShaderUsage::ShadowMaterial;

                    r_submesh->Material = new GenericShaderMaterial(options);
                    GenericShaderMaterial* genMat = static_cast<GenericShaderMaterial*> (r_submesh->Material);

                    // Material Properties
                    if (renderable->Geometries[i]->materialProperties.Twosided) genMat->SetCullFace(CullFace::DoubleSided);       
                    if (renderable->Geometries[i]->materialProperties.haveColor) { genMat->SetColor(renderable->Geometries[i]->materialProperties.Color); }
                    if (renderable->Geometries[i]->materialProperties.haveSpecular) { genMat->SetSpecular(renderable->Geometries[i]->materialProperties.Specular); }
                    if (renderable->Geometries[i]->materialProperties.Opacity) { genMat->SetOpacity(renderable->Geometries[i]->materialProperties.Opacity); }
                    if (renderable->Geometries[i]->materialProperties.haveColorMap) 
                    {
                        Texture colorMap;
                        colorMap.LoadTexture(renderable->Geometries[i]->materialProperties.colorMap, TextureType::Texture);
                        colorMap.SetMinMagFilter(TextureFilter::LinearMipmapNearest,TextureFilter::LinearMipmapNearest);
                        genMat->SetColorMap(colorMap);
                    }
                    if (renderable->Geometries[i]->materialProperties.haveSpecularMap) 
                    {
                        Texture specularMap;
                        specularMap.LoadTexture(renderable->Geometries[i]->materialProperties.specularMap, TextureType::Texture);
                        specularMap.SetMinMagFilter(TextureFilter::LinearMipmapNearest,TextureFilter::LinearMipmapNearest);
                        genMat->SetSpecularMap(specularMap);
                    }
                    if (renderable->Geometries[i]->materialProperties.haveNormalMap) 
                    {
                        Texture normalMap;
                        normalMap.LoadTexture(renderable->Geometries[i]->materialProperties.normalMap, TextureType::Texture);
                        normalMap.SetMinMagFilter(TextureFilter::LinearMipmapNearest,TextureFilter::LinearMipmapNearest);
                        genMat->SetNormalMap(normalMap);
                    }
                    Materialsvector[renderable->Geometries[i]->materialProperties.id] = genMat;
                }
                else {
                    r_submesh->Material = Materialsvector[renderable->Geometries[i]->materialProperties.id];
                    r_submesh->OwnMaterial = true;
                }
            }
            
            // Own this Mothafuckah!
            r_submesh->renderingComponent = this;
            
            // Push Mesh
            Meshes.push_back(r_submesh);
        }
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
    
    std::vector<IComponent*> RenderingComponent::GetComponents()
    {
        return Components;
    }
    
    std::vector<RenderingMesh*> RenderingComponent::GetRenderingMeshes(SceneGraph* scene)
    {
        return MeshesOnScene[scene];
    }
    
    std::vector<RenderingMesh*> RenderingComponent::GetMeshes()
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
    
    void RenderingComponent::Destroy()
    {
//        for (std::vector<RenderingMesh*>::iterator i=Meshes.begin();i!=Meshes.end();i++)
//        {
//            for (std::vector<RenderingMesh*>::iterator k=RenderingMeshes.begin();k!=RenderingMeshes.end();k++)
//            {
//                if ((*k)==(*i)) 
//                {
//                    RenderingMeshes.erase(k);
//                }
//            }
//            // Delete Mesh
//            delete (*i);
//        }
        // Clear Meshes List
        Meshes.clear();
    }
    
};