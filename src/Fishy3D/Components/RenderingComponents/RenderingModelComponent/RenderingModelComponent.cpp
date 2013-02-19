//============================================================================
// Name        : RenderingModelComponent.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rendering Model Component
//============================================================================

#include "RenderingModelComponent.h"
#include "../../../Materials/GenericShaderMaterials/GenericShaderMaterial.h"
namespace Fishy3D {
    
    RenderingModelComponent::RenderingModelComponent(const std::string &Name, const std::string &ModelPath, SuperSmartPointer<IMaterial> material, bool SNormals, bool MaterialsFromFile) : IRenderingComponent(Name, material, SNormals)
    {
        // Load Model
        mesh = SuperSmartPointer<ModelLoader> (new ModelLoader());
        mesh->Load(ModelPath.c_str());
        
        // has submeshes
        haveSubComponents = true;
        
        // Build Materials From File Flag
        materialsFromFile = MaterialsFromFile;
        
        if (materialsFromFile)
        {
            // Build Materials
            for (unsigned i=0;i<mesh->materials.size();i++)
            {
                MaterialProperties mp = mesh->materials[i];
                unsigned options = 0;
                // Get Material Options
                if (mp.haveColor) options = options | ShaderUsage::Color;
                if (mp.haveSpecular) options = options | ShaderUsage::SpecularColor;
                if (mp.haveColorMap) options = options | ShaderUsage::Texture;
                if (mp.haveSpecularMap) options = options | ShaderUsage::SpecularMap;
                if (mp.haveNormalMap) options = options | ShaderUsage::BumpMapping;
                options = options | ShaderUsage::Diffuse;
                
                SuperSmartPointer<IMaterial> mat = SuperSmartPointer<IMaterial> (new GenericShaderMaterial(options));
                SuperSmartPointer<GenericShaderMaterial> genMat = mat;

                // Face Culling
                if (mp.Twosided) mat->SetCullFace(CullFace::DoubleSided);
				
                if (mp.haveColor) { genMat->SetColor(mp.Color); }
                if (mp.haveSpecular) { genMat->SetSpecular(mp.Specular); }
                if (mp.Opacity) { genMat->SetOpacity(mp.Opacity); }
                if (mp.haveColorMap) 
                {
                    Texture colorMap;
                    // set texture path relative to model file
                    std::string path = ModelPath.substr(0,ModelPath.find_last_of("/")+1);
                    colorMap.LoadTexture(path + mp.colorMap, TextureType::Texture, TextureSubType::NormalTexture);
                    colorMap.SetMinMagFilter(TextureFilter::LinearMipmapNearest,TextureFilter::LinearMipmapNearest);
                    genMat->SetColorMap(colorMap);
                }
                if (mp.haveSpecularMap) 
                {
                    Texture specularMap;
                    // set texture path relative to model file
                    std::string path = ModelPath.substr(0,ModelPath.find_last_of("/")+1);
                    specularMap.LoadTexture(path + mp.specularMap, TextureType::Texture, TextureSubType::NormalTexture);
                    specularMap.SetMinMagFilter(TextureFilter::LinearMipmapNearest,TextureFilter::LinearMipmapNearest);
                    genMat->SetSpecularMap(specularMap);
                }
                if (mp.haveNormalMap) 
                {
                    Texture normalMap;
                    // set texture path relative to model file
                    std::string path = ModelPath.substr(0,ModelPath.find_last_of("/")+1);
                    normalMap.LoadTexture(path + mp.normalMap, TextureType::Texture, TextureSubType::NormalTexture);
                    normalMap.SetMinMagFilter(TextureFilter::LinearMipmapNearest,TextureFilter::LinearMipmapNearest);
                    genMat->SetNormalMap(normalMap);
                }
                
                Materials.push_back(mat);
            }
        }

        for (unsigned i=0;i<mesh->subMeshes.size();i++)
        {
            std::ostringstream toStr; toStr << i;
            
            if (materialsFromFile)
            {
                material = Materials[mesh->subMeshes[i].materialID];
            }
            SuperSmartPointer<IComponent> SubMesh (new RenderingSubMeshComponent("SubMesh" + toStr.str(), material, SNormals));
            RenderingSubMeshComponent* c_submesh = static_cast<RenderingSubMeshComponent*>(SubMesh.Get());               

            c_submesh->IndexData.resize(mesh->subMeshes[i].tIndex.size());
            memcpy(&c_submesh->IndexData[0],&mesh->subMeshes[i].tIndex[0],mesh->subMeshes[i].tIndex.size()*sizeof(unsigned int));

            if (mesh->subMeshes[i].hasVertex==true)
            {
                c_submesh->tVertex.resize(mesh->subMeshes[i].tVertex.size());
                memcpy(&c_submesh->tVertex[0],&mesh->subMeshes[i].tVertex[0],mesh->subMeshes[i].tVertex.size()*sizeof(vec3));
            }
            if (mesh->subMeshes[i].hasNormal==true)
            {
                c_submesh->tNormal.resize(mesh->subMeshes[i].tNormal.size());
                memcpy(&c_submesh->tNormal[0],&mesh->subMeshes[i].tNormal[0],mesh->subMeshes[i].tNormal.size()*sizeof(vec3));
            }
            if (mesh->subMeshes[i].hasTexcoord==true)
            {
                c_submesh->tTexcoord.resize(mesh->subMeshes[i].tTexcoord.size());
                memcpy(&c_submesh->tTexcoord[0],&mesh->subMeshes[i].tTexcoord[0],mesh->subMeshes[i].tTexcoord.size()*sizeof(vec2));
            }
            if (mesh->subMeshes[i].hasTangentBitangent==true)
            {
                c_submesh->tTangent.resize(mesh->subMeshes[i].tTangent.size());
                memcpy(&c_submesh->tTangent[0],&mesh->subMeshes[i].tTangent[0],mesh->subMeshes[i].tTangent.size()*sizeof(vec3));

                c_submesh->tBitangent.resize(mesh->subMeshes[i].tBitangent.size());
                memcpy(&c_submesh->tBitangent[0],&mesh->subMeshes[i].tBitangent[0],mesh->subMeshes[i].tBitangent.size()*sizeof(vec3));
            }
            if (mesh->subMeshes[i].hasBones==true)
            {
                c_submesh->tBonesID.resize(mesh->subMeshes[i].tVertex.size());
                memcpy(&c_submesh->tBonesID[0],&mesh->subMeshes[i].tBonesID[0],mesh->subMeshes[i].tBonesID.size()*sizeof(vec4));
                
                c_submesh->tBonesWeight.resize(mesh->subMeshes[i].tVertex.size());
                memcpy(&c_submesh->tBonesWeight[0],&mesh->subMeshes[i].tBonesWeight[0],mesh->subMeshes[i].tBonesWeight.size()*sizeof(vec4));
                
                // Save SubMesh Map
                c_submesh->MapBoneIDs = mesh->subMeshes[i].MapBoneIDs;
                // Save SubMesh Bones Offset Matrix
                c_submesh->BoneOffsetMatrix = mesh->subMeshes[i].BoneOffsetMatrix;
                // Set Skinning Flag on
                c_submesh->haveBones = true;
            }
            
            if (HaveSmoothNormals() == true) c_submesh->SmoothNormals();
            
            c_submesh->Build();
            SubComponents.push_back(SubMesh);
        }
        
        // Save Skeleton
        skeleton = mesh->skeleton;
        
        // Calculate All Model's Boundaries
        CalculateBounding();
        
    }
    
    RenderingModelComponent::~RenderingModelComponent()
    {
        if (materialsFromFile)
        {
            for(unsigned i=0;i<Materials.size();i++)
            {
                Materials[i].Dispose();
            }
        }        
    }
    
    const std::map<StringID, Bone> &RenderingModelComponent::GetSkeleton() const
    {
        return skeleton;
    }
    
    void RenderingModelComponent::Build()
    {
        
        // Not Used
        
    }
    
    void RenderingModelComponent::CalculateBounding()
    {
       
        // Not Used
        
    }

    void RenderingModelComponent::ActivateCulling()
    {
        for (std::vector <SuperSmartPointer <IComponent> >::iterator i = SubComponents.begin(); i != SubComponents.end(); i++)
        {
            IRenderingComponent* rcomp = static_cast<IRenderingComponent*> ((*i).Get());
            rcomp->ActivateCulling();
        }
    }
    
    void RenderingModelComponent::DeactivateCulling()
    {
        for (std::vector <SuperSmartPointer <IComponent> >::iterator i = SubComponents.begin(); i != SubComponents.end(); i++)
        {
            IRenderingComponent* rcomp = static_cast<IRenderingComponent*> ((*i).Get());
            rcomp->DeactivateCulling();
        }
    }
    
    void RenderingModelComponent::Start() {}
    void RenderingModelComponent::Update() {}
    void RenderingModelComponent::Shutdown() {}
    void RenderingModelComponent::Register(void *ptr)
    {
        // register its sub components
        if (HaveSubComponents()==true)
        {
            for (std::vector<SuperSmartPointer <IComponent> >::iterator i=SubComponents.begin();i!=SubComponents.end();i++)
            {
                (*i)->Register(ptr);
            }
        }
    }
    void RenderingModelComponent::UnRegister(void *ptr)
    {
        // unregister its sub components
        if (HaveSubComponents()==true)
        {
            for (std::vector<SuperSmartPointer <IComponent> >::iterator i=SubComponents.begin();i!=SubComponents.end();i++)
            {                
                (*i)->UnRegister(ptr);
            }
        }
    }
        
    // SUBMESH
    RenderingSubMeshComponent::RenderingSubMeshComponent(const std::string& Name, SuperSmartPointer<IMaterial> material, bool SNormals) : IRenderingComponent(Name, material, SNormals) 
    {    
        IndexBuffer  =  SuperSmartPointer<GeometryBuffer> (new GeometryBuffer());    
    }
    
    void RenderingSubMeshComponent::Build()
    {
        
        CalculateBounding();
        
        SuperSmartPointer <AttributeBuffer> Vertex (new AttributeBuffer(Buffer::Type::Attribute,Buffer::Draw::Static));
        if (tVertex.size()>0) Vertex->AddAttribute("aPosition", Buffer::Attribute::Type::Vec3,&tVertex[0],tVertex.size());
        if (tNormal.size()>0) Vertex->AddAttribute("aNormal", Buffer::Attribute::Type::Vec3,&tNormal[0],tNormal.size());
        if (tTexcoord.size()>0) Vertex->AddAttribute("aTexcoord", Buffer::Attribute::Type::Vec2,&tTexcoord[0],tTexcoord.size());
        if (tTangent.size()>0) Vertex->AddAttribute("aTangent", Buffer::Attribute::Type::Vec3,&tTangent[0],tTangent.size());
        if (tBitangent.size()>0) Vertex->AddAttribute("aBitangent", Buffer::Attribute::Type::Vec3,&tBitangent[0],tBitangent.size());
        if (tBonesID.size()>0) Vertex->AddAttribute("aBonesID", Buffer::Attribute::Type::Vec4,&tBonesID[0],tBonesID.size());
        if (tBonesWeight.size()>0) Vertex->AddAttribute("aBonesWeight", Buffer::Attribute::Type::Vec4,&tBonesWeight[0],tBonesWeight.size());
        AddBuffer(Vertex);
        
        SendBuffers();            
    }
    void RenderingSubMeshComponent::CalculateBounding()
    {
        for (unsigned i=0;i<tVertex.size();i++)
        {
            if (i==0) {
                minBounds = tVertex[i];
                maxBounds = tVertex[i];
            } else {
                if (tVertex[i].x<minBounds.x) minBounds.x = tVertex[i].x;
                if (tVertex[i].y<minBounds.y) minBounds.y = tVertex[i].y;
                if (tVertex[i].z<minBounds.z) minBounds.z = tVertex[i].z;
                if (tVertex[i].x>maxBounds.x) maxBounds.x = tVertex[i].x;
                if (tVertex[i].y>maxBounds.y) maxBounds.y = tVertex[i].y;
                if (tVertex[i].z>maxBounds.z) maxBounds.z = tVertex[i].z;
            }
        }
        
        // Bounding Sphere
        boundSphereCenter = maxBounds-minBounds;
        float a = maxBounds.distance(boundSphereCenter);
        float b = minBounds.distance(boundSphereCenter);        
        boundSphereRadius = (a>b?a:b);
        
        // Debug
        #if _DEBUG
        // Bounding Box
        tPositionBoundingBox.push_back((vec3(minBounds.x,minBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,minBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,minBounds.y,minBounds.z)));
        tPositionBoundingBox.push_back((vec3(minBounds.x,minBounds.y,minBounds.z)));
        tPositionBoundingBox.push_back((vec3(minBounds.x,maxBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,maxBounds.y,maxBounds.z)));
        tPositionBoundingBox.push_back((vec3(maxBounds.x,maxBounds.y,minBounds.z)));
        tPositionBoundingBox.push_back((vec3(minBounds.x,maxBounds.y,minBounds.z)));
        tIndexBounding.push_back(0);
        tIndexBounding.push_back(1);
        tIndexBounding.push_back(1);
        tIndexBounding.push_back(2);
        tIndexBounding.push_back(2);
        tIndexBounding.push_back(3);
        tIndexBounding.push_back(3);
        tIndexBounding.push_back(0);
        tIndexBounding.push_back(0);
        tIndexBounding.push_back(4);
        tIndexBounding.push_back(4);
        tIndexBounding.push_back(5);
        tIndexBounding.push_back(5);
        tIndexBounding.push_back(6);
        tIndexBounding.push_back(6);
        tIndexBounding.push_back(7);
        tIndexBounding.push_back(7);
        tIndexBounding.push_back(4);
        tIndexBounding.push_back(7);
        tIndexBounding.push_back(3);
        tIndexBounding.push_back(6);
        tIndexBounding.push_back(2);
        tIndexBounding.push_back(5);
        tIndexBounding.push_back(1);
        
        // Normals
        unsigned count  = 0;
        float LineHeight = 1.f;
        for (int i=0;i+2<IndexData.size();i+=3)
        {
            float centroidX, centroidY, centroidZ;                                                        
            centroidX = (tVertex[IndexData[i]].x + tVertex[IndexData[i+1]].x + tVertex[IndexData[i+2]].x)/3;
            centroidY = (tVertex[IndexData[i]].y + tVertex[IndexData[i+1]].y + tVertex[IndexData[i+2]].y)/3;
            centroidZ = (tVertex[IndexData[i]].z + tVertex[IndexData[i+1]].z + tVertex[IndexData[i+2]].z)/3;

            tPositionNormals.push_back(vec3(centroidX,centroidY,centroidZ));
            tIndexNormals.push_back(count);
            count++;
            vec3 Normals = (tNormal[IndexData[i]] + tNormal[IndexData[i+1]] + tNormal[IndexData[i+2]])/3;
            tPositionNormals.push_back(vec3(centroidX,centroidY,centroidZ)+Normals.normalize()*LineHeight);
            tIndexNormals.push_back(count);
            count++;
        }
        #endif
    }
    
    void RenderingSubMeshComponent::SetSkinningBones(const std::vector<Matrix> &Bones)
    {
        SkinningBones.resize(Bones.size());
        memcpy(&SkinningBones[0],&Bones[0],Bones.size()*sizeof(Matrix));
    }
    
    void RenderingSubMeshComponent::SmoothNormals()
    {
        if (tNormal.size()>0)
        {
            std::vector<vec3> CopyNormals;
            for (unsigned i=0;i<tNormal.size();i++) {
                    CopyNormals.push_back(tNormal[i]);
            }
            for (unsigned i=0;i<tNormal.size();i++) {
                    for (unsigned j=0;j<tNormal.size();j++) {
                            if (i!=j && tVertex[i]==tVertex[j])
                                    tNormal[j]+=CopyNormals[i];            
                    }
            }
            for (unsigned i=0;i<tNormal.size();i++) {
                    tNormal[i].normalizeSelf();
            }
        }
    }
    
}